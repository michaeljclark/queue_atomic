//
//  queue_std_mutex.h
//

#ifndef queue_std_mutex_h
#define queue_std_mutex_h

/*
 * queue_std_mutex
 *
 *   - uses mutex protection around queue
 */

template <typename T>
struct queue_std_mutex
{
    typedef std::queue<T>                       queue_type;
    typedef std::atomic<T>                      atomic_item_t;
    
    queue_type                                  queue;
    std::mutex                                  queue_mutex;
    
    queue_std_mutex(size_t size_limit) {}
    
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

#endif
