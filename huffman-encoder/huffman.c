#include <stdio.h>
#define true 1
#define false 0

int process_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file) {
        int c;
        while ((c = fgetc(file)) != EOF) {
            printf("%c", c);
        }
        fclose(file);
        return true;
    }
    return false;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    char* filename = argv[1];

    if (process_file(filename)) {
        printf("exists! %s", filename);
    }
    else {
        printf("FIle does not exist: %s", filename);
    }
    return 0;
}
