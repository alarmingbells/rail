#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

bool newCall = true;
char call[32];
int line = 0;

bool inMain = false;
bool inFunc = false;
bool inNest = false;
bool isFunc = false;

int currentVarIdx = 0;

int argCount = 0;

char funcCallBuffer[32];

int buffer = 0;
int buffer2 = 0;
int buffer3 = 0;

char cbuffer[32];
char cbuffer2[32];
char cbuffer3[32];

char output[65536];
char bytes[65536];
int outputPos = 0;

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

int hexStringToByte(const char *hex) {
    int byte;
    sscanf(hex, "%2x", &byte);

    return byte;
}

typedef struct Branch {
    int parent;
    int depth;
    int type;
    int bytePos;
} Branch;

#define MAX_BRANCHES 128
Branch branches[MAX_BRANCHES];
int branch_count = 0;
int current_branch = -1;

void branch(type, position) {
    if (branch_count >= MAX_BRANCHES) return;

    branches[branch_count].parent = current_branch;
    branches[branch_count].depth = (current_branch == -1) ? 0 : branches[current_branch].depth + 1;
    branches[branch_count].type = type;
    branches[branch_count].bytePos = position;
    current_branch = branch_count;
    branch_count++;
}

void unbranch(isElse) {
    if (current_branch != -1) {
        int xxPos = (branches[current_branch].bytePos-1)*3;
        int distance = (outputPos/3) - branches[current_branch].bytePos;
        char jhCode[2];
        sprintf(jhCode, "%02X", distance & 0xFF);

        output[xxPos] = jhCode[0];
        output[xxPos + 1] = jhCode[1];

        current_branch = branches[current_branch].parent;

        if (isElse) {
            outputPos += sprintf(output + outputPos, "A9 01 C9 01 F0 XX ");
            branch(1, outputPos/3);
        }
    }
}

const char *rxlib[] = {
    "input",
    "A9 02 20 00 FF ",
    "print",
    "A9 01 20 00 FF "
};

int parseToken(char *token) {
    if (newCall) {
        newCall = false;

        size_t len = strlen(token);
        if (len > 0 && token[len - 1] == ';') {
            strncpy(call, token, len - 1);
            call[len - 1] = '\0';
        } else {
            if (len > 0 && token[len - 1] == ':') {
                strncpy(call, token, len - 1);
                call[len - 1] = '\0';
            } else {
                strcpy(call, token);
            }
        }


        if (strncmp(call, "main", 4) == 0) {
            inMain = true;
        } else {
            if (!inMain) {
                if (strcmp(call, "function") == 0) {
                    inFunc = true;
                } else {
                    printf("\033[31mRail compile failure: call '%s' outside of main or function at line %d\033[0m\n", call, line);
                    return -1;
                }
            } else if (call[0] == '#') {
                isFunc = true;
                int found = -1;
                for (size_t i = 0; i < sizeof(rxlib) / sizeof(rxlib[0]); i++) {
                    if (strcmp(call + 1, rxlib[i]) == 0) {
                        found = i;
                        break;
                    }
                }
                if (found != -1) {
                    strcpy(funcCallBuffer, rxlib[found+1]);
                } else {
                    printf("\n\033[31mRail compile failure: '%s' is not a valid function at line %d\033[0m\n", call, line);
                    return -1;
                }
            } else if (strcmp(call, "end") == 0) {
                unbranch(false);
            } else if (strcmp(call, "else") == 0) {
                unbranch(true);
            } else if (strcmp(call, "if") == 0) {
            } else if (strcmp(call, "var") == 0) {
            } else if (strcmp(call, "assign") == 0) {
            } else if (strcmp(call, "addto") == 0) {
            } else if (strcmp(call, "subfrom") == 0) {
            } else {
                printf("\n\033[31mRail compile failure: unknown keyword '%s' at line %d\033[0m\n", call, line);
                return -1;
            }                
        }
    } else {
        argCount++;

        char stoken[128];

        size_t len = strlen(token);
        while (len > 0 && (token[len - 1] == ';' || token[len - 1] == ':')) {
            len--;
        }
        strncpy(stoken, token, len);
        stoken[len] = '\0';

        if (strcmp(call, "assign") == 0) {
            if (argCount == 1) {
                currentVarIdx = find_variable(stoken);
                if (currentVarIdx == -1) {
                    printf("\n\033[31mRail compile failure: '%s' is undefined at line %d\033[0m\n", stoken, line);
                    return -1;
                }
            } else if (argCount == 2) {
                char *endptr;
                buffer = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    printf("\n\033[31mRail compile failure: expected integer value for 'assign', got '%s' at line %d\033[0m\n", stoken, line);
                    return -1;
                }

                outputPos += sprintf(output + outputPos, "A9 %02X 8D ", buffer);
                int addr = vars[find_variable(stoken)].address;
                outputPos += sprintf(output + outputPos, "%02X %02X ", vars[currentVarIdx].address & 0xFF, (vars[currentVarIdx].address >> 8) & 0xFF);
            } else {
                printf("\n\033[31mRail compile failure: incorrect number of arguments for '%s' at line %d\033[0m\n", call, line);
                return -1;
            }

        } else if (strcmp(call, "var") == 0) {
            if (argCount == 1) {
                if (find_variable(stoken) == -1) {
                    currentVarIdx = add_variable(stoken);
                } else {
                    printf("\n\033[31mRail compile failure: variable name '%s' already in use at line %d\033[0m\n", stoken, line);
                    return -1;
                }
            } else if (argCount == 2) {
                char *endptr;
                int value = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    printf("\n\033[31mRail compile failure: expected integer value for 'var', got '%s' at line %d\033[0m\n", stoken, line);
                    return -1;
                }
                outputPos += sprintf(output + outputPos, "A9 %02X 8D ", value);
                outputPos += sprintf(output + outputPos, "%02X %02X ", vars[currentVarIdx].address & 0xFF, (vars[currentVarIdx].address >> 8) & 0xFF);
            } else {
                printf("\n\033[31mRail compile failure: incorrect number of arguments for '%s' at line %d\033[0m\n", call, line);
                return -1;
            }

        } else if (strcmp(call, "if") == 0) {
            if (argCount == 1) {
                char *endptr;
                buffer = strtol(stoken, &endptr, 10);
                strcpy(cbuffer3, "A9");
                if (*endptr != '\0') {
                    int varIdx = find_variable(stoken);
                    if (varIdx != -1) {
                        strcpy(cbuffer, "AD");
                        buffer = vars[varIdx].address;
                    } else {
                        printf("\n\033[31mRail compile failure: expected integer or variable for 'if', got '%s' at line %d\033[0m\n", stoken, line);
                        return -1;
                    }
                }
            } else if (argCount == 2) {
                if (strcmp(stoken, "==") == 0) {
                    buffer2 = 1;
                    strcpy(cbuffer2, "D0");
                } else if (strcmp(stoken, "!=") == 0) {
                    buffer2 = 2;
                    strcpy(cbuffer2, "F0");
                } else if (strcmp(stoken, ">=") == 0) {
                    buffer2 = 3;
                    strcpy(cbuffer2, "B0");
                } else {
                    printf("\n\033[31mRail compile failure: unexpected token '%s' at line %d\033[0m\n", stoken, line);
                    return -1;
                }
            } else if (argCount == 3) {
                char *endptr;
                buffer3 = strtol(stoken, &endptr, 10);
                strcpy(cbuffer3, "C9");
                if (*endptr != '\0') {
                    int varIdx = find_variable(stoken);
                    if (varIdx != -1) {
                        strcpy(cbuffer3, "CD");
                        buffer3 = vars[varIdx].address;
                    } else {
                        printf("\n\033[31mRail compile failure: expected integer or variable for 'if', got '%s' at line %d\033[0m\n", stoken, line);
                        return -1;
                    }
                }

                outputPos += sprintf(output + outputPos, "%s %02X %02X ", cbuffer, buffer & 0xFF, (buffer >> 8) & 0xFF);
                if (strcmp(cbuffer3, "CD") == 0) {
                    outputPos += sprintf(output + outputPos, "%s %02X %02X ", cbuffer3, buffer3 & 0xFF, (buffer3 >> 8) & 0xFF);
                } else if (strcmp(cbuffer3, "C9") == 0) {
                    outputPos += sprintf(output + outputPos, "%s %02X ", cbuffer3, buffer3 & 0xFF);
                }
                outputPos += sprintf(output + outputPos, "%s XX ", cbuffer2);

                branch(1, outputPos/3);
            } else {
                printf("\n\033[31mRail compile failure: incorrect number of arguments for '%s' at line %d\033[0m\n", call, line);
                return -1;
            }

        } else if (strcmp(call, "addto") == 0) {
            if (argCount == 1) {
                int variable = find_variable(stoken);
                if (variable == -1) {
                    printf("\n\033[31mRail compile failure: invalid variable name '%s' for 'addto' at line %d\033[0m\n", stoken, line);
                    return -1;
                }
                buffer = vars[variable].address;
            } else if (argCount == 2) {
                char *endptr;
                buffer2 = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    printf("\n\033[31mRail compile failure: expected integer value for 'addto', got '%s' at line %d\033[0m\n", stoken, line);
                    return -1;
                }
                outputPos += sprintf(output + outputPos, "AD %02X %02X ", buffer & 0xFF, (buffer >> 8) & 0xFF);
                outputPos += sprintf(output + outputPos, "69 %02X ", buffer2 & 0xFF);
                outputPos += sprintf(output + outputPos, "8D %02X %02X ", buffer & 0xFF, (buffer >> 8) & 0xFF);
            } else {
                printf("\n\033[31mRail compile failure: incorrect number of arguments for '%s' at line %d\033[0m\n", call, line);
                return -1;
            }
        } else if (strcmp(call, "subfrom") == 0) {
            if (argCount == 1) {
                int variable = find_variable(stoken);
                if (variable == -1) {
                    printf("\n\033[31mRail compile failure: invalid variable name '%s' for 'subfrom' at line %d\033[0m\n", stoken, line);
                    return -1;
                }
                buffer = vars[variable].address;
            } else if (argCount == 2) {
                char *endptr;
                buffer2 = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    printf("\n\033[31mRail compile failure: expected integer value for 'subfrom', got '%s' at line %d\033[0m\n", stoken, line);
                    return -1;
                }
                outputPos += sprintf(output + outputPos, "AD %02X %02X ", buffer & 0xFF, (buffer >> 8) & 0xFF);
                outputPos += sprintf(output + outputPos, "E9 %02X ", buffer2 & 0xFF);
                outputPos += sprintf(output + outputPos, "8D %02X %02X ", buffer & 0xFF, (buffer >> 8) & 0xFF);
            } else {
                printf("\n\033[31mRail compile failure: incorrect number of arguments for '%s' at line %d\033[0m\n", call, line);
                return -1;
            }
        } else if (isFunc) {
        } else {
            printf("\n\033[31mRail compile failure: expected ';' before '%s' at line %d\033[0m\n", stoken, line);
            return -1;
        }
    }
    if (token[strlen(token)-1] == ';' || token[strlen(token)-1] == ':') {
        if (isFunc) outputPos += sprintf(output + outputPos, "%s", funcCallBuffer);
        newCall = true;
        isFunc = false;
        argCount = 0;
    }
    return 0;
}

int compile(char *code) {
    char token[128];
    int tokenIdx = 0;

    int strPos = 0;

    bool inComment = false;
    bool inString = false;

    for (int c = 0; c < strlen(code); c++) {
        char ch = code[c];

        if (ch == '\n') line++;
        if (ch == '/') {inComment = !inComment; continue;}
        if (inComment) continue;

        if (ch == '"') {
            inString = !inString;
            if (!inString) parseToken("str");
            strPos = 0;
            continue;
        }

        if (inString) {
            if (strPos > 31) {
                printf("\n\033[31mRail compile failure: string exceeds 32 byte buffer limit at line %d\033[0m\n", line);
                return -1;
            }
            outputPos += sprintf(output + outputPos, "A9 %02X 85 %02X ", (unsigned char)ch, 224 + strPos);
            strPos++;
        }

        if (ch == '(') {
            if (!newCall) {
                inNest = true;

                int nestBuffer = buffer;
                int nestBuffer2 = buffer2;
                int nestBuffer3 = buffer3;

                char nestCBuffer[32];
                char nestCBuffer2[32];
                char nestCBuffer3[32];
                strcpy(nestCBuffer, cbuffer);
                strcpy(nestCBuffer2, cbuffer2);
                strcpy(nestCBuffer3, cbuffer3);

                char nestCall[32];
                strcpy(nestCall, call);
                newCall = true;

                int nestArgCount = argCount;
            } else {
                printf("\n\033[31mRail compile failure: unexpected '(' at line %d\033[0m\n", line);
                return -1;
            }
        }

        if (isspace(ch) && !inComment && !inString) {
            if (tokenIdx > 0) {
                token[tokenIdx] = '\0';
                if (parseToken(token) == -1) return -1;
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
    if (current_branch == -1) {
        printf("\n\033[32mFile compiled successfully.\033[0m\n");
        return 0;
    }
    printf("\n\033[31mRail compile failure: %d branch(es) unclosed at line %d\033[0m\n", current_branch+1, line);
    return -1;
}

int main() {
    printf("Rail Compiler for MOS 6502\n");
    printf("Copyright (C) Innovation Incorporated 2025. All rights reserved.\n");
    while (true) {
        newCall = true;
        call[32];
        line = 0;
        inMain = false;
        inFunc = false;
        isFunc = false;
        currentVarIdx = 0;
        argCount = 0;
        buffer = 0;
        buffer2 = 0;
        buffer3 = 0;
        outputPos = 0;
        var_count = 0;
        next_heap_addr = 0x0200;
        memset(vars, 0, sizeof(vars));
        memset(output, 0, sizeof(output));
        memset(bytes, 0, sizeof(bytes));
        branch_count = 0;
        current_branch = -1;

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

                int sucess = compile(content);
                
                if (sucess == 0) {
                    
                    char compFileName[32];
                    for (int i = 0; i < strlen(filename); i++) {
                        if (filename[i] == '.') {
                            compFileName[i] = '\0';
                            break;
                        }
                        compFileName[i] = filename[i];
                    }
                    strcat(compFileName, ".bin");

                    printf("%s\n", output);

                    int outputSize = 0;
                    FILE *binary = fopen(compFileName, "wb");

                    for (int i = 0; i < (int)strlen(output); i += 3) {
                        char hex[3] = {0};
                        if (output[i] == ' ' || output[i] == '\0' || output[i] == '\n') continue;
                        hex[0] = output[i];
                        hex[1] = output[i+1];
                        int byte = hexStringToByte(hex);
                        bytes[outputSize++] = (unsigned char)byte;
                    }
                    fwrite(bytes, 1, outputSize, binary);
                    fclose(binary);
                }
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