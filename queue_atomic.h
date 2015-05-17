//
//  queue_atomic.h
//

#ifndef queue_atomic_h
#define queue_atomic_h

/*
 * queue_std_vec_mutex_v1
 *
 *   - uses mutex protection around vector
 */

template <typename T>
struct queue_std_vec_mutex_v1
{
    typedef std::vector<T>                      vec_type;
    typedef std::atomic<T>                      atomic_item_t;
    
    vec_type                                    queue;
    std::mutex                                  queue_mutex;
    
    queue_std_vec_mutex_v1(size_t size_limit)
    {
        queue.reserve(size_limit);
    }
    
    size_t size()
    {
        size_t size;
        queue_mutex.lock();
        size = queue.size();
        queue_mutex.unlock();
        return size;
    }
    
    bool push_back(T elem)
    {
        queue_mutex.lock();
        queue.push_back(elem);
        queue_mutex.unlock();
        return true;
    }
    
    T pop_front()
    {
        queue_mutex.lock();
        T result(0);
        if (queue.size() > 0) {
            result = queue.back();
            queue.pop_back();
        }
        queue_mutex.unlock();
        return result;
    }
};

/*
 * queue_std_queue_mutex_v2
 *
 *   - uses mutex protection around queue
 */

template <typename T>
struct queue_std_queue_mutex_v2
{
    typedef std::queue<T>                       queue_type;
    typedef std::atomic<T>                      atomic_item_t;
    
    queue_type                                  queue;
    std::mutex                                  queue_mutex;
    
    queue_std_queue_mutex_v2(size_t size_limit) {}
    
    size_t size()
    {
        size_t size;
        queue_mutex.lock();
        size = queue.size();
        queue_mutex.unlock();
        return size;
    }
    
    bool push_back(T elem)
    {
        queue_mutex.lock();
        queue.push(elem);
        queue_mutex.unlock();
        return true;
    }
    
    T pop_front()
    {
        queue_mutex.lock();
        T result(0);
        if (queue.size() > 0) {
            result = queue.front();
            queue.pop();
        }
        queue_mutex.unlock();
        return result;
    }
};


/*
 * queue_atomic_v3
 *
 *   - uses 3 atomic variables: version_counter, version_back and version_front
 *
 *   - push_back reads 3 atomics: version_counter, version_back and version_front
 *               writes 2 atomics: version_counter and version_back
 *
 *   - pop_front reads 3 atomics: version_counter, version_back and version_front
 *               writes 2 atomics: version_counter and version_front
 *
 *   - version plus back or front are packed into version_back and version_front
 *
 *   - version is used for conflict detection during ordered writes
 *
 *   * NOTE: suffers cache line contention from concurrent reading and writing
 *   * NOTE: std::memory_order_consume faster than std::memory_order_acquire with g++
 *   * NOTE: std::memory_order_acquire faster than std::memory_order_consume with clang++
 *   * SEE: http://preshing.com/20141124/fixing-gccs-implementation-of-memory_order_consume/
 */

template <typename T,
          typename ATOMIC_UINT = uint64_t,
          const int OFFSET_BITS = 32,
          const int VERSION_BITS = 32,
          std::memory_order load_memory_order = std::memory_order_relaxed,
          std::memory_order consume_memory_order = std::memory_order_consume,
          std::memory_order release_memory_order = std::memory_order_release>
struct queue_atomic_v3
{
    /* queue atomic type */
    
    typedef ATOMIC_UINT                         atomic_uint_t;
    typedef std::atomic<T>                      atomic_item_t;
    
    
    /* queue constants */
    
    static const int spin_limit =               65536;
    static const int debug_spin =               true;
    static const int atomic_bits =              sizeof(atomic_uint_t) << 3;
    static const int offset_bits =              OFFSET_BITS;
    static const int version_bits =             VERSION_BITS;
    static const int offset_shift =             0;
    static const int version_shift =            offset_bits;
    static const atomic_uint_t size_max =       (1ULL << (offset_bits - 1));
    static const atomic_uint_t offset_limit =   (1ULL << offset_bits);
    static const atomic_uint_t version_limit =  (1ULL << version_bits);
    static const atomic_uint_t offset_mask =    (1ULL << offset_bits) - 1;
    static const atomic_uint_t version_mask =   (1ULL << version_bits) - 1;
    
    
    /* queue storage */
    
    atomic_item_t *vec;
    const atomic_uint_t size_limit;
    std::atomic<atomic_uint_t> version_counter __attribute__ ((aligned (64)));
    std::atomic<atomic_uint_t> version_back;
    std::atomic<atomic_uint_t> version_front;
    
    
    /* queue helpers */
    
    static inline bool ispow2(size_t val) { return val && !(val & (val-1)); }
    
    static inline const atomic_uint_t pack_offset(const atomic_uint_t version, const atomic_uint_t offset)
    {
        assert(version < version_limit);
        assert(offset < offset_limit);
        return (version << version_shift) | (offset << offset_shift);
    }

    static inline bool unpack_offsets(const atomic_uint_t version_counter, const atomic_uint_t back_pack, const atomic_uint_t front_pack,
                                      atomic_uint_t &last_version, atomic_uint_t &back_version, atomic_uint_t &front_version,
                                      atomic_uint_t &back_offset, atomic_uint_t &front_offset)
    {
        last_version = version_counter & version_mask;
        back_version = (back_pack >> version_shift) & version_mask;
        front_version = (front_pack >> version_shift) & version_mask;
        
        if (back_version == last_version || front_version == last_version) {
            back_offset = (back_pack >> offset_shift) & offset_mask;
            front_offset = (front_pack >> offset_shift) & offset_mask;
            return true;
        }
        return false;
    }

    
    /* queue implementation */

    atomic_uint_t _last_version()   { return version_counter & version_mask; }
    atomic_uint_t _back_version()   { return (version_back >> version_shift) & version_mask; }
    atomic_uint_t _front_version()  { return (version_front >> version_shift) & version_mask; }
    atomic_uint_t _back()           { return (version_back >> offset_shift) & offset_mask; }
    atomic_uint_t _front()          { return (version_front >> offset_shift) & offset_mask; }
    size_t capacity()               { return size_limit; }
    

    queue_atomic_v3(size_t size_limit) :
        size_limit(size_limit),
        version_counter(0),
        version_back(pack_offset(0, 0)),
        version_front(pack_offset(0, size_limit))
    {
        static_assert(version_bits + offset_bits <= atomic_bits,
                      "version_bits + offset_bits must fit into atomic integer type");
        assert(size_limit > 0);
        assert(size_limit <= size_max);
        assert(ispow2(size_limit));
        vec = new atomic_item_t[size_limit]();
        assert(vec != nullptr);
    }
    
    virtual ~queue_atomic_v3()
    {
        delete vec;
    }
    
    bool empty()
    {
        int spin_count = spin_limit;
        do {
            // if front_version or back_version equals last_version then return result
            atomic_uint_t last_version, back_version, front_version, back, front;
            atomic_uint_t _version_counter = version_counter.load(load_memory_order);
            atomic_uint_t _version_back = version_back.load(load_memory_order);
            atomic_uint_t _version_front = version_front.load(load_memory_order);
            if (unpack_offsets(_version_counter, _version_back, _version_front,
                               last_version, back_version, front_version, back, front))
            {
                // return true if queue is empty
                return (front - back == size_limit);
            }
            
            // yield the thread before retrying
            std::this_thread::yield();
            
        } while (--spin_count > 0);
        
        if (debug_spin) {
            log_debug("%s thread:%p failed: reached spin limit", __func__, std::this_thread::get_id());
        }
        
        return false;
    }
    
    bool full()
    {
        int spin_count = spin_limit;
        do {
            // if front_version or back_version equals last_version then return result
            atomic_uint_t last_version, back_version, front_version, back, front;
            atomic_uint_t _version_counter = version_counter.load(load_memory_order);
            atomic_uint_t _version_back = version_back.load(load_memory_order);
            atomic_uint_t _version_front = version_front.load(load_memory_order);
            if (unpack_offsets(_version_counter, _version_back, _version_front,
                               last_version, back_version, front_version, back, front))
            {
                // return true if queue is full
                return (front == back);
            }
            
            // yield the thread before retrying
            std::this_thread::yield();
            
        } while (--spin_count > 0);
        
        if (debug_spin) {
            log_debug("%s thread:%p failed: reached spin limit", __func__, std::this_thread::get_id());
        }
        
        return false;
    }
    
    size_t size()
    {
        int spin_count = spin_limit;
        do {
            // if front_version or back_version equals last_version then return result
            atomic_uint_t last_version, back_version, front_version, back, front;
            atomic_uint_t _version_counter = version_counter.load(load_memory_order);
            atomic_uint_t _version_back = version_back.load(load_memory_order);
            atomic_uint_t _version_front = version_front.load(load_memory_order);
            if (unpack_offsets(_version_counter, _version_back, _version_front,
                               last_version, back_version, front_version, back, front))
            {
                // return queue size
                return (front < back) ? back - front : size_limit - front + back;
            }
            
            // yield the thread before retrying
            std::this_thread::yield();
            
        } while (--spin_count > 0);
        
        if (debug_spin) {
            log_debug("%s thread:%p failed: reached spin limit", __func__, std::this_thread::get_id());
        }
        
        return 0;
    }
    
    bool push_back(T elem)
    {
        int spin_count = spin_limit;
        do {
            // if front_version or back_version equals last_version then attempt push back
            atomic_uint_t last_version, back_version, front_version, back, front;
            atomic_uint_t _version_counter = version_counter.load(load_memory_order);
            atomic_uint_t _version_back = version_back.load(load_memory_order);
            atomic_uint_t _version_front = version_front.load(load_memory_order);
            if (unpack_offsets(_version_counter, _version_back, _version_front,
                               last_version, back_version, front_version, back, front))
            {
                // if (full) return false;
                if (front == back) return false;
                
                // increment back_version
                back_version = (last_version + 1) & version_mask;

                // calculate store offset and update back
                size_t offset = back++ & (size_limit - 1);
                
                // pack back_version and back
                atomic_uint_t pack = pack_offset(back_version, back & (offset_limit - 1));

                // compare_exchange_weak to attempt to update the version_counter
                // if successful other threads will spin until new version_back is visible
                // if successful then write value followed by version_back
                if (version_counter.compare_exchange_weak(last_version, back_version)) {
                    vec[offset].store(elem, release_memory_order);
                    version_back.store(pack, release_memory_order);
                    return true;
                }
            }
            
            // yield the thread before retrying
            std::this_thread::yield();
            
        } while (--spin_count > 0);
        
        if (debug_spin) {
            log_debug("%s thread:%p failed: reached spin limit", __func__, std::this_thread::get_id());
        }
        
        return false;
    }
    
    T pop_front()
    {
        int spin_count = spin_limit;
        do {
            // if front_version or back_version equals last_version then attempt pop front
            atomic_uint_t last_version, back_version, front_version, back, front;
            atomic_uint_t _version_counter = version_counter.load(load_memory_order);
            atomic_uint_t _version_back = version_back.load(load_memory_order);
            atomic_uint_t _version_front = version_front.load(load_memory_order);
            if (unpack_offsets(_version_counter, _version_back, _version_front,
                               last_version, back_version, front_version, back, front))
            {
                // if (empty) return nullptr;
                if (front - back == size_limit) return T(0);

                // increment front_version
                front_version = (last_version + 1) & version_mask;

                // calculate offset and update front
                size_t offset = front++ & (size_limit - 1);

                // pack front_version and front
                atomic_uint_t pack = pack_offset(front_version, front & (offset_limit - 1));

                // compare_exchange_weak to attempt to update the version_counter
                // if successful other threads will spin until new version_front is visible
                // if successful then write version_front
                if (version_counter.compare_exchange_weak(last_version, front_version)) {
                    T val = vec[offset].load(consume_memory_order);
                    version_front.store(pack, release_memory_order);
                    return val;
                }
            }

            // yield the thread before retrying
            std::this_thread::yield();
            
        } while (--spin_count > 0);
        
        if (debug_spin) {
            log_debug("%s thread:%p failed: reached spin limit", __func__, std::this_thread::get_id());
        }
        
        return T(0);
    }
};

/*
 * queue_atomic_v4
 *
 *   - uses 2 atomic variables: version_counter and offset_pack
 *
 *   - push_back reads 2 atomics: version_counter and offset_pack
 *               writes 2 atomics: version_counter and offset_pack
 *
 *   - pop_front reads 2 atomics: version_counter and offset_pack
 *               writes 2 atomics: version_counter and offset_pack
 *
 *   - front, back and version are packed into offset_pack
 *
 *   - version is used for conflict detection during ordered writes
 *
 *   * NOTE: suffers cache line contention from concurrent reading and writing
 *   * NOTE: std::memory_order_consume faster than std::memory_order_acquire with g++
 *   * NOTE: std::memory_order_acquire faster than std::memory_order_consume with clang++
 *   * SEE: http://preshing.com/20141124/fixing-gccs-implementation-of-memory_order_consume/
 *
 */

template <typename T,
          typename ATOMIC_UINT = uint64_t,
          const int OFFSET_BITS = 24,
          const int VERSION_BITS = 16,
          std::memory_order load_memory_order = std::memory_order_relaxed,
          std::memory_order consume_memory_order = std::memory_order_consume,
          std::memory_order release_memory_order = std::memory_order_release>
struct queue_atomic_v4
{
    /* queue atomic type */
    
    typedef ATOMIC_UINT                         atomic_uint_t;
    typedef std::atomic<T>                      atomic_item_t;
    
    
    /* queue constants */
    
    static const int spin_limit =               65536;
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
    std::atomic<atomic_uint_t> version_counter;
    
    
    /* queue helpers */
    
    static inline bool ispow2(size_t val) { return val && !(val & (val-1)); }
    
    static inline const atomic_uint_t pack_offsets(const atomic_uint_t version, const atomic_uint_t back, const atomic_uint_t front)
    {
        assert(version < version_limit);
        assert(back < offset_limit);
        assert(front < offset_limit);
        return (version << version_shift) | (back << back_shift) | (front << front_shift);
    }
    
    static inline bool unpack_offsets(const atomic_uint_t version_counter, const atomic_uint_t offset_pack,
                                      atomic_uint_t &last_version, atomic_uint_t &version, atomic_uint_t &back, atomic_uint_t &front)
    {
        last_version = version_counter & version_mask;
        version = (offset_pack & version_pack) >> version_shift;
        if (last_version == version) {
            back = (offset_pack & back_pack) >> back_shift;
            front = (offset_pack & front_pack) >> front_shift;
            return true;
        }
        return false;
    }
    
    atomic_uint_t _last_version() { return version_counter & version_mask; }
    atomic_uint_t _version() { return (offset_pack & version_pack) >> version_shift; }
    atomic_uint_t _back() { return (offset_pack & back_pack) >> back_shift; }
    atomic_uint_t _front() { return (offset_pack & front_pack) >> front_shift; }
    size_t capacity()               { return size_limit; }
    
    
    /* queue implementation */
    
    queue_atomic_v4(size_t size_limit) :
        size_limit(size_limit),
        offset_pack(pack_offsets(0, 0, size_limit)),
        version_counter(0)
    {
        static_assert(version_bits + offset_bits + offset_bits <= atomic_bits,
                      "version_bits + 2 * offset_bits must fit into atomic integer type");
        assert(size_limit > 0);
        assert(size_limit <= size_max);
        assert(ispow2(size_limit));
        vec = new atomic_item_t[size_limit]();
        assert(vec != nullptr);
    }
    
    virtual ~queue_atomic_v4()
    {
        delete vec;
    }
    
    bool empty()
    {
        int spin_count = spin_limit;
        do {
            // if the versions are consistent then return result
            atomic_uint_t last_version, version, back, front;
            atomic_uint_t _version_counter = version_counter.load(load_memory_order);
            atomic_uint_t _offset_pack = offset_pack.load(load_memory_order);
            if (unpack_offsets(_version_counter, _offset_pack, last_version, version, back, front))
            {
                // return true if queue is empty
                return (front - back == size_limit);
            }
            
            // yield the thread before retrying
            std::this_thread::yield();
            
        } while (--spin_count > 0);
        
        if (debug_spin) {
            log_debug("%s thread:%p failed: reached spin limit", __func__, std::this_thread::get_id());
        }
        
        return false;
    }
    
    bool full()
    {
        int spin_count = spin_limit;
        do {
            // if the versions are consistent then return result
            atomic_uint_t last_version, version, back, front;
            atomic_uint_t _version_counter = version_counter.load(load_memory_order);
            atomic_uint_t _offset_pack = offset_pack.load(load_memory_order);
            if (unpack_offsets(_version_counter, _offset_pack, last_version, version, back, front))
            {
                // return true if queue is full
                return (front == back);
            }
            
            // yield the thread before retrying
            std::this_thread::yield();
            
        } while (--spin_count > 0);
        
        if (debug_spin) {
            log_debug("%s thread:%p failed: reached spin limit", __func__, std::this_thread::get_id());
        }
        
        return false;
    }
    
    size_t size()
    {
        int spin_count = spin_limit;
        do {
            // if the versions are consistent then return result
            atomic_uint_t last_version, version, back, front;
            atomic_uint_t _version_counter = version_counter.load(load_memory_order);
            atomic_uint_t _offset_pack = offset_pack.load(load_memory_order);
            if (unpack_offsets(_version_counter, _offset_pack, last_version, version, back, front))
            {
                // return queue size
                return (front < back) ? back - front : size_limit - front + back;
            }
            
            // yield the thread before retrying
            std::this_thread::yield();
            
        } while (--spin_count > 0);
        
        if (debug_spin) {
            log_debug("%s thread:%p failed: reached spin limit", __func__, std::this_thread::get_id());
        }
        
        return 0;
    }
    
    bool push_back(T elem)
    {
        int spin_count = spin_limit;
        do {
            // if the versions are consistent then attempt push back
            atomic_uint_t last_version, version, back, front;
            atomic_uint_t _version_counter = version_counter.load(load_memory_order);
            atomic_uint_t _offset_pack = offset_pack.load(load_memory_order);
            if (unpack_offsets(_version_counter, _offset_pack, last_version, version, back, front))
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
                if (version_counter.compare_exchange_weak(last_version, version)) {
                    vec[offset].store(elem, release_memory_order);
                    offset_pack.store(pack, release_memory_order);
                    return true;
                }
            }
            
            // yield the thread before retrying
            std::this_thread::yield();
            
        } while (--spin_count > 0);
        
        if (debug_spin) {
            log_debug("%s thread:%p failed: reached spin limit", __func__, std::this_thread::get_id());
        }
        
        return false;
    }
    
    T pop_front()
    {
        int spin_count = spin_limit;
        do {
            // if the versions are consistent then attempt pop front
            atomic_uint_t last_version, version, back, front;
            atomic_uint_t _version_counter = version_counter.load(load_memory_order);
            atomic_uint_t _offset_pack = offset_pack.load(load_memory_order);
            if (unpack_offsets(_version_counter, _offset_pack, last_version, version, back, front))
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
                if (version_counter.compare_exchange_weak(last_version, version)) {
                    T val = vec[offset].load(consume_memory_order);
                    offset_pack.store(pack, release_memory_order);
                    return val;
                }
            }
            
            // yield the thread before retrying
            std::this_thread::yield();
            
        } while (--spin_count > 0);
        
        if (debug_spin) {
            log_debug("%s thread:%p failed: reached spin limit", __func__, std::this_thread::get_id());
        }
        
        return T(0);
    }
};


#endif
