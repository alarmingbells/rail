#include <stdio.h>

int main() {
    char content[999];

    while (1) {
        char filename[500];
        fgets(filename, sizeof(filename), stdin);

        size_t len = strlen(filename);
        if (len > 0 && filename[len - 1] == '\n') {
            filename[len - 1] = '\0';
        }
        FILE *file = fopen(filename, "r");
        if (file) {
            fgets(content, 999, file);
            printf("%s", content);
            fclose(file);
        } else {
            printf("Could not open file: %s\n", filename);
        }
    }

    return 0;
}