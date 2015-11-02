# queue_atomic

Multiple producer multiple consumer queue template using C++11 atomics.

Solves the ABA problem and implements 2-phase ordered updates by packing a monotonically increasing version number into the queue front and back offsets. The contended case is detected by checking that the expected version counter is visible in the packed front or back offset.

During an update the version counter is checked against the version packed in the offset, if the offset is up-to-date the version counter is atomically incremented, data is stored (push_back) or retrieved (pop_front) and in a final phase the front or back offset is atomically updated with a new version and offset. Data only becomes visible in another thread when the version counter matchs the version packed into the offsets. The front and back offsets always increase in the common case and buffer offsets are calculated modulus the queue size.

- queue_atomic is completely lockless in the single producer single consumer case
- queue_atomic can be used in multiple producer multiple consumer mode however it will spin calling std::this_thread::yield() when there is contention

## Notes

### queue_std_mutex

 - std::mutex wrapper around std::queue

### queue_atomic

- uses 4 atomic variables: counter_back, version_back, counter_front and version_front
- push_back reads 3 atomics: (counter_back, version_back and version_front)
       and writes 2 atomics: (counter_back and version_back)
- pop_front reads 3 atomics: (counter_front, version_back and version_front)
       and writes 2 atomics: (counter_front and version_front)
- uses two separate monotonically increasing version counters and 2-phase ordered updates
- completely lockless in the single producer single consumer case
- back version counter and back offset are packed into version_back
- front version counter and front offset are packed into version_front
* NOTE: limited to 140737488355328 (2^47) items
````
queue_atomic::is_lock_free  = 1
queue_atomic::atomic_bits   = 64
queue_atomic::offset_bits   = 48
queue_atomic::version_bits  = 16
queue_atomic::offset_shift  = 0
queue_atomic::version_shift = 48
queue_atomic::size_max      = 0x0000800000000000 (140737488355328)
queue_atomic::offset_limit  = 0x0001000000000000 (281474976710656)
queue_atomic::version_limit = 0x0000000000010000 (65536)
queue_atomic::offset_mask   = 0x0000ffffffffffff
queue_atomic::version_mask  = 0x000000000000ffff
````

## Timings

- -O3, OS X 10.10, Apple LLVM version 7.0.0, 22nm Ivy Bridge 2.7 GHz Intel Core i7

````
queue_implementation      threads iterations items/thread time(µs)    ops        op_time(µs)
queue_atomic              8       10         1024         4711        81920      0.057507
queue_atomic              8       10         65536        190221      5242880    0.036282
queue_atomic              8       64         65536        1225404     33554432   0.036520
queue_atomic              8       16         262144       1151575     33554432   0.034320
queue_std_mutex           8       10         1024         752439      81920      9.185046 
````

- -O3, Linux 4.2.0-amd64, GCC 5.2.1, 45nm Bloomfield 3.33GHZ GHz Intel Core i7 975

````
queue_implementation      threads iterations items/thread time(µs)    ops        op_time(µs)
queue_atomic              8       10         1024         8022        81920      0.097925
queue_atomic              8       10         65536        505085      5242880    0.096337
queue_atomic              8       64         65536        3182992     33554432   0.094861
queue_atomic              8       16         262144       3259350     33554432   0.097136
queue_std_mutex           8       10         1024         25139       81920      0.306873
````
