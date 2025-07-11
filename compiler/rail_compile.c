#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

int newCall = 1;
char call[32];
int line = 1;
int inMain = 0;

int parseToken(char *token) {
    if (newCall) {
        newCall = 0;
        strcpy(call, token);
        if (strncmp(call, "main", 4) == 0) {
            inMain = 1;
        } else {
            if (!inMain) {
                printf("\033[31mRail compile failure: call outside of main at line %d\033[0m\n", line);
                return -1;
            } else if (call[0] == '#') {
                printf("\033[31mRail compile failure: invalid system call '%s' at line %d\033[0m\n", call, line);
                return -1;
            } else {
                printf("\033[31mRail compile failure: unknown call '%s' at line %d\033[0m\n", call, line);
                return -1;
            }
            
        }
    }
    if (token[strlen(token)-1] == ';' || token[strlen(token)-1] == ':') {
        newCall = 1;
    }
    return 0;
}

void compile(char *code) {
    char token[128];
    int tokenIdx = 0;
    int inString = 0;

    for (int c = 0; c < strlen(code); c++) {
        char ch = code[c];

        if (ch == '\n') line++;

        if (ch == '"') {
            inString = !inString;
            if (tokenIdx < (int)sizeof(token) - 1) {
                token[tokenIdx++] = ch;
            }
        } else if (isspace(ch) && !inString) {
            if (tokenIdx > 0) {
                token[tokenIdx] = '\0';
                if (parseToken(token) == -1) return;
                tokenIdx = 0;
            }
        } else {
            if (tokenIdx < (int)sizeof(token) - 1) {
                token[tokenIdx++] = ch;
            }
        }
    }
    
    if (tokenIdx > 0) {
        token[tokenIdx] = '\0';
        parseToken(token);
    }
}

int main() {
    printf("Rail Compiler for 6502\n");
    while (1) {
        newCall = 1;
        call[32];
        line = 1;
        inMain = 0;
        printf("RAIL>");

        char filename[500];

        fgets(filename, sizeof(filename), stdin);

        size_t len = strlen(filename);
        if (len > 0 && filename[len - 1] == '\n') {
            filename[len - 1] = '\0';
        }
        if (strcmp(&filename[len - 5], "rail") != 0) {
            printf("\033[31mFile open failure: '%s' is not a valid .rail file\033[0m\n", filename);
            continue;
        }

        FILE *file = fopen(filename, "r");
        if (file) {
            fseek(file, 0, SEEK_END);
            long file_size = ftell(file);
            rewind(file);

            char *content = malloc(file_size + 1);
            if (content) {
                size_t bytesRead = fread(content, 1, file_size, file);
                content[bytesRead] = '\0';
                fclose(file);
                printf("Starting build of file '%s'...\n", filename);
                compile(content);
                free(content);
            } else {
                printf("\033[31mFile open failure: memory allocation failure. Please try again\033[0m\n");
                fclose(file);
            }
        } else {
            printf("\033[31mFile open failure: file '%s' was not found\033[0m\n", filename);
        }
    }

    return 0;
}