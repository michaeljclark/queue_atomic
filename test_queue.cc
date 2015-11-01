//
//  test_queue.cc
//

#include <cstdio>
#include <cstdint>
#include <cstdarg>
#include <cassert>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <chrono>
#include <vector>
#include <queue>
#include <set>

extern void log_debug(const char* fmt, ...);

#include "rdtsc.h"
#include "queue_atomic_v1.h"
#include "queue_atomic_v2.h"
#include "queue_atomic_v3.h"
#include "queue_std_mutex.h"

using namespace std::chrono;

typedef unsigned long long u64;


void log_prefix(const char* prefix, const char* fmt, va_list arg)
{
    std::vector<char> buf(1024);
    
    int len = vsnprintf(buf.data(), buf.capacity(), fmt, arg);

    if (len >= (int)buf.capacity()) {
        buf.resize(len + 1);
        vsnprintf(buf.data(), buf.capacity(), fmt, arg);
    }
    
    fprintf(stderr, "%s: %s\n", prefix, buf.data());
}

void log_debug(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_prefix("debug", fmt, ap);
    va_end(ap);
}


/* test_push_pop_worker */

template<typename item_type, typename queue_type>
struct test_push_pop_worker : std::thread
{
    typedef std::vector<item_type> vec_type;
    
    vec_type vec;
    queue_type &queue;
    const size_t items_per_thread;
    std::thread thread;
    
    test_push_pop_worker(queue_type &queue, const size_t items_per_thread)
        : queue(queue), items_per_thread(items_per_thread), thread(&test_push_pop_worker::mainloop, this) {}
    
    void mainloop()
    {
        // transfer items from the queue to the vector
        for (size_t i = 0; i < items_per_thread; i++) {
            item_type v = queue.pop_front();
            if (v) {
                vec.push_back(v);
            } else {
                log_debug("%p queue.pop_front() returned null item", std::this_thread::get_id());
            }
        }
        // transfer items from vector to the queue
        for (auto v : vec) {
            if (!queue.push_back(v)) {
                log_debug("%p queue.push_back() returned false", std::this_thread::get_id());
            }
        }
    }
};

/* test_push_pop_threads */

template<typename item_type, typename queue_type>
void test_push_pop_threads(const char* queue_type_name, const size_t num_threads, const size_t iterations, const size_t items_per_thread)
{
    const size_t num_items = num_threads * items_per_thread;
    const size_t num_ops = num_items * iterations;
    
    typedef test_push_pop_worker<item_type, queue_type> worker_type;
    typedef std::shared_ptr<worker_type> worker_ptr;
    typedef std::vector<worker_ptr> worker_list;
    typedef std::set<item_type> set_type;
    
    queue_type queue(num_items);
    
    // populate queue
    assert(queue.size() == 0);
    for (size_t i = 1; i <= num_items; i++) {
        queue.push_back(item_type(i));
    }
    assert(queue.size() == num_items);
    
    // run test iterations
    const auto t1 = std::chrono::high_resolution_clock::now();
    for (size_t iter = 0; iter < iterations; iter++)
    {
        // start worker threads
        worker_list workers;
        for (size_t i = 0; i < num_threads; i++) {
            workers.push_back(std::make_shared<worker_type>(queue, items_per_thread));
        }
        
        // join worker threads
        for (auto worker : workers) {
            worker->thread.join();
        }
        assert(queue.size() == num_items);
    }
    const auto t2 = std::chrono::high_resolution_clock::now();
    uint64_t work_time_us = duration_cast<microseconds>(t2 - t1).count();
    
    // transfer items to a set
    set_type check_set;
    for (size_t i = 1; i <= num_items; i++) {
        item_type v = queue.pop_front();
        if (v) {
            check_set.insert(v);
        } else {
            log_debug("queue.pop_front() returned null item");
        }
    }
    assert(queue.size() == 0);
    
    // check items in set
    size_t check_count = 0;
    for (size_t i = 1; i <= num_items; i++) {
        if (check_set.find(item_type(i)) != check_set.end()) {
            check_count++;
        }
    }
    assert(check_count == num_items);
    
    printf("test_push_pop_threads::%-25s num_threads=%-3lu iterations=%-5lu items_per_thread=%-9lu "
           "time(µs)=%-9llu ops=%-9llu op_time(µs)=%-9.6lf\n",
            queue_type_name, num_threads, iterations, items_per_thread,
            (u64)work_time_us, (u64)num_ops, (double)work_time_us / (double)num_ops);
}


/* test_queue */

struct test_queue
{
    
    void test_queue_constants_v1()
    {
        const size_t qsize = 1024;
        typedef queue_atomic_v1<void*> qtype;
        qtype q(qsize);
        
        printf("queue_atomic_v1::is_lock_free  = %u\n", q.counter.is_lock_free());
        printf("queue_atomic_v1::atomic_bits   = %u\n", qtype::atomic_bits);
        printf("queue_atomic_v1::offset_bits   = %u\n", qtype::offset_bits);
        printf("queue_atomic_v1::version_bits  = %u\n", qtype::version_bits);
        printf("queue_atomic_v1::front_shift   = %u\n", qtype::front_shift);
        printf("queue_atomic_v1::back_shift    = %u\n", qtype::back_shift);
        printf("queue_atomic_v1::version_shift = %u\n", qtype::version_shift);
        printf("queue_atomic_v1::size_max      = 0x%016llx (%llu)\n", (u64)qtype::size_max, (u64)qtype::size_max);
        printf("queue_atomic_v1::offset_limit  = 0x%016llx (%llu)\n", (u64)qtype::offset_limit, (u64)qtype::offset_limit);
        printf("queue_atomic_v1::version_limit = 0x%016llx (%llu)\n", (u64)qtype::version_limit, (u64)qtype::version_limit);
        printf("queue_atomic_v1::offset_mask   = 0x%016llx\n", (u64)qtype::offset_mask);
        printf("queue_atomic_v1::version_mask  = 0x%016llx\n", (u64)qtype::version_mask);
        printf("queue_atomic_v1::front_pack    = 0x%016llx\n", (u64)qtype::front_pack);
        printf("queue_atomic_v1::back_pack     = 0x%016llx\n", (u64)qtype::back_pack);
        printf("queue_atomic_v1::version_pack  = 0x%016llx\n", (u64)qtype::version_pack);
        
        assert(qtype::atomic_bits   == 64);
        assert(qtype::offset_bits   == 24);
        assert(qtype::version_bits  == 16);
        assert(qtype::front_shift   == 0);
        assert(qtype::back_shift    == 24);
        assert(qtype::version_shift == 48);
        assert(qtype::size_max      == 8388608);
        assert(qtype::offset_limit  == 16777216);
        assert(qtype::version_limit == 65536);
        assert(qtype::offset_mask   == 0x00ffffff);
        assert(qtype::version_mask  == 0x0000ffff);
        assert(qtype::front_pack    == 0x0000000000ffffffULL);
        assert(qtype::back_pack     == 0x0000ffffff000000ULL);
        assert(qtype::version_pack  == 0xffff000000000000ULL);
    }
    
    void test_empty_invariants_v1()
    {
        const size_t qsize = 1024;
        typedef queue_atomic_v1<void*> qtype;
        qtype q(qsize);
        
        assert(q.capacity() == 1024);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
        assert(q.size_limit == 1024);
        assert(q._last_version() == 0);
        assert(q._version() == 0);
        assert(q._back() == 0);
        assert(q._front() == 1024);
    }
    
    void test_push_pop_v1()
    {
        const size_t qsize = 4;
        typedef queue_atomic_v1<void*> qtype;
        qtype q(qsize);
        
        // check initial invariants
        assert(q.capacity() == qsize);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
        assert(q.size_limit == qsize);
        assert(q._last_version() == 0);
        assert(q._version() == 0);
        assert(q._back() == 0);
        assert(q._front() == qsize);
        
        // push_back 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.push_back((void*)i) == true);
            assert(q._last_version() == i);
            assert(q._version() == i);
            assert(q._back() == i);
            assert(q._front() == qsize);
            assert(q.size() == i);
            assert(q.empty() == false);
            assert(q.full() == (i < 4 ? false : true));
        }
        
        // push_back overflow test
        assert(q.push_back((void*)5) == false);
        assert(q._last_version() == 4);
        assert(q._version() == 4);
        assert(q._back() == 4);
        assert(q._front() == qsize);
        assert(q.size() == 4);
        assert(q.empty() == false);
        assert(q.full() == true);
        
        // pop_front 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.pop_front() == (void*)i);
            assert(q._last_version() == 4 + i);
            assert(q._version() == 4 + i);
            assert(q._back() == 4);
            assert(q._front() == 4 + i);
            assert(q.size() == 4 - i);
            assert(q.empty() == (i > 3 ? true : false));
            assert(q.full() == false);
        }
        
        // pop_front underflow test
        assert(q.pop_front() == (void*)0);
        assert(q._last_version() == 8);
        assert(q._version() == 8);
        assert(q._back() == 4);
        assert(q._front() == 8);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
        
        // push_back 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.push_back((void*)i) == true);
            assert(q._last_version() == 8 + i);
            assert(q._version() == 8 + i);
            assert(q._back() == 4 + i);
            assert(q._front() == 8);
            assert(q.size() == i);
            assert(q.empty() == false);
            assert(q.full() == (i < 4 ? false : true));
        }
        
        // push_back overflow test
        assert(q.push_back((void*)5) == false);
        assert(q._last_version() == 12);
        assert(q._version() == 12);
        assert(q._back() == 8);
        assert(q._front() == 8);
        assert(q.size() == 4);
        assert(q.empty() == false);
        assert(q.full() == true);
        
        // pop_front 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.pop_front() == (void*)i);
            assert(q._last_version() == 12 + i);
            assert(q._version() == 12 + i);
            assert(q._back() == 8);
            assert(q._front() == 8 + i);
            assert(q.size() == 4 - i);
            assert(q.empty() == (i > 3 ? true : false));
            assert(q.full() == false);
        }
        
        // pop_front underflow test
        assert(q.pop_front() == (void*)0);
        assert(q._last_version() == 16);
        assert(q._version() == 16);
        assert(q._back() == 8);
        assert(q._front() == 12);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
    }

    void test_queue_constants_v2()
    {
        const size_t qsize = 1024;
        typedef queue_atomic_v2<void*> qtype;
        qtype q(qsize);
        
        printf("queue_atomic_v2::is_lock_free  = %u\n", q.counter.is_lock_free());
        printf("queue_atomic_v2::atomic_bits   = %u\n", qtype::atomic_bits);
        printf("queue_atomic_v2::offset_bits   = %u\n", qtype::offset_bits);
        printf("queue_atomic_v2::version_bits  = %u\n", qtype::version_bits);
        printf("queue_atomic_v2::offset_shift  = %u\n", qtype::offset_shift);
        printf("queue_atomic_v2::version_shift = %u\n", qtype::version_shift);
        printf("queue_atomic_v2::size_max      = 0x%016llx (%llu)\n", (u64)qtype::size_max, (u64)qtype::size_max);
        printf("queue_atomic_v2::offset_limit  = 0x%016llx (%llu)\n", (u64)qtype::offset_limit, (u64)qtype::offset_limit);
        printf("queue_atomic_v2::version_limit = 0x%016llx (%llu)\n", (u64)qtype::version_limit, (u64)qtype::version_limit);
        printf("queue_atomic_v2::offset_mask   = 0x%016llx\n", (u64)qtype::offset_mask);
        printf("queue_atomic_v2::version_mask  = 0x%016llx\n", (u64)qtype::version_mask);

        assert(qtype::atomic_bits   == 64);
        assert(qtype::offset_bits   == 32);
        assert(qtype::version_bits  == 32);
        assert(qtype::offset_shift  == 0);
        assert(qtype::version_shift == 32);
        assert(qtype::size_max      == 2147483648);
        assert(qtype::offset_limit  == 4294967296);
        assert(qtype::version_limit == 4294967296);
        assert(qtype::offset_mask   == 0x00000000ffffffffULL);
        assert(qtype::version_mask  == 0x00000000ffffffffULL);
    }

    void test_empty_invariants_v2()
    {
        const size_t qsize = 1024;
        typedef queue_atomic_v2<void*> qtype;
        qtype q(qsize);
        
        assert(q.capacity() == 1024);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
        assert(q.size_limit == 1024);
        assert(q._last_version() == 0);
        assert(q._back_version() == 0);
        assert(q._front_version() == 0);
        assert(q._back() == 0);
        assert(q._front() == 1024);
    }
    
    void test_push_pop_v2()
    {
        const size_t qsize = 4;
        typedef queue_atomic_v2<void*> qtype;
        qtype q(qsize);

        // check initial invariants
        assert(q.capacity() == qsize);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
        assert(q.size_limit == qsize);
        assert(q._last_version() == 0);
        assert(q._back_version() == 0);
        assert(q._front_version() == 0);
        assert(q._back() == 0);
        assert(q._front() == qsize);
        
        // push_back 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.push_back((void*)i) == true);
            assert(q._last_version() == i);
            assert(q._back_version() == i);
            assert(q._front_version() == 0);
            assert(q._back() == i);
            assert(q._front() == qsize);
            assert(q.size() == i);
            assert(q.empty() == false);
            assert(q.full() == (i < 4 ? false : true));
        }
  
        // push_back overflow test
        assert(q.push_back((void*)5) == false);
        assert(q._last_version() == 4);
        assert(q._back_version() == 4);
        assert(q._front_version() == 0);
        assert(q._back() == 4);
        assert(q._front() == qsize);
        assert(q.size() == 4);
        assert(q.empty() == false);
        assert(q.full() == true);
        
        // pop_front 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.pop_front() == (void*)i);
            assert(q._last_version() == 4 + i);
            assert(q._back_version() == 4);
            assert(q._front_version() == 4 + i);
            assert(q._back() == 4);
            assert(q._front() == 4 + i);
            assert(q.size() == 4 - i);
            assert(q.empty() == (i > 3 ? true : false));
            assert(q.full() == false);
        }
        
        // pop_front underflow test
        assert(q.pop_front() == (void*)0);
        assert(q._last_version() == 8);
        assert(q._back_version() == 4);
        assert(q._front_version() == 8);
        assert(q._back() == 4);
        assert(q._front() == 8);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);

        // push_back 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.push_back((void*)i) == true);
            assert(q._last_version() == 8 + i);
            assert(q._back_version() == 8 + i);
            assert(q._front_version() == 8);
            assert(q._back() == 4 + i);
            assert(q._front() == 8);
            assert(q.size() == i);
            assert(q.empty() == false);
            assert(q.full() == (i < 4 ? false : true));
        }

        // push_back overflow test
        assert(q.push_back((void*)5) == false);
        assert(q._last_version() == 12);
        assert(q._back_version() == 12);
        assert(q._front_version() == 8);
        assert(q._back() == 8);
        assert(q._front() == 8);
        assert(q.size() == 4);
        assert(q.empty() == false);
        assert(q.full() == true);
        
        // pop_front 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.pop_front() == (void*)i);
            assert(q._last_version() == 12 + i);
            assert(q._back_version() == 12);
            assert(q._front_version() == 12 + i);
            assert(q._back() == 8);
            assert(q._front() == 8 + i);
            assert(q.size() == 4 - i);
            assert(q.empty() == (i > 3 ? true : false));
            assert(q.full() == false);
        }

        // pop_front underflow test
        assert(q.pop_front() == (void*)0);
        assert(q._last_version() == 16);
        assert(q._back_version() == 12);
        assert(q._front_version() == 16);
        assert(q._back() == 8);
        assert(q._front() == 12);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
    }
    
    void test_queue_constants_v3()
    {
        const size_t qsize = 1024;
        typedef queue_atomic_v3<void*> qtype;
        qtype q(qsize);
        
        printf("queue_atomic_v3::is_lock_free  = %u\n", q.counter_back.is_lock_free());
        printf("queue_atomic_v3::atomic_bits   = %u\n", qtype::atomic_bits);
        printf("queue_atomic_v3::offset_bits   = %u\n", qtype::offset_bits);
        printf("queue_atomic_v3::version_bits  = %u\n", qtype::version_bits);
        printf("queue_atomic_v3::offset_shift  = %u\n", qtype::offset_shift);
        printf("queue_atomic_v3::version_shift = %u\n", qtype::version_shift);
        printf("queue_atomic_v3::size_max      = 0x%016llx (%llu)\n", (u64)qtype::size_max, (u64)qtype::size_max);
        printf("queue_atomic_v3::offset_limit  = 0x%016llx (%llu)\n", (u64)qtype::offset_limit, (u64)qtype::offset_limit);
        printf("queue_atomic_v3::version_limit = 0x%016llx (%llu)\n", (u64)qtype::version_limit, (u64)qtype::version_limit);
        printf("queue_atomic_v3::offset_mask   = 0x%016llx\n", (u64)qtype::offset_mask);
        printf("queue_atomic_v3::version_mask  = 0x%016llx\n", (u64)qtype::version_mask);
        
        assert(qtype::atomic_bits   == 64);
        assert(qtype::offset_bits   == 32);
        assert(qtype::version_bits  == 32);
        assert(qtype::offset_shift  == 0);
        assert(qtype::version_shift == 32);
        assert(qtype::size_max      == 2147483648);
        assert(qtype::offset_limit  == 4294967296);
        assert(qtype::version_limit == 4294967296);
        assert(qtype::offset_mask   == 0x00000000ffffffffULL);
        assert(qtype::version_mask  == 0x00000000ffffffffULL);
    }
    
    void test_empty_invariants_v3()
    {
        const size_t qsize = 1024;
        typedef queue_atomic_v3<void*> qtype;
        qtype q(qsize);
        
        assert(q.capacity() == 1024);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
        assert(q.size_limit == 1024);
        assert(q._back_version() == 0);
        assert(q._front_version() == 0);
        assert(q._back() == 0);
        assert(q._front() == 1024);
    }
    
    void test_push_pop_v3()
    {
        const size_t qsize = 4;
        typedef queue_atomic_v3<void*> qtype;
        qtype q(qsize);
        
        // check initial invariants
        assert(q.capacity() == qsize);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
        assert(q.size_limit == qsize);
        assert(q._back_version() == 0);
        assert(q._front_version() == 0);
        assert(q._back() == 0);
        assert(q._front() == qsize);
        
        // push_back 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.push_back((void*)i) == true);
            assert(q._back_version() == i);
            assert(q._front_version() == 0);
            assert(q._back() == i);
            assert(q._front() == qsize);
            assert(q.size() == i);
            assert(q.empty() == false);
            assert(q.full() == (i < 4 ? false : true));
        }
        
        // push_back overflow test
        assert(q.push_back((void*)5) == false);
        assert(q._back_version() == 4);
        assert(q._front_version() == 0);
        assert(q._back() == 4);
        assert(q._front() == qsize);
        assert(q.size() == 4);
        assert(q.empty() == false);
        assert(q.full() == true);
        
        // pop_front 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.pop_front() == (void*)i);
            assert(q._back_version() == 4);
            assert(q._front_version() == i);
            assert(q._back() == 4);
            assert(q._front() == 4 + i);
            assert(q.size() == 4 - i);
            assert(q.empty() == (i > 3 ? true : false));
            assert(q.full() == false);
        }
        
        // pop_front underflow test
        assert(q.pop_front() == (void*)0);
        assert(q._back_version() == 4);
        assert(q._front_version() == 4);
        assert(q._back() == 4);
        assert(q._front() == 8);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
        
        // push_back 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.push_back((void*)i) == true);
            assert(q._back_version() == 4 + i);
            assert(q._front_version() == 4);
            assert(q._back() == 4 + i);
            assert(q._front() == 8);
            assert(q.size() == i);
            assert(q.empty() == false);
            assert(q.full() == (i < 4 ? false : true));
        }
        
        // push_back overflow test
        assert(q.push_back((void*)5) == false);
        assert(q._back_version() == 8);
        assert(q._front_version() == 4);
        assert(q._back() == 8);
        assert(q._front() == 8);
        assert(q.size() == 4);
        assert(q.empty() == false);
        assert(q.full() == true);
        
        // pop_front 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.pop_front() == (void*)i);
            assert(q._back_version() == 8);
            assert(q._front_version() == 4 + i);
            assert(q._back() == 8);
            assert(q._front() == 8 + i);
            assert(q.size() == 4 - i);
            assert(q.empty() == (i > 3 ? true : false));
            assert(q.full() == false);
        }
        
        // pop_front underflow test
        assert(q.pop_front() == (void*)0);
        assert(q._back_version() == 8);
        assert(q._front_version() == 8);
        assert(q._back() == 8);
        assert(q._front() == 12);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
    }

    void test_push_pop_threads_queue_mutex()
    {
        test_push_pop_threads<int,queue_std_mutex<int>>("queue_std_mutex", 8, 10, 1024);
    }

    void test_push_pop_threads_v1()
    {
        test_push_pop_threads<int,queue_atomic_v1<int>>("queue_atomic_v1", 8, 10, 1024);
        test_push_pop_threads<int,queue_atomic_v1<int>>("queue_atomic_v1", 8, 10, 1024);
        test_push_pop_threads<int,queue_atomic_v1<int>>("queue_atomic_v1", 8, 10, 65536);
        test_push_pop_threads<int,queue_atomic_v1<int>>("queue_atomic_v1", 8, 10, 65536);
        test_push_pop_threads<int,queue_atomic_v1<int>>("queue_atomic_v1", 8, 64, 65536);
        test_push_pop_threads<int,queue_atomic_v1<int>>("queue_atomic_v1", 8, 64, 65536);
        test_push_pop_threads<int,queue_atomic_v1<int>>("queue_atomic_v1", 8, 16, 262144);
        test_push_pop_threads<int,queue_atomic_v1<int>>("queue_atomic_v1", 8, 16, 262144);
    }

    void test_push_pop_threads_v2()
    {
        test_push_pop_threads<int,queue_atomic_v2<int>>("queue_atomic_v2", 8, 10, 1024);
        test_push_pop_threads<int,queue_atomic_v2<int>>("queue_atomic_v2", 8, 10, 1024);
        test_push_pop_threads<int,queue_atomic_v2<int>>("queue_atomic_v2", 8, 10, 65536);
        test_push_pop_threads<int,queue_atomic_v2<int>>("queue_atomic_v2", 8, 10, 65536);
        test_push_pop_threads<int,queue_atomic_v2<int>>("queue_atomic_v2", 8, 64, 65536);
        test_push_pop_threads<int,queue_atomic_v2<int>>("queue_atomic_v2", 8, 64, 65536);
        test_push_pop_threads<int,queue_atomic_v2<int>>("queue_atomic_v2", 8, 16, 262144);
        test_push_pop_threads<int,queue_atomic_v2<int>>("queue_atomic_v2", 8, 16, 262144);
    }

    void test_push_pop_threads_v3()
    {
        test_push_pop_threads<int,queue_atomic_v3<int>>("queue_atomic_v3", 8, 10, 1024);
        test_push_pop_threads<int,queue_atomic_v3<int>>("queue_atomic_v3", 8, 10, 1024);
        test_push_pop_threads<int,queue_atomic_v3<int>>("queue_atomic_v3", 8, 10, 65536);
        test_push_pop_threads<int,queue_atomic_v3<int>>("queue_atomic_v3", 8, 10, 65536);
        test_push_pop_threads<int,queue_atomic_v3<int>>("queue_atomic_v3", 8, 64, 65536);
        test_push_pop_threads<int,queue_atomic_v3<int>>("queue_atomic_v3", 8, 64, 65536);
        test_push_pop_threads<int,queue_atomic_v3<int>>("queue_atomic_v3", 8, 16, 262144);
        test_push_pop_threads<int,queue_atomic_v3<int>>("queue_atomic_v3", 8, 16, 262144);
    }

    void test_push_pop_threads_v1_contention()
    {
        test_push_pop_threads<int,queue_atomic_v1<int,true>>("queue_atomic_v1:contention", 1, 10, 65536);
        test_push_pop_threads<int,queue_atomic_v1<int,true>>("queue_atomic_v1:contention", 8, 1, 256);
    }
    
    void test_push_pop_threads_v2_contention()
    {
        test_push_pop_threads<int,queue_atomic_v2<int,true>>("queue_atomic_v2:contention", 1, 10, 65536);
        test_push_pop_threads<int,queue_atomic_v2<int,true>>("queue_atomic_v2:contention", 8, 1, 256);
    }

    void test_push_pop_threads_v3_contention()
    {
        test_push_pop_threads<int,queue_atomic_v3<int,true>>("queue_atomic_v3:contention", 1, 10, 65536);
        test_push_pop_threads<int,queue_atomic_v3<int,true>>("queue_atomic_v3:contention", 8, 1, 256);
    }
};

int main(int argc, const char * argv[])
{
    test_queue tq;
    tq.test_queue_constants_v1();
    tq.test_empty_invariants_v1();
    tq.test_queue_constants_v2();
    tq.test_empty_invariants_v2();
    tq.test_queue_constants_v3();
    tq.test_empty_invariants_v3();
    tq.test_push_pop_v1();
    tq.test_push_pop_v2();
    tq.test_push_pop_v3();
    tq.test_push_pop_threads_queue_mutex();
    tq.test_push_pop_threads_v1();
    tq.test_push_pop_threads_v2();
    tq.test_push_pop_threads_v3();
    tq.test_push_pop_threads_v1_contention();
    tq.test_push_pop_threads_v2_contention();
    tq.test_push_pop_threads_v3_contention();
}

