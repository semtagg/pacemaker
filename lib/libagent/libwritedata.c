#include <stdio.h>

int start(){
    FILE *f; 
    f = fopen("/var/log/test.txt", "w+");
    fprintf(f, "Test line. \n");
    fclose(f);
    return 1;
}