#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "ticks_clock.hpp"

using base::TicksClock;

#define MAXMEMORYALLOC (1UL << 23)
#define MAXALLOCSIZE (1UL << 10)
#define NUMOFTESTS 30

static int numAllocPerThrd = 0;

void allocationThread(void*) {
  for (int j = 0; j < 10; j++) {
    for (int i = 1; i < numAllocPerThrd; i++) {
      int allocsize = rand() % MAXALLOCSIZE + 1;
      char* p1 = (char*) malloc(allocsize);
      *p1 = 100;
      char* p2 = (char*) malloc(100);
      *p2 = 222;
      free(p1);
      free(p2);
    }
  }
  return;
}

extern "C" void atExitHandlerInC();

int main(int argc, char* argv[]) {
  int numberOfThreads = 12;
  void* psingle = malloc(20);
  TicksClock::Ticks avgticks = 0;

  pthread_t* t = new pthread_t[numberOfThreads];
  numAllocPerThrd = MAXMEMORYALLOC / (numberOfThreads * MAXALLOCSIZE);
  printf("\n---- Running test3 ---numAllocPerThrd: %d numThreads: %d\n",
      numAllocPerThrd, numberOfThreads);

  int i;
  for (int j = 0; j < NUMOFTESTS; ++j) {
    TicksClock::Ticks diff = TicksClock::getTicks();
    for (i = 0; i < numberOfThreads; ++i) {
      pthread_create(t+i, NULL, (void* (*) (void *))allocationThread, NULL);
    }
    for (i = 0; i < numberOfThreads; ++i) {
      // printf("Wait for %lu\n", t[i]);
      pthread_join(t[i], NULL);
    }
    diff = TicksClock::getTicks() - diff;
    avgticks += diff;
  }

  free(psingle);
  delete [] t;

  printf(">>>> test3 Finished\n\n");
  std::cout << "Avg Time elapsed: " << avgticks / NUMOFTESTS << std::endl;
  return 0;
}
