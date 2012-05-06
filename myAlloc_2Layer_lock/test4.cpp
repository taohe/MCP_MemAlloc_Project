/* Multi-bin test
 * Date: 05/05/2012 12:23 PM
 */
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define MEMORYALLOC (1UL << 20)
#define NUMOFRUNS 10

const int allocations = 1000;
static int MAX_BIN_ALLOC_SIZE (1UL << 10);
static int numBins = 0;

void allocationThread(void*) {
  // char** binArr = (char**) malloc (numBins * sizeof(char*));

  for (int j = 0; j < NUMOFRUNS; ++j) {
    for (int i = 0; i < numBins; ++i) {
      int allocsize = rand() % MAX_BIN_ALLOC_SIZE + 1;
      char* tmp = (char*) malloc(allocsize);
      free(tmp);
    }
  }

  // free(binArr);
  return;
}

extern "C" void atExitHandlerInC();

int main(int argc, char* argv[]) {
  if (argc < 2) {
    puts("Format: ./4test  #Of_Threads");
    return 1;
  }

   int numberOfThreads = atoi(argv[1]);
  //int numberOfThreads = 5;
  numBins = MEMORYALLOC / (MAX_BIN_ALLOC_SIZE * numberOfThreads);
  printf("---Running 3test---\nMAX_BIN_SIZE: %d #ofBins: %d\n", MAX_BIN_ALLOC_SIZE
      , numBins);
  void* psingle = malloc(20);

  pthread_t* t = new pthread_t[numberOfThreads];
  int i;
  for ( i = 0; i < numberOfThreads; i++ ) {
    pthread_create(t+i, NULL, (void* (*) (void *))allocationThread, NULL);
  }

  for (i = 0; i < numberOfThreads; i++ ) {
    printf("Wait for %lu\n", t[i]);
    pthread_join(t[i], NULL);
  }

  free(psingle);
  delete [] t;

  printf(">>>> test3 Finished\n\n");
  return 0;
}
