all: test_queue

clean:
	rm -f test_queue

test_queue: test_queue.cc queue_atomic.h
	c++ -pthread -O3 -std=c++11 $< -o $@
