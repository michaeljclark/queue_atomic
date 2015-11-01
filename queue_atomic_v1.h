//
//  queue_atomic_v1.h
//

#ifndef queue_atomic_v1_h
#define queue_atomic_v1_h

/*
 * queue_atomic_v1
 *
 *   - uses 2 atomic variables: counter and offset_pack
 *
 *   - push_back reads 2 atomics: counter and offset_pack
 *               writes 2 atomics: counter and offset_pack
 *
 *   - pop_front reads 2 atomics: counter and offset_pack
 *               writes 2 atomics: counter and offset_pack
 *
 *   - front, back and version are packed into offset_pack
 *
 *   - version is used for conflict detection during ordered writes
 *
 *   * NOTE: suffers cache line contention with concurrent push and pop
 *
 */

template <typename T,
          const int debug_contention = false,
          typename ATOMIC_UINT = uint64_t,
          const int OFFSET_BITS = 24,
          const int VERSION_BITS = 16,
          std::memory_order relaxed_memory_order = std::memory_order_relaxed,
          std::memory_order acquire_memory_order = std::memory_order_acquire,
          std::memory_order release_memory_order = std::memory_order_release>
struct queue_atomic_v1
{
    /* queue atomic type */
    
    typedef ATOMIC_UINT                         atomic_uint_t;
    typedef std::atomic<T>                      atomic_item_t;
    
    
    /* queue constants */
    
    static const int spin_limit =               1 << 24;
    static const int debug_spin =               true;
    static const int atomic_bits =              sizeof(atomic_uint_t) << 3;
    static const int offset_bits =              OFFSET_BITS;
    static const int version_bits =             VERSION_BITS;
    static const int front_shift =              0;
    static const int back_shift =               offset_bits;
    static const int version_shift =            offset_bits + offset_bits;
    static const atomic_uint_t size_max =       (1 << (offset_bits - 1));
    static const atomic_uint_t offset_limit =   (1 << offset_bits);
    static const atomic_uint_t version_limit =  (1 << version_bits);
    static const atomic_uint_t offset_mask =    (1 << offset_bits) - 1;
    static const atomic_uint_t version_mask =   (1 << version_bits) - 1;
    static const atomic_uint_t front_pack =     offset_mask << front_shift;
    static const atomic_uint_t back_pack =      offset_mask << back_shift;
    static const atomic_uint_t version_pack =   version_mask << version_shift;
    
    
    /* queue storage */
    
    atomic_item_t *vec;
    const atomic_uint_t size_limit;
    std::atomic<atomic_uint_t> offset_pack      __attribute__ ((aligned (64)));
    std::atomic<atomic_uint_t> counter  __attribute__ ((aligned (64)));
    
    
    /* queue helpers */
    
    static inline bool ispow2(size_t val) { return val && !(val & (val-1)); }
    
    static inline const atomic_uint_t pack_offsets(const atomic_uint_t version, const atomic_uint_t back, const atomic_uint_t front)
    {
        assert(version < version_limit);
        assert(back < offset_limit);
        assert(front < offset_limit);
        return (version << version_shift) | (back << back_shift) | (front << front_shift);
    }
    
    static inline bool unpack_offsets(const atomic_uint_t counter, const atomic_uint_t offset_pack,
                                      atomic_uint_t &last_version, atomic_uint_t &version, atomic_uint_t &back, atomic_uint_t &front)
    {
        last_version = counter & version_mask;
        version = (offset_pack & version_pack) >> version_shift;
        if (last_version == version) {
            back = (offset_pack & back_pack) >> back_shift;
            front = (offset_pack & front_pack) >> front_shift;
            return true;
        }
        return false;
    }
    
    atomic_uint_t _last_version() { return counter & version_mask; }
    atomic_uint_t _version() { return (offset_pack & version_pack) >> version_shift; }
    atomic_uint_t _back() { return (offset_pack & back_pack) >> back_shift; }
    atomic_uint_t _front() { return (offset_pack & front_pack) >> front_shift; }
    size_t capacity()               { return size_limit; }
    
    
    /* queue implementation */
    
    queue_atomic_v1(size_t size_limit) :
        size_limit(size_limit),
        offset_pack(pack_offsets(0, 0, size_limit)),
        counter(0)
    {
        static_assert(version_bits + offset_bits + offset_bits <= atomic_bits,
                      "version_bits + 2 * offset_bits must fit into atomic integer type");
        assert(size_limit > 0);
        assert(size_limit <= size_max);
        assert(ispow2(size_limit));
        vec = new atomic_item_t[size_limit]();
        assert(vec != nullptr);
    }
    
    virtual ~queue_atomic_v1()
    {
        delete [] vec;
    }
    
    bool empty()
    {
        atomic_uint_t _offset_pack = offset_pack.load(relaxed_memory_order);
        atomic_uint_t back = (_offset_pack & back_pack) >> back_shift;
        atomic_uint_t front = (_offset_pack & front_pack) >> front_shift;

        // return true if queue is empty
        return (front - back == size_limit);
    }
    
    bool full()
    {
        atomic_uint_t _offset_pack = offset_pack.load(relaxed_memory_order);
        atomic_uint_t back = (_offset_pack & back_pack) >> back_shift;
        atomic_uint_t front = (_offset_pack & front_pack) >> front_shift;
        
        // return true if queue is full
        return (front == back);
    }
    
    size_t size()
    {
        atomic_uint_t _offset_pack = offset_pack.load(relaxed_memory_order);
        atomic_uint_t back = (_offset_pack & back_pack) >> back_shift;
        atomic_uint_t front = (_offset_pack & front_pack) >> front_shift;

        // return queue size
        return (front < back) ? back - front : size_limit - front + back;
    }
    
    bool push_back(T elem)
    {
        int spin_count = 0;
        do {
            // if the versions are consistent then attempt push back
            atomic_uint_t last_version, version, back, front;
            atomic_uint_t _counter = counter.load(relaxed_memory_order);
            atomic_uint_t _offset_pack = offset_pack.load(relaxed_memory_order);
            if (unpack_offsets(_counter, _offset_pack, last_version, version, back, front))
            {
                // if (full) return false;
                if (front == back) return false;
                
                // increment version
                version = (last_version + 1) & version_mask;
                
                // calculate store offset and update back
                size_t offset = back++ & (size_limit - 1);
                
                // pack front, back and version
                atomic_uint_t pack = pack_offsets(version, back & (offset_limit - 1), front);
                
                // compare_exchange_weak version
                // if success then write value followed by offset_pack
                if (counter.compare_exchange_weak(last_version, version, std::memory_order_acq_rel)) {
                    vec[offset].store(elem, release_memory_order);
                    offset_pack.store(pack, release_memory_order);
                    return true;
                } else if (debug_contention) {
                    uint64_t _tsc = rdtsc();
                    log_debug("%s version=%llu time=%llu spin_count=%d thread:%p phase 2 contention",
                              __func__, _counter, _tsc, spin_count, std::this_thread::get_id());
                }
            } else {
                if (debug_contention) {
                    uint64_t _tsc = rdtsc();
                    log_debug("%s version=%llu time=%llu spin_count=%d thread:%p phase 1 contention",
                              __func__, _counter, _tsc, spin_count, std::this_thread::get_id());
                }
            }

            // yield the thread before retrying
            std::this_thread::yield();
            
        } while (++spin_count < spin_limit);
        
        if (debug_spin) {
            log_debug("%s thread:%p failed: reached spin limit", __func__, std::this_thread::get_id());
        }
        
        return false;
    }
    
    T pop_front()
    {
        int spin_count = 0;
        do {
            // if the versions are consistent then attempt pop front
            atomic_uint_t last_version, version, back, front;
            atomic_uint_t _counter = counter.load(relaxed_memory_order);
            atomic_uint_t _offset_pack = offset_pack.load(relaxed_memory_order);
            if (unpack_offsets(_counter, _offset_pack, last_version, version, back, front))
            {
                // if (empty) return nullptr;
                if (front - back == size_limit) return T(0);
                
                // increment version
                version = (last_version + 1) & version_mask;
                
                // calculate offset and update front
                size_t offset = front++ & (size_limit - 1);
                
                // pack front, back and version
                atomic_uint_t pack = pack_offsets(version, back, front & (offset_limit - 1));
                
                // compare_exchange_weak version
                // if success then write offset_pack
                if (counter.compare_exchange_weak(last_version, version, std::memory_order_acq_rel)) {
                    T val = vec[offset].load(acquire_memory_order);
                    offset_pack.store(pack, release_memory_order);
                    return val;
                } else if (debug_contention) {
                    uint64_t _tsc = rdtsc();
                    log_debug("%s version=%llu time=%llu spin_count=%d thread:%p phase 2 contention",
                              __func__, _counter, _tsc, spin_count, std::this_thread::get_id());
                }
            } else {
                if (debug_contention) {
                    uint64_t _tsc = rdtsc();
                    log_debug("%s version=%llu time=%llu spin_count=%d thread:%p phase 1 contention",
                              __func__, _counter, _tsc, spin_count, std::this_thread::get_id());
                }
            }

            // yield the thread before retrying
            std::this_thread::yield();
            
        } while (++spin_count < spin_limit);
        
        if (debug_spin) {
            log_debug("%s thread:%p failed: reached spin limit", __func__, std::this_thread::get_id());
        }
        
        return T(0);
    }
};

#endif
