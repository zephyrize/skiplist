# skiplist
High Concurrency skiplist base on c++11

线程安全的跳表两种实现方式，>=c++11环境可使用

- 第一种是基于leveldb的跳表实现，由于leveldb中的跳表没有提供删除元素的接口，该项目源码中添加了删除的接口，同时将内存分配器改为tlsf算法，缓解频繁插入删除带来的内存碎片问题。
- 第二种是folly库的concurretr_skiplist实现，原库中concurretr_skiplist需要依赖boost，且不支持c++11，该项目基于源码做了一些修改，主要是将依赖的boost中的函数重写，采用c++11支持的语法重构了部分代码（大部分为模板元编程）


## leveldb-skiplist使用方式

```c++
// 定义排序方式
using namespace utility::skiplist;
struct Comparator {
    int operator()(const Key &a, const Key &b) const {
        if (a < b) {
            return -1;
        }
        else if (a > b) {
            return 1;
        }
        else {
            return 0;
        }
    }
};

// 初始化内存分配器，可以传入参数指定内存池大小，默认为256k
MemoryPoolTLSF tlsf;
// 初始化比较器
Comparator cmp;
// 初始化跳表
SkipList<Key, Comparator> skiplist(cmp, &tlsf);
// 插入元素
skiplist.Insert(1);
// 删除元素
skiplist.Delete(1);
// 判断元素是否存在
skiplist.Contains(1);
// 获取元素个数
skiplist.size()


// 迭代器遍历
SkipList<Key, Comparator>::Iterator iter(&list);
// 直接定位到某个key位置，如果元素不存在则定位到>=key的第一个位置
int key = 1;
iter.Seek(key);
while (iter.Valid()) {
    std::cout << iter.key() << std::endl;
    iter.Next();
}

// 定位到第一个和最后一个元素
iter.SeekToFirst();
iter.SeekToLast();

// 定位到前一个元素
iter.Prev()

```
> 为什么`leveldb`没有提供删除的接口？
- 设计简化： 跳表的核心目的是支持高效的查找、插入和范围查询操作，而删除操作相对较少发生，并且对性能的影响较大。为了简化实现和优化常见操作（如插入和查找），LevelDB 通过不提供直接删除接口来减少复杂度。
- 删除通过标记： 在 LevelDB 中，删除操作并不是通过立即在跳表中删除元素，而是通过 “标记删除” 来实现的。这是因为跳表的结构需要保持其有序性，直接删除元素可能会破坏跳表的平衡。在实际删除时，LevelDB 会将删除标记添加到元素上，实际的删除操作发生在一个后续的“垃圾回收”阶段，通常是通过合并（Compaction）来处理的。
- 延迟删除： 由于 LevelDB 使用了类似 Log-Structured Merge Tree (LSM-Tree) 的结构，所有写操作（包括删除）最终会被合并到一个新的 SSTable 文件中。在合并过程中，标记为删除的元素才会被彻底清除。因此，删除并不是即时进行的，而是延迟执行，减少了频繁修改跳表结构的需求。
- 性能考虑： 跳表删除操作通常需要调整多个链表的指针，以确保跳表的平衡。如果频繁删除元素，可能会影响性能。通过标记删除和延迟回收，LevelDB 能够避免这种性能损失。

## concurrent-skiplist使用方式

```c++
using namespace utility::skiplist;

// 定义比较器
struct MyComparator {
    bool operator()(int lhs, int rhs) const {
        return lhs < rhs;
    }
};


using SkipList = ConcurrentSkipList<int, MyComparator>;
MyComparator cmp;
// 创建一个跳表
auto skiplist = SkipList::create(12, cmp);

// 也可以使用createInstance函数创建，返回一个shared_ptr
// auto skiplist = SkipList::createInstance(12, cmp);

// 插入元素：提供了两个接口：add和insert
// add: 如果元素已经存在，则返回false，否则返回true
// insert: 返回一个pair，first为插入记录的迭代器，second为bool，true表示插入成功，false表示插入失败
skiplist.add(1);
auto res = skiplist.insert(2);
std::cout << *res.first << " " << res.second << std::endl;
ASSERT_TRUE(skiplist.add(3));
ASSERT_FALSE(skiplist.add(3));

// 判断元素是否存在
ASSERT_TRUE(skiplist.contains(1));

// 删除元素：提供了两个接口：erase和remove
// erase: 返回删除元素的个数, 如果删除成功，则返回1，否则返回0
// remove: 返回bool，true表示删除成功，false表示删除失败
std::cout << "erase: "  << skiplist.erase(1) << std::endl;
ASSERT_FALSE(skiplist.contains(1));

ASSERT_TRUE(skiplist.contains(2));
ASSERT_TRUE(skiplist.remove(2));
ASSERT_FALSE(skiplist.remove(2));

// size()返回当前元素个数
std::cout << "skiplist size: " << skiplist.size() << std::endl;


// 迭代器遍历方式1
for(const auto& key : skiplist) {
    std::cout << key << " ";
}
std::cout << std::endl;

// 迭代器遍历方式2
for (auto iter = skiplist.begin(); iter != skiplist.end(); ++iter) {
    std::cout << *iter << " ";
}
std::cout << std::endl;

```