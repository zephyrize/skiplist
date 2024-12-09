#pragma once

#include "tlsf.h"

#include <cstdint>
#include <iostream>
#include <vector>
#include <cassert>

/**
 * @brief 封装的tlsf内存池，构造时指定预分配的大小，默认为256K，增长速率为1.5
 */

namespace utility {
namespace memorypool {

typedef char* PoolTypeStr;
typedef char PoolType;

class MemoryPoolTLSF {
public:
    MemoryPoolTLSF(size_t size = 256 * 1024)
        : initial_size_(size),
          cur_pool_size_(size)
    {
        addPool(size);
    }

    ~MemoryPoolTLSF() {
        printf("MemoryPoolTLSF::~MemoryPoolTLSF, release MemoryPoolTLSF\n");
        tlsf_destroy(tlsf_);
        for (auto& pool : pools_) {
            ::free(pool);
        }
    }

    void* malloc(size_t size) {
        void* ptr = tlsf_malloc(tlsf_, size);
        if (!ptr) {
            if (addPool(cur_pool_size_)) {
                ptr = tlsf_malloc(tlsf_, size);
            }
            else {
                return nullptr;
            }
        }
        return ptr;
    }

    void free(void* ptr) {
        tlsf_free(tlsf_, ptr);
    }

    MemoryPoolTLSF(const MemoryPoolTLSF&) = delete;
    MemoryPoolTLSF& operator=(const MemoryPoolTLSF&) = delete;

private:
    bool addPool(size_t size) {
        size_t total_size = cur_pool_size_ 
                            + tlsf_size() 
                            + tlsf_pool_overhead()
                            + tlsf_alloc_overhead();
        printf("begin add pool, size:%d\n", size);
        void* pool = ::malloc(total_size);
        if (pool) {
            if (cur_pool_size_ == initial_size_) {
                printf("Firstly create tlsf pool, size: %d\n", size);
                tlsf_ = tlsf_create_with_pool(pool, total_size);
            }
            else {
                tlsf_add_pool(tlsf_, pool, cur_pool_size_);
            }

            pools_.emplace_back(pool);
            cur_pool_size_ = static_cast<size_t>(cur_pool_size_ * INC_RATIO);
            return true;
        }
        else {
            printf("System Error: memory allocated failed");
            return false;
        }
    }

private:
    size_t initial_size_;
    size_t cur_pool_size_;
    tlsf_t tlsf_;
    const double INC_RATIO = 1.5;

    std::vector<void*> pools_;
};

} // namespace memorypool
} // namespace utility