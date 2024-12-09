#pragma once

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <type_traits>

#include "concurrent_skiplist_node.h"
#include "iterators.h"
#include "memory.h"
#include "microspinlock.h"

namespace utility {
namespace skiplist {

// 修改兼容Windows编译
#if defined(_MSC_VER)  // MSVC
    // MSVC 不支持 __builtin_expect，提供替代实现
    #define FOLLY_BUILTIN_EXPECT(exp, c) static_cast<bool>((exp) ? (c) : (!c))
#else
    // 其他编译器如 GCC 和 Clang 支持 __builtin_expect
    #define FOLLY_BUILTIN_EXPECT(exp, c) __builtin_expect(static_cast<bool>(exp), c)
#endif

// 定义 FOLLY_UNLIKELY 宏
#define FOLLY_UNLIKELY(...) FOLLY_BUILTIN_EXPECT((__VA_ARGS__), 0)


template <
    typename T,
    typename Comparator = std::less<T>,
    // All nodes are allocated using provided SysAllocator,
    // it should be thread-safe.
    typename NodeAlloc = SysAllocator<char>,
    int MAX_HEIGHT = 24>
class ConcurrentSkipList {
  // MAX_HEIGHT needs to be at least 2 to suppress compiler
  // warnings/errors (Werror=uninitialized triggered due to preds_[1]
  // being treated as a scalar in the compiler).
  static_assert(
      MAX_HEIGHT >= 2 && MAX_HEIGHT < 64,
      "MAX_HEIGHT can only be in the range of [2, 64)");
  typedef std::unique_lock<MicroSpinLock> ScopedLocker;
  typedef ConcurrentSkipList<T, Comparator, NodeAlloc, MAX_HEIGHT> SkipListType;

 public:
  typedef detail::SkipListNode<T> NodeType;
  typedef T value_type;
  typedef T key_type;

  typedef detail::csl_iterator<value_type, NodeType> iterator;
  typedef detail::csl_iterator<const value_type, NodeType> const_iterator;

  class Accessor;
  class Skipper;

  // explicit ConcurrentSkipList(int height, const NodeAlloc& alloc)
  //     : recycler_(alloc),
  //       head_(NodeType::create(recycler_.alloc(), height, value_type(), true)) {
  // }

  explicit ConcurrentSkipList(int height)
      : recycler_(),
        head_(NodeType::create(recycler_.alloc(), height, value_type(), true)),
        cmp_(std::less<T>()) {
  }

  explicit ConcurrentSkipList(int height, const Comparator& cmp)
      : recycler_(),
        head_(NodeType::create(recycler_.alloc(), height, value_type(), true)),
        cmp_(cmp) {
  }


  // Convenience function to get an Accessor to a new instance.
  // static Accessor create(int height, const NodeAlloc& alloc) {
  //   return Accessor(createInstance(height, alloc));
  // }

  static Accessor create(int height = 1) {
    return Accessor(createInstance(height));
  }

  static Accessor create(int height, const Comparator& cmp) {
    return Accessor(createInstance(height, cmp));
  }

  // Create a shared_ptr skiplist object with initial head height.
  // static std::shared_ptr<SkipListType> createInstance(
  //     int height, const NodeAlloc& alloc) {
  //   return std::make_shared<ConcurrentSkipList>(height, alloc);
  // }

  static std::shared_ptr<SkipListType> createInstance(int height = 1) {
    return std::make_shared<ConcurrentSkipList>(height);
  }

  static std::shared_ptr<SkipListType> createInstance(int height, const Comparator& cmp) {
    return std::make_shared<ConcurrentSkipList>(height, cmp);
  }

  size_t size() const { return size_.load(std::memory_order_relaxed); }
  bool empty() const { return size() == 0; }

  //===================================================================
  // Below are implementation details.
  // Please see ConcurrentSkipList::Accessor for stdlib-like APIs.
  //===================================================================

  ~ConcurrentSkipList() {
    if /* constexpr */ (NodeType::template DestroyIsNoOp<NodeAlloc>::value) {
      // Avoid traversing the list if using arena allocator.
      return;
    }
    for (NodeType* current = head_.load(std::memory_order_relaxed); current;) {
      NodeType* tmp = current->skip(0);
      NodeType::destroy(recycler_.alloc(), current);
      current = tmp;
    }
  }

 private:
  bool greater(const value_type& data, const NodeType* node) const {
    return node && cmp_(node->data(), data);
  }

  bool less(const value_type& data, const NodeType* node) const {
    return (node == nullptr) || cmp_(data, node->data());
  }

  int findInsertionPoint(
      NodeType* cur,
      int cur_layer,
      const value_type& data,
      NodeType* preds[],
      NodeType* succs[]) const {
    int foundLayer = -1;
    NodeType* pred = cur;
    NodeType* foundNode = nullptr;
    for (int layer = cur_layer; layer >= 0; --layer) {
      NodeType* node = pred->skip(layer);
      while (this->greater(data, node)) {
        pred = node;
        node = node->skip(layer);
      }
      if (foundLayer == -1 && !this->less(data, node)) { // the two keys equal
        foundLayer = layer;
        foundNode = node;
      }
      preds[layer] = pred;

      // if found, succs[0..foundLayer] need to point to the cached foundNode,
      // as foundNode might be deleted at the same time thus pred->skip() can
      // return nullptr or another node.
      succs[layer] = foundNode ? foundNode : node;
    }
    return foundLayer;
  }

  int height() const { return head_.load(std::memory_order_acquire)->height(); }

  int maxLayer() const { return height() - 1; }

  size_t incrementSize(int delta) {
    return size_.fetch_add(delta, std::memory_order_relaxed) + delta;
  }

  // Returns the node if found, nullptr otherwise.
  NodeType* find(const value_type& data) {
    auto ret = findNode(data);
    if (ret.second && !ret.first->markedForRemoval()) {
      return ret.first;
    }
    return nullptr;
  }

  // lock all the necessary nodes for changing (adding or removing) the list.
  // returns true if all the lock acquired successfully and the related nodes
  // are all validate (not in certain pending states), false otherwise.
  bool lockNodesForChange(
      int nodeHeight,
      ScopedLocker guards[MAX_HEIGHT],
      NodeType* preds[MAX_HEIGHT],
      NodeType* succs[MAX_HEIGHT],
      bool adding = true) {
    NodeType *pred, *succ, *prevPred = nullptr;
    bool valid = true;
    for (int layer = 0; valid && layer < nodeHeight; ++layer) {
      pred = preds[layer];
      // DCHECK(pred != nullptr) << "layer=" << layer << " height=" << height()
      //                         << " nodeheight=" << nodeHeight;
      succ = succs[layer];
      if (pred != prevPred) {
        guards[layer] = pred->acquireGuard();
        prevPred = pred;
      }
      valid = !pred->markedForRemoval() &&
          pred->skip(layer) == succ; // check again after locking

      if (adding) { // when adding a node, the succ shouldn't be going away
        valid = valid && (succ == nullptr || !succ->markedForRemoval());
      }
    }

    return valid;
  }

  // Returns a paired value:
  //   pair.first always stores the pointer to the node with the same input key.
  //     It could be either the newly added data, or the existed data in the
  //     list with the same key.
  //   pair.second stores whether the data is added successfully:
  //     0 means not added, otherwise returns the new size.
  template <typename U>
  std::pair<NodeType*, size_t> addOrGetData(U&& data) {
    NodeType *preds[MAX_HEIGHT], *succs[MAX_HEIGHT];
    NodeType* newNode;
    size_t newSize;
    while (true) {
      int max_layer = 0;
      int layer = findInsertionPointGetMaxLayer(data, preds, succs, &max_layer);

      if (layer >= 0) {
        NodeType* nodeFound = succs[layer];
        // DCHECK(nodeFound != nullptr);
        if (nodeFound->markedForRemoval()) {
          continue; // if it's getting deleted retry finding node.
        }
        // wait until fully linked.
        while (FOLLY_UNLIKELY(!nodeFound->fullyLinked())) {
        }
        return std::make_pair(nodeFound, 0);
      }

      // need to capped at the original height -- the real height may have grown
      int nodeHeight =
          detail::SkipListRandomHeight::instance()->getHeight(max_layer + 1);

      ScopedLocker guards[MAX_HEIGHT];
      if (!lockNodesForChange(nodeHeight, guards, preds, succs)) {
        continue; // give up the locks and retry until all valid
      }

      // locks acquired and all valid, need to modify the links under the locks.
      newNode = NodeType::create(
          recycler_.alloc(), nodeHeight, std::forward<U>(data));
      for (int k = 0; k < nodeHeight; ++k) {
        newNode->setSkip(k, succs[k]);
        preds[k]->setSkip(k, newNode);
      }

      newNode->setFullyLinked();
      newSize = incrementSize(1);
      break;
    }

    int hgt = height();
    size_t sizeLimit =
        detail::SkipListRandomHeight::instance()->getSizeLimit(hgt);

    if (hgt < MAX_HEIGHT && newSize > sizeLimit) {
      growHeight(hgt + 1);
    }
    // CHECK_GT(newSize, 0);
    return std::make_pair(newNode, newSize);
  }

  bool remove(const value_type& data) {
    NodeType* nodeToDelete = nullptr;
    ScopedLocker nodeGuard;
    bool isMarked = false;
    int nodeHeight = 0;
    NodeType *preds[MAX_HEIGHT], *succs[MAX_HEIGHT];

    while (true) {
      int max_layer = 0;
      int layer = findInsertionPointGetMaxLayer(data, preds, succs, &max_layer);
      if (!isMarked && (layer < 0 || !okToDelete(succs[layer], layer))) {
        return false;
      }

      if (!isMarked) {
        nodeToDelete = succs[layer];
        nodeHeight = nodeToDelete->height();
        nodeGuard = nodeToDelete->acquireGuard();
        if (nodeToDelete->markedForRemoval()) {
          return false;
        }
        nodeToDelete->setMarkedForRemoval();
        isMarked = true;
      }

      // acquire pred locks from bottom layer up
      ScopedLocker guards[MAX_HEIGHT];
      if (!lockNodesForChange(nodeHeight, guards, preds, succs, false)) {
        continue; // this will unlock all the locks
      }

      for (int k = nodeHeight - 1; k >= 0; --k) {
        preds[k]->setSkip(k, nodeToDelete->skip(k));
      }

      incrementSize(-1);
      break;
    }
    recycle(nodeToDelete);
    return true;
  }

  const value_type* first() const {
    auto node = head_.load(std::memory_order_acquire)->skip(0);
    return node ? &node->data() : nullptr;
  }

  const value_type* last() const {
    NodeType* pred = head_.load(std::memory_order_acquire);
    NodeType* node = nullptr;
    for (int layer = maxLayer(); layer >= 0; --layer) {
      do {
        node = pred->skip(layer);
        if (node) {
          pred = node;
        }
      } while (node != nullptr);
    }
    return pred == head_.load(std::memory_order_relaxed) ? nullptr
                                                         : &pred->data();
  }

  static bool okToDelete(NodeType* candidate, int layer) {
    // DCHECK(candidate != nullptr);
    return candidate->fullyLinked() && candidate->maxLayer() == layer &&
        !candidate->markedForRemoval();
  }

  // find node for insertion/deleting
  int findInsertionPointGetMaxLayer(
      const value_type& data,
      NodeType* preds[],
      NodeType* succs[],
      int* max_layer) const {
    *max_layer = maxLayer();
    return this->findInsertionPoint(
        head_.load(std::memory_order_acquire), *max_layer, data, preds, succs);
  }

  // Find node for access. Returns a paired values:
  // pair.first = the first node that no-less than data value
  // pair.second = 1 when the data value is founded, or 0 otherwise.
  // This is like lower_bound, but not exact: we could have the node marked for
  // removal so still need to check that.
  std::pair<NodeType*, int> findNode(const value_type& data) const {
    return findNodeDownRight(data);
  }

  // Find node by first stepping down then stepping right. Based on benchmark
  // results, this is slightly faster than findNodeRightDown for better
  // locality on the skipping pointers.
  std::pair<NodeType*, int> findNodeDownRight(const value_type& data) const {
    NodeType* pred = head_.load(std::memory_order_acquire);
    int ht = pred->height();
    NodeType* node = nullptr;

    bool found = false;
    while (!found) {
      // stepping down
      for (; ht > 0 && this->less(data, node = pred->skip(ht - 1)); --ht) {
      }
      if (ht == 0) {
        return std::make_pair(node, 0); // not found
      }
      // node <= data now, but we need to fix up ht
      --ht;

      // stepping right
      while (this->greater(data, node)) {
        pred = node;
        node = node->skip(ht);
      }
      found = !this->less(data, node);
    }
    return std::make_pair(node, found);
  }

  // find node by first stepping right then stepping down.
  // We still keep this for reference purposes.
  std::pair<NodeType*, int> findNodeRightDown(const value_type& data) const {
    NodeType* pred = head_.load(std::memory_order_acquire);
    NodeType* node = nullptr;
    auto top = maxLayer();
    int found = 0;
    for (int layer = top; !found && layer >= 0; --layer) {
      node = pred->skip(layer);
      while (this->greater(data, node)) {
        pred = node;
        node = node->skip(layer);
      }
      found = !this->less(data, node);
    }
    return std::make_pair(node, found);
  }

  NodeType* lower_bound(const value_type& data) const {
    auto node = findNode(data).first;
    while (node != nullptr && node->markedForRemoval()) {
      node = node->skip(0);
    }
    return node;
  }

  void growHeight(int height) {
    NodeType* oldHead = head_.load(std::memory_order_acquire);
    if (oldHead->height() >= height) { // someone else already did this
      return;
    }

    NodeType* newHead =
        NodeType::create(recycler_.alloc(), height, value_type(), true);

    { // need to guard the head node in case others are adding/removing
      // nodes linked to the head.
      ScopedLocker g = oldHead->acquireGuard();
      newHead->copyHead(oldHead);
      NodeType* expected = oldHead;
      if (!head_.compare_exchange_strong(
              expected, newHead, std::memory_order_release)) {
        // if someone has already done the swap, just return.
        NodeType::destroy(recycler_.alloc(), newHead);
        return;
      }
      oldHead->setMarkedForRemoval();
    }
    recycle(oldHead);
  }

  void recycle(NodeType* node) { recycler_.add(node); }

  detail::NodeRecycler<NodeType, NodeAlloc> recycler_;
  std::atomic<NodeType*> head_;
  std::atomic<size_t> size_{0};

  Comparator const cmp_;
};

template <typename T, typename Comparator, typename NodeAlloc, int MAX_HEIGHT>
class ConcurrentSkipList<T, Comparator, NodeAlloc, MAX_HEIGHT>::Accessor {
  typedef detail::SkipListNode<T> NodeType;
  typedef ConcurrentSkipList<T, Comparator, NodeAlloc, MAX_HEIGHT> SkipListType;

 public:
  typedef T value_type;
  typedef T key_type;
  typedef T& reference;
  typedef T* pointer;
  typedef const T& const_reference;
  typedef const T* const_pointer;
  typedef size_t size_type;
  typedef Comparator key_compare;
  typedef Comparator value_compare;

  typedef typename SkipListType::iterator iterator;
  typedef typename SkipListType::const_iterator const_iterator;
  typedef typename SkipListType::Skipper Skipper;

  explicit Accessor(std::shared_ptr<ConcurrentSkipList> skip_list)
      : slHolder_(std::move(skip_list)) {
    sl_ = slHolder_.get();
    // DCHECK(sl_ != nullptr);
    sl_->recycler_.addRef();
  }

  // Unsafe initializer: the caller assumes the responsibility to keep
  // skip_list valid during the whole life cycle of the Accessor.
  explicit Accessor(ConcurrentSkipList* skip_list) : sl_(skip_list) {
    // DCHECK(sl_ != nullptr);
    sl_->recycler_.addRef();
  }

  Accessor(const Accessor& accessor)
      : sl_(accessor.sl_), slHolder_(accessor.slHolder_) {
    sl_->recycler_.addRef();
  }

  Accessor& operator=(const Accessor& accessor) {
    if (this != &accessor) {
      slHolder_ = accessor.slHolder_;
      sl_->recycler_.releaseRef();
      sl_ = accessor.sl_;
      sl_->recycler_.addRef();
    }
    return *this;
  }

  ~Accessor() { sl_->recycler_.releaseRef(); }

  bool empty() const { return sl_->size() == 0; }
  size_t size() const { return sl_->size(); }
  size_type max_size() const { return std::numeric_limits<size_type>::max(); }

  // returns end() if the value is not in the list, otherwise returns an
  // iterator pointing to the data, and it's guaranteed that the data is valid
  // as far as the Accessor is hold.
  iterator find(const key_type& value) { return iterator(sl_->find(value)); }
  const_iterator find(const key_type& value) const {
    return iterator(sl_->find(value));
  }
  size_type count(const key_type& data) const { return contains(data); }

  iterator begin() const {
    NodeType* head = sl_->head_.load(std::memory_order_acquire);
    return iterator(head->next());
  }
  iterator end() const { return iterator(nullptr); }
  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }

  template <
      typename U,
      typename =
          typename std::enable_if<std::is_convertible<U, T>::value>::type>
  std::pair<iterator, bool> insert(U&& data) {
    auto ret = sl_->addOrGetData(std::forward<U>(data));
    return std::make_pair(iterator(ret.first), ret.second);
  }
  size_t erase(const key_type& data) { return remove(data); }

  iterator lower_bound(const key_type& data) const {
    return iterator(sl_->lower_bound(data));
  }

  size_t height() const { return sl_->height(); }

  // first() returns pointer to the first element in the skiplist, or
  // nullptr if empty.
  //
  // last() returns the pointer to the last element in the skiplist,
  // nullptr if list is empty.
  //
  // Note: As concurrent writing can happen, first() is not
  //   guaranteed to be the min_element() in the list. Similarly
  //   last() is not guaranteed to be the max_element(), and both of them can
  //   be invalid (i.e. nullptr), so we name them differently from front() and
  //   tail() here.
  const key_type* first() const { return sl_->first(); }
  const key_type* last() const { return sl_->last(); }

  // Try to remove the last element in the skip list.
  //
  // Returns true if we removed it, false if either the list is empty
  // or a race condition happened (i.e. the used-to-be last element
  // was already removed by another thread).
  bool pop_back() {
    auto last = sl_->last();
    return last ? sl_->remove(*last) : false;
  }

  std::pair<key_type*, bool> addOrGetData(const key_type& data) {
    auto ret = sl_->addOrGetData(data);
    return std::make_pair(&ret.first->data(), ret.second);
  }

  SkipListType* skiplist() const { return sl_; }

  // legacy interfaces
  // TODO:(xliu) remove these.
  // Returns true if the node is added successfully, false if not, i.e. the
  // node with the same key already existed in the list.
  bool contains(const key_type& data) const { return sl_->find(data); }
  bool add(const key_type& data) { return sl_->addOrGetData(data).second; }
  bool remove(const key_type& data) { return sl_->remove(data); }

 private:
  SkipListType* sl_;
  std::shared_ptr<SkipListType> slHolder_;
};

// implements forward iterator concept.
template <typename ValT, typename NodeT>
class detail::csl_iterator : public detail::IteratorFacade<
                                 csl_iterator<ValT, NodeT>,
                                 ValT,
                                 std::forward_iterator_tag> {
 public:
  typedef ValT value_type;
  typedef value_type& reference;
  typedef value_type* pointer;
  typedef ptrdiff_t difference_type;

  explicit csl_iterator(NodeT* node = nullptr) : node_(node) {}

  template <typename OtherVal, typename OtherNode>
  csl_iterator(
      const csl_iterator<OtherVal, OtherNode>& other,
      typename std::enable_if<
          std::is_convertible<OtherVal*, ValT*>::value>::type* = nullptr)
      : node_(other.node_) {}

  size_t nodeSize() const {
    return node_ == nullptr ? 0
                            : node_->height() * sizeof(NodeT*) + sizeof(*this);
  }

  bool good() const { return node_ != nullptr; }

 private:
  template <class, class>
  friend class csl_iterator;
  friend class detail::
      IteratorFacade<csl_iterator, ValT, std::forward_iterator_tag>;

  void increment() { node_ = node_->next(); }
  bool equal(const csl_iterator& other) const { return node_ == other.node_; }
  value_type& dereference() const { return node_->data(); }

  NodeT* node_;
};

// Skipper interface
template <typename T, typename Comparator, typename NodeAlloc, int MAX_HEIGHT>
class ConcurrentSkipList<T, Comparator, NodeAlloc, MAX_HEIGHT>::Skipper {
  typedef detail::SkipListNode<T> NodeType;
  typedef ConcurrentSkipList<T, Comparator, NodeAlloc, MAX_HEIGHT> SkipListType;
  typedef typename SkipListType::Accessor Accessor;

 public:
  typedef T value_type;
  typedef T& reference;
  typedef T* pointer;
  typedef ptrdiff_t difference_type;

  Skipper(std::shared_ptr<SkipListType> skipList)
      : accessor_(std::move(skipList)) {
    init();
  }

  Skipper(const Accessor& accessor) : accessor_(accessor) { init(); }

  void init() {
    // need to cache the head node
    NodeType* head_node = head();
    headHeight_ = head_node->height();
    for (int i = 0; i < headHeight_; ++i) {
      preds_[i] = head_node;
      succs_[i] = head_node->skip(i);
    }
    int max_layer = maxLayer();
    for (int i = 0; i < max_layer; ++i) {
      hints_[i] = uint8_t(i + 1);
    }
    hints_[max_layer] = max_layer;
  }

  // advance to the next node in the list.
  Skipper& operator++() {
    preds_[0] = succs_[0];
    succs_[0] = preds_[0]->skip(0);
    int height = curHeight();
    for (int i = 1; i < height && preds_[0] == succs_[i]; ++i) {
      preds_[i] = succs_[i];
      succs_[i] = preds_[i]->skip(i);
    }
    return *this;
  }

  Accessor& accessor() { return accessor_; }
  const Accessor& accessor() const { return accessor_; }

  bool good() const { return succs_[0] != nullptr; }

  int maxLayer() const { return headHeight_ - 1; }

  int curHeight() const {
    // need to cap the height to the cached head height, as the current node
    // might be some newly inserted node and also during the time period the
    // head height may have grown.
    return succs_[0] ? std::min(headHeight_, succs_[0]->height()) : 0;
  }

  const value_type& data() const {
    // DCHECK(succs_[0] != nullptr);
    return succs_[0]->data();
  }

  value_type& operator*() const {
    // DCHECK(succs_[0] != nullptr);
    return succs_[0]->data();
  }

  value_type* operator->() {
    // DCHECK(succs_[0] != nullptr);
    return &succs_[0]->data();
  }

  /*
   * Skip to the position whose data is no less than the parameter.
   * (I.e. the lower_bound).
   *
   * Returns true if the data is found, false otherwise.
   */
  bool to(const value_type& data) {
    int layer = curHeight() - 1;
    if (layer < 0) {
      return false; // reaches the end of the list
    }

    int lyr = hints_[layer];
    int max_layer = maxLayer();
    while (this->greater(data, succs_[lyr]) && lyr < max_layer) {
      ++lyr;
    }
    hints_[layer] = lyr; // update the hint

    int foundLayer = this->findInsertionPoint(
        preds_[lyr], lyr, data, preds_, succs_);
    if (foundLayer < 0) {
      return false;
    }

    // DCHECK(succs_[0] != nullptr)
    //     << "lyr=" << lyr << "; max_layer=" << max_layer;
    return !succs_[0]->markedForRemoval();
  }

 private:
  NodeType* head() const {
    return accessor_.skiplist()->head_.load(std::memory_order_acquire);
  }

  Accessor accessor_;
  int headHeight_;
  NodeType *succs_[MAX_HEIGHT], *preds_[MAX_HEIGHT];
  uint8_t hints_[MAX_HEIGHT];
};

} // namespace skiplist
} // namespace utility