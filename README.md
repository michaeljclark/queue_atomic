# queue_atomic

c++11 atomic queue / ringbuffer

### -O0 timings (OS X 10.10) Apple LLVM version 6.1.0 (clang-602.0.49)

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

### -O3 timings (OS X 10.10) Apple LLVM version 6.1.0 (clang-602.0.49)

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
