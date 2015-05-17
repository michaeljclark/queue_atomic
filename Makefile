all: test_queue

clean:
	rm -f test_queue

test_queue: test_queue.cc
	c++ -O3 -std=c++11 $^ -o $@
