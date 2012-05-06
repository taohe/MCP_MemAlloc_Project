#include <stdlib.h>
#include <stdio.h>

#define maxallocs 10000
int
main(){
char allocs[maxallocs];

for ( int i = 0; i < maxallocs; i++ ) {
        allocs[i] = malloc(i);
        malloc(1);
}

for ( int i = 0; i < maxallocs; i++ ) {
        free(allocs[i]);
}

for ( int i = maxallocs-1; i >= 0; i-- ) {
        allocs[i] = malloc(i);
}
}

