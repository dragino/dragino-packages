#include <stdio.h>
#include "utilities.h"

struct test {
        char testsize[32];
        char hase[16];
        int who;
};

void pt(void *);

void main() {
        int i = 0;
        struct test test;
        struct test* how;
        
        how = &test;
        pt(how);

       // for (i = 0; i < 50; i++) {
        //        printf("output(%d): %ld\n", i, rand1());
        //}
        return;
}

void pt(void *data) {
        struct test* temp = (struct test*)data;
        printf("sizeof:%lu\n", sizeof(temp->hase));
}

