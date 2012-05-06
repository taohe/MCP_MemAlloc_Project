#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int allocations = 2000;

int main( int argc, char **argv ) {
  srand(time(NULL));
  printf("\n---- Running test2 ---\n");
  for (int i = 1; i < allocations; ++i) {
    int allocsize = rand() % i + 1;
    char* p1 = (char*) malloc (allocsize);
    *p1 = 100;
    char* p2 = (char*) malloc (100);
    free(p1);
    free(p2);
  }
  char * p3 = (char*) malloc(100000);
  *p3 = 0;
  free(p3);
  printf(">>>> test2 Finished\n\n");
  exit( 0 );
}
