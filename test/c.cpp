#include <stdio.h>

int main(){
    char p[5];
    char*& ptr = *p;
    int* k = (int*)(p+1);
    *k = 12;
    printf("%d\n", *(int*)(p+1));
    return 0;
}