# queue_atomic

Multiple producer multiple consumer queue template using C++11 atomics.

Solves the ABA problem and implements 2-phase ordered updates by packing a monotonically increasing version number into the queue front and back offsets. The contended case is detected by checking that the expected version counter is visible in the packed front or back offset. During an update the version counter is checked against the version in the offset, if the offset is up-to-date the version counter is atomically incremented, data is stored (push_back) or retrieved (pop_front) and in a final phase the front or back offset is atomically updated with the new version and offset. Data only becomes visible in another thread when the version counter matchs the version packed into the offsets. The front and back offsets increase in the common case and buffer offsets are calculated modulus the queue size. The offset underflow wraparound special case is handled.

- queue_atomic_v3 is completely lockless in the single producer single consumer case
- all queues can be used in multiple producer multiple consumer mode however they will spin calling std::this_thread::yield() when there is contention

## Notes

### queue_std_mutex

 - std::mutex wrapper around std::queue

### queue_atomic_v1

- uses 2 atomic variables: counter and offset_pack
- push_back reads 2 atomics: (counter and offset_pack)
       and writes 2 atomics: (counter and offset_pack)
- pop_front reads 2 atomics: (counter and offset_pack)
       and writes 2 atomics: (counter and offset_pack)
- uses one monotonically increasing version counter and 2-phase ordered updates
- version counter, front offset and back offset are packed into offset_pack
- version counter is used for conflict detection during ordered writes
- NOTE: suffers cache line contention with concurrent push and pop
- NOTE: limited to 8388608 items
````
queue_atomic_v1::is_lock_free  = 1
queue_atomic_v1::atomic_bits   = 64
queue_atomic_v1::offset_bits   = 24
queue_atomic_v1::version_bits  = 16
queue_atomic_v1::front_shift   = 0
queue_atomic_v1::back_shift    = 24
queue_atomic_v1::version_shift = 48
queue_atomic_v1::size_max      = 0x0000000000800000 (8388608)
queue_atomic_v1::offset_limit  = 0x0000000001000000 (16777216)
queue_atomic_v1::version_limit = 0x0000000000010000 (65536)
queue_atomic_v1::offset_mask   = 0x0000000000ffffff
queue_atomic_v1::version_mask  = 0x000000000000ffff
queue_atomic_v1::front_pack    = 0x0000000000ffffff
queue_atomic_v1::back_pack     = 0x0000ffffff000000
queue_atomic_v1::version_pack  = 0xffff000000000000
````

### queue_atomic_v2

 - uses 3 atomic variables: counter, version_back and version_front
 - push_back reads 3 atomics: (counter, version_back and version_front)
        and writes 2 atomics: (counter and version_back)
 - pop_front reads 3 atomics: (counter, version_back and version_front)
        and writes 2 atomics: (counter and version_front)
 - uses one monotonically increasing version counter and 2-phase ordered updates
 - version counter and back offset are packed into version_back
 - version counter and front offset are packed into version_front
 - version counter is used for conflict detection during ordered writes
 * NOTE: suffers cache line contention with concurrent push and pop
 * NOTE: limited to 2147483648 items
````
queue_atomic_v2::is_lock_free  = 1
queue_atomic_v2::atomic_bits   = 64
queue_atomic_v2::offset_bits   = 32
queue_atomic_v2::version_bits  = 32
queue_atomic_v2::offset_shift  = 0
queue_atomic_v2::version_shift = 32
queue_atomic_v2::size_max      = 0x0000000080000000 (2147483648)
queue_atomic_v2::offset_limit  = 0x0000000100000000 (4294967296)
queue_atomic_v2::version_limit = 0x0000000100000000 (4294967296)
queue_atomic_v2::offset_mask   = 0x00000000ffffffff
queue_atomic_v2::version_mask  = 0x00000000ffffffff
````

### queue_atomic_v3

- uses 4 atomic variables: counter_back, version_back, counter_front and version_front
- push_back reads 3 atomics: (counter_back, version_back and version_front)
       and writes 2 atomics: (counter_back and version_back)
- pop_front reads 3 atomics: (counter_front, version_back and version_front)
       and writes 2 atomics: (counter_front and version_front)
- uses two separate monotonically increasing version counters and 2-phase ordered updates
- completely lockless in the single producer single consumer case
- back version counter and back offset are packed into version_back
- front version counter and front offset are packed into version_front
* NOTE: limited to 2147483648 items
````
queue_atomic_v3::is_lock_free  = 1
queue_atomic_v3::atomic_bits   = 64
queue_atomic_v3::offset_bits   = 32
queue_atomic_v3::version_bits  = 32
queue_atomic_v3::offset_shift  = 0
queue_atomic_v3::version_shift = 32
queue_atomic_v3::size_max      = 0x0000000080000000 (2147483648)
queue_atomic_v3::offset_limit  = 0x0000000100000000 (4294967296)
queue_atomic_v3::version_limit = 0x0000000100000000 (4294967296)
queue_atomic_v3::offset_mask   = 0x00000000ffffffff
queue_atomic_v3::version_mask  = 0x00000000ffffffff
````

## Timings

- -O3, OS X 10.10, Apple LLVM version 7.0.0, 22nm Ivy Bridge 2.7 GHz Intel Core i7

````
queue_implementation      threads iterations items/thread time(µs)    ops        op_time(µs)
queue_atomic_v1           8       10         1024         4404        81920      0.053760 
queue_atomic_v1           8       10         65536        177312      5242880    0.033820 
queue_atomic_v1           8       64         65536        1141385     33554432   0.034016 
queue_atomic_v1           8       16         262144       1163573     33554432   0.034677 
queue_atomic_v2           8       10         1024         3729        81920      0.045520 
queue_atomic_v2           8       10         65536        188271      5242880    0.035910 
queue_atomic_v2           8       64         65536        1183945     33554432   0.035284 
queue_atomic_v2           8       16         262144       1191541     33554432   0.035511
queue_atomic_v3           8       10         1024         4711        81920      0.057507
queue_atomic_v3           8       10         65536        190221      5242880    0.036282
queue_atomic_v3           8       64         65536        1225404     33554432   0.036520
queue_atomic_v3           8       16         262144       1151575     33554432   0.034320
queue_std_mutex           8       10         1024         752439      81920      9.185046 
````

- -O3, Linux 4.2.0-amd64, GCC 5.2.1, 45nm Bloomfield 3.33GHZ GHz Intel Core i7 975

````
queue_implementation      threads iterations items/thread time(µs)    ops        op_time(µs)
queue_atomic_v1           8       10         1024         11524       81920      0.140674
queue_atomic_v1           8       10         65536        758910      5242880    0.144751
queue_atomic_v1           8       64         65536        4870627     33554432   0.145156
queue_atomic_v1           8       16         262144       4901671     33554432   0.146081
queue_atomic_v2           8       10         1024         12209       81920      0.149036
queue_atomic_v2           8       10         65536        788602      5242880    0.150414
queue_atomic_v2           8       64         65536        5042567     33554432   0.150280
queue_atomic_v2           8       16         262144       5083757     33554432   0.151508
queue_atomic_v3           8       10         1024         8022        81920      0.097925
queue_atomic_v3           8       10         65536        505085      5242880    0.096337
queue_atomic_v3           8       64         65536        3182992     33554432   0.094861
queue_atomic_v3           8       16         262144       3259350     33554432   0.097136
queue_std_mutex           8       10         1024         25139       81920      0.306873
````
