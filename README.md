# queue_atomic

Multiple producer multiple consumer c++11 atomic queue / ringbuffer

Solves the ABA problem by packing a monotonically increasing version number into the ring buffer offsets.

## Notes

### queue_std_vec_mutex_v1

 - std::mutex wrapper around std::vector

### queue_std_queue_mutex_v2

 - std::mutex wrapper around std::queue

### queue_atomic_v3

 - uses 3 atomic variables: version_counter, version_back and version_front
 - push_back reads 3 atomics: version_counter, version_back and version_front
        and writes 2 atomics: version_counter and version_back
 - pop_front reads 3 atomics: version_counter, version_back and version_front
        and writes 2 atomics: version_counter and version_front
 - version plus back or front are packed into version_back and version_front
 - version is used for conflict detection during ordered writes
 * NOTE: suffers cache line contention from concurrent reading and writing
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

### queue_atomic_v4

 - uses 2 atomic variables: version_counter and offset_pack
 - push_back reads 2 atomics: version_counter and offset_pack
         and writes 2 atomics: version_counter and offset_pack
 - pop_front reads 2 atomics: version_counter and offset_pack
         and writes 2 atomics: version_counter and offset_pack
 - front, back and version are packed into offset_pack
 - version is used for conflict detection during ordered writes
 - NOTE: suffers cache line contention from concurrent reading and writing
 - NOTE: limited to 8388608 items
````
queue_atomic_v4::is_lock_free  = 1
queue_atomic_v4::atomic_bits   = 64
queue_atomic_v4::offset_bits   = 24
queue_atomic_v4::version_bits  = 16
queue_atomic_v4::front_shift   = 0
queue_atomic_v4::back_shift    = 24
queue_atomic_v4::version_shift = 48
queue_atomic_v4::size_max      = 0x0000000000800000 (8388608)
queue_atomic_v4::offset_limit  = 0x0000000001000000 (16777216)
queue_atomic_v4::version_limit = 0x0000000000010000 (65536)
queue_atomic_v4::offset_mask   = 0x0000000000ffffff
queue_atomic_v4::version_mask  = 0x000000000000ffff
queue_atomic_v4::front_pack    = 0x0000000000ffffff
queue_atomic_v4::back_pack     = 0x0000ffffff000000
queue_atomic_v4::version_pack  = 0xffff000000000000
````

## Timings

### -O0, OS X 10.10, Apple LLVM version 6.1.0 (clang-602.0.49)

````
queue_implementation      threads iterations items/thread time(µs)    ops        op_time(µs)
queue_atomic_v4           8       10         1024         17483       81920      0.213416 
queue_atomic_v4           8       10         65536        970421      5242880    0.185093 
queue_atomic_v4           8       64         65536        6222348     33554432   0.185440 
queue_atomic_v4           8       16         262144       6254965     33554432   0.186412 
queue_atomic_v3           8       10         1024         17363       81920      0.211951 
queue_atomic_v3           8       10         65536        1053129     5242880    0.200868 
queue_atomic_v3           8       64         65536        6845973     33554432   0.204026 
queue_atomic_v3           8       16         262144       6855103     33554432   0.204298 
queue_std_queue_mutex_v2  8       10         1024         918224      81920      11.208789
queue_std_vec_mutex_v1    8       10         1024         956731      81920      11.678845
````

### -O3, OS X 10.10, Apple LLVM version 6.1.0 (clang-602.0.49)

````
queue_implementation      threads iterations items/thread time(µs)    ops        op_time(µs)
queue_atomic_v4           8       10         1024         4404        81920      0.053760 
queue_atomic_v4           8       10         65536        177312      5242880    0.033820 
queue_atomic_v4           8       64         65536        1141385     33554432   0.034016 
queue_atomic_v4           8       16         262144       1163573     33554432   0.034677 
queue_atomic_v3           8       10         1024         3729        81920      0.045520 
queue_atomic_v3           8       10         65536        188271      5242880    0.035910 
queue_atomic_v3           8       64         65536        1183945     33554432   0.035284 
queue_atomic_v3           8       16         262144       1191541     33554432   0.035511 
queue_std_queue_mutex_v2  8       10         1024         769476      81920      9.393018 
queue_std_vec_mutex_v1    8       10         1024         824315      81920      10.062439
````
