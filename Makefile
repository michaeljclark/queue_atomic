all: test_queue

clean:
	rm -f test_queue

test_queue: test_queue.cc
	c++ -std=c++11 $^ -o $@
