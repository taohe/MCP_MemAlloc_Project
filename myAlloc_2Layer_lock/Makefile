CC = g++

all: 1test 2test 3test 4test 5test myAlloc.so


# myAlloc.so: heap_alloc.cpp heap_alloc.hpp
# 	$(CC) -c -g -fPIC heap_alloc.cpp
# 	g++ -g -shared -o myAlloc.so heap_alloc.o

myAlloc.so: thread_cache.cpp thread_cache.hpp heap_alloc.hpp heap_alloc.cpp
	$(CC) -c -g -fPIC thread_cache.cpp
	$(CC) -c -g -fPIC heap_alloc.cpp
	g++ -g -shared -o myAlloc.so heap_alloc.o thread_cache.o

1test: test1.cc myAlloc.so
	$(CC) -g -o 1test test1.cc myAlloc.so -lpthread

2test: test2.cc myAlloc.so
	$(CC) -g -o 2test test2.cc myAlloc.so -lpthread

3test: test3.cc myAlloc.so ticksClock.o
	$(CC) -g -o 3test test3.cc myAlloc.so ticksClock.o -lpthread

5test: test5.cc myAlloc.so
	$(CC) -g -o 5test test5.cc myAlloc.so

4test: test4.cpp myAlloc.so
	$(CC) -g -o 4test test4.cpp myAlloc.so -lpthread

ticksClock.o: ticks_clock.cpp ticks_clock.hpp logging.hpp log_message.hpp
	$(CC) -g -c ticks_clock.cpp -o ticksClock.o


1runtest: 1test
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:'pwd' && export LD_LIBRARY_PATH && \
	./1test

2runtest: 2test
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:'pwd' && export LD_LIBRARY_PATH && \
	./2test

3runtest: 3test
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:'pwd' && export LD_LIBRARY_PATH && \
	./3test

4runtest: 4test
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:'pwd' && export LD_LIBRARY_PATH && \
	./4test

clean:
	rm -f *.o 1test 2test 3test 4test 5test myAlloc.so
