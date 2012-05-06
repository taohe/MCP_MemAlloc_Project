#include <stdlib.h>
#include <stdio.h>

int main( int argc, char **argv ) {
  printf("\n---- Running test1 ---\n");

  int* p = (int*) malloc(520);
  *p = 10;
  free(p);

  p = (int*) malloc(610);
  *p = 33;
  free(p);

  p = (int*) malloc(120);
  *p = 33;
  free(p);
  
  printf(">>>> test1 passed\n\n");
  return 0;
}
