#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

int newCall = 1;
char call[32];
int line = 0;
int inMain = 0;
int inFunc = 0;

int currentVarIdx = 0;

int argCount = 0;
int buffer = 0;

#define MAX_VARS 128

typedef struct {
    char name[32];
    int address;
} Variable;

Variable vars[MAX_VARS];
int var_count = 0;
int next_heap_addr = 0x0200;

int add_variable(const char *name) {
    if (var_count >= MAX_VARS) return -1;
    strncpy(vars[var_count].name, name, sizeof(vars[var_count].name) - 1);
    vars[var_count].name[sizeof(vars[var_count].name) - 1] = '\0';
    vars[var_count].address = next_heap_addr++;
    return var_count++;
}

int find_variable(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0) return i;
    }
    return -1;
}

const char *rxlib[] = {
    "print",
    "A9 01 AE TP TP A0 TP 20 00 FF ", // pointer to string, length of string
    "input",
    "A9 02 20 00 FF"
};

int parseToken(char *token) {
    if (newCall) {
        newCall = 0;

        size_t len = strlen(token);
        if (len > 0 && token[len - 1] == ';') {
            strncpy(call, token, len - 1);
            call[len - 1] = '\0';
        } else {
            strcpy(call, token);
        }

        if (strncmp(call, "main", 4) == 0) {
            inMain = 1;
        } else {
            if (!inMain) {
                if (strcmp(call, "function") == 0) {
                    inFunc = 1;
                }
                printf("\033[31mRail compile failure: call outside of main or function at line %d\033[0m\n", line);
                return -1;
            } else if (call[0] == '#') {
                int found = -1;
                for (size_t i = 0; i < sizeof(rxlib) / sizeof(rxlib[0]); i++) {
                    if (strcmp(call + 1, rxlib[i]) == 0) {
                        found = i;
                        break;
                    }
                }
                if (found != -1) {
                    printf("%s", rxlib[found+1]);
                } else {
                    printf("\n\033[31mRail compile failure: unknown system call '%s' at line %d\033[0m\n", call, line);
                    return -1;
                }
            } else if (strcmp(call, "end") == 0) {
            } else {
                if ((strcmp(call, "global") == 0)) {
                } else if ((strcmp(call, "assign") == 0)) {
                } else {
                    printf("\n\033[31mRail compile failure: unknown function or keyword '%s' at line %d\033[0m\n", call, line);
                    return -1;
                }                
            }
        }
    } else {
        argCount++;

        char stoken[128];

        size_t len = strlen(token);
        if (len > 0 && token[len - 1] == ';') {
            strncpy(stoken, token, len - 1);
            stoken[len - 1] = '\0';
        } else {
            strcpy(stoken, token);
        }

        if (strcmp(call, "assign") == 0) {
            if (argCount == 1) {
                currentVarIdx = find_variable(stoken);
            } else if (argCount == 2) {
                char *endptr;
                buffer = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    printf("\n\033[31mRail compile failure: expected integer value for 'assign', got '%s' at line %d\033[0m\n", stoken, line);
                    return -1;
                }

                printf("A9 %02X 8D ", buffer);
                int addr = vars[find_variable(stoken)].address;
                printf("%02X %02X ", vars[currentVarIdx].address & 0xFF, (vars[currentVarIdx].address >> 8) & 0xFF);
            } else {
                printf("\n\033[31mRail compile failure: incorrect number of arguments for '%s' at line %d\033[0m\n", call, line);
                return -1;
            }
        } else if (strcmp(call, "global") == 0) {
            if (argCount == 1) {
                if (find_variable(stoken) == -1) {
                    currentVarIdx = add_variable(stoken);
                } else {
                    printf("\n\033[31mRail compile failure: global name '%s' already in use at line %d\033[0m\n", stoken, line);
                    return -1;
                }
            } else if (argCount == 2) {
                char *endptr;
                int value = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    printf("\n\033[31mRail compile failure: expected integer value for 'global', got '%s' at line %d\033[0m\n", stoken, line);
                    return -1;
                }
                printf("A9 %02X 8D ", value);
                printf("%02X %02X ", vars[currentVarIdx].address & 0xFF, (vars[currentVarIdx].address >> 8) & 0xFF);
            } else {
                printf("\n\033[31mRail compile failure: incorrect number of arguments for '%s' at line %d\033[0m\n", call, line);
                return -1;
            }
        }
    }
    if (token[strlen(token)-1] == ';' || token[strlen(token)-1] == ':') {
        newCall = 1;
        argCount = 0;
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
    printf("\n\033[32mFile compiled successfully.\033[0m\n");
}

int main() {
    printf("Rail Compiler version [0.1]\n");
    printf("Copyright (c) me 2025. All rights reserved.\n");
    while (1) {
        newCall = 1;
        call[32];
        line = 0;
        inMain = 0;
        inFunc = 0;

        currentVarIdx = 0;

        argCount = 0;
        buffer = 0;

        printf("\nRAIL>");

        char filename[500];

        fgets(filename, sizeof(filename), stdin);

        size_t len = strlen(filename);
        if (len > 0 && filename[len - 1] == '\n') {
            filename[len - 1] = '\0';
        }
        if (strcmp(&filename[len - 5], "rail") != 0) {
            printf("\n\033[31mFile open failure: '%s' is not a valid .rail file\033[0m\n", filename);
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
                printf("\nStarting build of file '%s'...\n", filename);
                compile(content);
                free(content);
            } else {
                printf("\n\033[31mFile open failure: memory allocation failure. Please try again\033[0m\n");
                fclose(file);
            }
        } else {
            printf("\n\033[31mFile open failure: file '%s' was not found\033[0m\n", filename);
        }
        printf("Compilation finished.\n", filename);
    }

    return 0;
}