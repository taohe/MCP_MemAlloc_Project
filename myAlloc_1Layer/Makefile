CC = g++

all: 1test 2test 3test 4test 5test MyMalloc.so


MyMalloc.so: MyMalloc.cpp MyMalloc.hpp
	$(CC) -c -g -fPIC MyMalloc.cpp
	g++ -g -shared -o MyMalloc.so MyMalloc.o

1test: test1.cc MyMalloc.so
	$(CC) -g -o 1test test1.cc MyMalloc.so

2test: test2.cc MyMalloc.so
	$(CC) -g -o 2test test2.cc MyMalloc.so

3test: test3.cc MyMalloc.so ticksClock.o
	$(CC) -g -o 3test test3.cc MyMalloc.so ticksClock.o -lpthread

5test: test5.cc MyMalloc.so
	$(CC) -g -o 5test test5.cc MyMalloc.so

4test: test4.cpp MyMalloc.so
	$(CC) -g -o 4test test4.cpp MyMalloc.so -lpthread

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
	rm -f *.o 1test 2test 3test 4test 5test MyMalloc.so
