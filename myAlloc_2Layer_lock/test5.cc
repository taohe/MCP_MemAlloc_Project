#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
  printf("\n---- Running test5 ---\n");

  int* a = (int*) malloc(100 * 1024 - 8);
  int* b = (int*) malloc(10 * 1024 - 8);
  int* c = (int*) malloc(50 * 1024 - 8);

  free(a);
  free(c);

  c = (int*) malloc(50 * 1024 - 8);
  a = (int*) malloc(100 * 1024 - 8);

  puts(">>>> test5 Finished");

  return 0;
}
