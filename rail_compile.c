#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

bool newCall = true;
char call[32];
int line = 1;

bool inMain = false;
bool inFunc = false;
bool inNest = false;
bool isFunc = false;

int mainPos = 0;

int currentVarIdx = 0;

int argCount = 0;

char funcCallBuffer[32];

int funcReturnAddr = 0x8000;
int mainAddr = 0x3;

int buffer = 0;
int buffer2 = 0;
int buffer3 = 0;

char cbuffer[32];
char cbuffer2[32];
char cbuffer3[32];

char output[65536];
char bytes[32768];
int outputPos = 0;

typedef struct {
    char name[32];
    int address;
} Variable;

typedef struct {
    char name[32];
    int address;
} Function;

Variable vars[128];
int varCount = 0;
int nextHeapAddr = 0x0200;

Function funcs[8];
int funcCount = 0;

int addVariable(const char *name) {
    if (varCount >= 128) return -1;

    strncpy(vars[varCount].name, name, sizeof(vars[varCount].name) - 1);
    vars[varCount].name[sizeof(vars[varCount].name) - 1] = '\0';
    vars[varCount].address = nextHeapAddr++;

    return varCount++;
}

int addFunc(const char *name) {
    if (funcCount >= 8) return -1;

    strncpy(funcs[funcCount].name, name, sizeof(funcs[funcCount].name) - 1);
    funcs[funcCount].name[sizeof(funcs[funcCount].name) - 1] = '\0';
    funcs[funcCount].address = 0x8000 + outputPos/3;

    return funcCount++;
}

int findVariable(const char *name) {
    for (int i = 0; i < varCount; i++) {
        if (strcmp(vars[i].name, name) == 0) return i;
    }

    return -1;
}

int findFunc(const char *name) {
    for (int i = 0; i < varCount; i++) {
        if (strcmp(funcs[i].name, name) == 0) return i;
    }

    return -1;
}

int callFunc(const char *name) {
    for (int i = 0; i < funcCount; i++) {
        if (strcmp(funcs[i].name, name) == 0) return i;
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
    int isImmediate;
    int loopCond[3];
} Branch;

#define MAX_BRANCHES 128
Branch branches[MAX_BRANCHES];
int branch_count = 0;
int current_branch = -1;

void branch(position) {
    if (branch_count >= MAX_BRANCHES) return;

    branches[branch_count].parent = current_branch;
    branches[branch_count].depth = (current_branch == -1) ? 0 : branches[current_branch].depth + 1;
    branches[branch_count].type = 1;
    branches[branch_count].bytePos = position;
    current_branch = branch_count;
    branch_count++;
}
void loopBranch(position, conditionVar1, operator, conditionVar2, isImmediate) {
    if (branch_count >= MAX_BRANCHES) return;

    branches[branch_count].isImmediate = isImmediate;
    branches[branch_count].parent = current_branch;
    branches[branch_count].depth = (current_branch == -1) ? 0 : branches[current_branch].depth + 1;
    branches[branch_count].type = 2;
    branches[branch_count].bytePos = position;

    branches[branch_count].loopCond[0] = conditionVar1;
    branches[branch_count].loopCond[1] = operator;
    branches[branch_count].loopCond[2] = conditionVar2;

    current_branch = branch_count;
    branch_count++;
}

void unbranch(isElse) {
    if (current_branch != -1) {
        if (branches[current_branch].type == 1) {
            int xxPos = (branches[current_branch].bytePos-2)*3;
            int distance = (outputPos/3) - branches[current_branch].bytePos;

            char jhCode[2];
            char jhCode2[2];
            int outputByte = 0x8003 + outputPos/3;

            sprintf(jhCode, "%02X", outputByte & 0xFF);
            sprintf(jhCode2, "%02X", (outputByte >> 8) & 0xFF);

            output[xxPos] = jhCode[0];
            output[xxPos + 1] = jhCode[1];

            output[xxPos + 3] = jhCode2[0];
            output[xxPos + 4] = jhCode2[1];

            if (isElse) {
                outputPos += sprintf(output + outputPos, "4C XX XX ");
                branch(outputPos/3);
            }
        } else if (branches[current_branch].type == 2) {
            int var1Addr = branches[current_branch].loopCond[0];
            int var2Addr = branches[current_branch].loopCond[2];
            outputPos += sprintf(output + outputPos, "AD %02X %02X ", var1Addr & 0xFF, (var1Addr >> 8) & 0xFF);
            
            if (!branches[current_branch].isImmediate) outputPos += sprintf(output + outputPos, "CD %02X %02X ", var2Addr & 0xFF, (var2Addr >> 8) & 0xFF);
            else outputPos += sprintf(output + outputPos, "C9 %02X ", var2Addr);

            int addr = 0x8003 + branches[current_branch].bytePos;
            if (branches[current_branch].loopCond[1] == 1) {
                outputPos += sprintf(output + outputPos, "D0 03 4C %02X %02X", addr & 0xFF, (addr >> 8) & 0xFF);
            } else if (branches[current_branch].loopCond[1] == 2) {
                outputPos += sprintf(output + outputPos, "F0 03 4C %02X %02X", addr & 0xFF, (addr >> 8) & 0xFF);
            } else if (branches[current_branch].loopCond[1] == 3) {
                outputPos += sprintf(output + outputPos, "B0 03 4C %02X %02X", addr & 0xFF, (addr >> 8) & 0xFF);
            }
        }
        current_branch = branches[current_branch].parent;
    } else {
        if (!inMain) printf("\033[33mWarning: unmatched 'end' was ignored at line %d\033[0m\n", line);
    }
}

const char *rxlib[] = {
    "print",
    "A9 01 20 00 FF ",
    "printn",
    "A9 02 20 00 FF ",
    "input",
    "A9 03 20 00 FF ",
    "readInput",
    "A9 04 20 00 FF "
};

int parseToken(char *token) {
    if (strcmp(token, ";") == 0) {
        if (isFunc) outputPos += sprintf(output + outputPos, "%s", funcCallBuffer);
        newCall = true;
        isFunc = false;
        argCount = 0;
        return 0;
    }

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
            if (!inFunc) {
                inMain = true;
                mainAddr = 0x8003 + outputPos/3;
            } else {
                printf("\033[31mRail compile failure: expected 'return' to end function before 'main' at line %d\033[0m\n", line);
                return -1;
            }
        } else {
            if (!inMain && !inFunc) {
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
                    int funcIdx = findFunc(call);
                    if (funcIdx != 1) {
                        outputPos += sprintf(output + outputPos, "20 %02X %02X ", funcs[funcIdx].address & 0xFF, (funcs[funcIdx].address >> 8) & 0xFF);
                    } else {
                        printf("\n\033[31mRail compile failure: '%s' is undefined at line %d\033[0m\n", call, line);
                        return -1;
                    }
                }
            } else if (strcmp(call, "return") == 0) {
            } else if (strcmp(call, "end") == 0) {
                unbranch(false);
            } else if (strcmp(call, "else") == 0) {
                unbranch(true);
            } else if (strcmp(call, "if") == 0) {
            } else if (strcmp(call, "while") == 0) {
            } else if (strcmp(call, "var") == 0) {
            } else if (strcmp(call, "assign") == 0) {
            } else if (strcmp(call, "addto") == 0) {
            } else if (strcmp(call, "subfrom") == 0) {
            } else if (strcmp(call, "") == 0) {
            } else if (findVariable(call) != -1) {
                int varIdx = findVariable(call);
                int VAddress = vars[varIdx].address;
                outputPos += sprintf(output + outputPos, "AD %02X %02X ", VAddress & 0xFF, (VAddress >> 8) & 0xFF);
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

        if (strcmp(stoken, "") == 0) return 0;

        if (strcmp(call, "function") == 0) {
            if (argCount == 1) {
                addFunc(stoken);
            } else {
                printf("\n\033[31mRail compile failure: unexpected argument '%s' for '%s' at line %d\033[0m\n", stoken, call, line);
                return -1;
            }
        } else if (strcmp(call, "return") == 0) {
            if (argCount == 1) {
                char *endptr;
                int value = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    int varIdx = findVariable(stoken);
                    if (varIdx == -1) {
                        printf("\n\033[31mRail compile failure: '%s' is undefined at line %d\033[0m\n", stoken, line);
                        return -1;
                    } else {
                        outputPos += sprintf(output + outputPos, "AD %02X %02X ", vars[varIdx].address & 0xFF, (vars[varIdx].address >> 8) & 0xFF);
                    }
                } else {
                    outputPos += sprintf(output + outputPos, "A9 %02X ", strtol(stoken, NULL, 10) & 0xFF);
                }

                outputPos += sprintf(output + outputPos, "60 ");

                inFunc = false;
            } else {
                printf("\n\033[31mRail compile failure: cannot return multiple values at line %d\033[0m\n", line);
                return -1;
            }

        } else if (strcmp(call, "assign") == 0) {
            if (argCount == 1) {
                currentVarIdx = findVariable(stoken);
                if (currentVarIdx == -1) {
                    printf("\n\033[31mRail compile failure: '%s' is undefined at line %d\033[0m\n", stoken, line);
                    return -1;
                }
            } else if (argCount == 2) {
                if (strcmp(stoken, "nest") != 0) {
                    char *endptr;
                    buffer = strtol(stoken, &endptr, 10);
                    if (*endptr != '\0') {
                        printf("\n\033[31mRail compile failure: expected integer value for 'assign', got '%s' at line %d\033[0m\n", stoken, line);
                        return -1;
                    }
                    outputPos += sprintf(output + outputPos, "A9 %02X ", buffer);
                }
                int addr = vars[findVariable(stoken)].address;
                outputPos += sprintf(output + outputPos, "8D %02X %02X ", vars[currentVarIdx].address & 0xFF, (vars[currentVarIdx].address >> 8) & 0xFF);
            } else {
                printf("\n\033[31mRail compile failure: unexpected argument '%s' for '%s' at line %d\033[0m\n", stoken, call, line);
                return -1;
            }

        } else if (strcmp(call, "var") == 0) {
            if (argCount == 1) {
                if (findVariable(stoken) == -1) {
                    currentVarIdx = addVariable(stoken);
                } else {
                    printf("\n\033[31mRail compile failure: variable name '%s' already in use at line %d\033[0m\n", stoken, line);
                    return -1;
                }
            } else if (argCount == 2) {
                if (strcmp(stoken, "nest") != 0) {
                    char *endptr;
                    int value = strtol(stoken, &endptr, 10);
                    if (*endptr != '\0') {
                        printf("\n\033[31mRail compile failure: expected integer value for 'var', got '%s' at line %d\033[0m\n", stoken, line);
                        return -1;
                    }
                    outputPos += sprintf(output + outputPos, "A9 %02X ", value);
                }
                outputPos += sprintf(output + outputPos, "8D %02X %02X ", vars[currentVarIdx].address & 0xFF, (vars[currentVarIdx].address >> 8) & 0xFF);
            } else {
                printf("\n\033[31mRail compile failure: unexpected argument '%s' for '%s' at line %d\033[0m\n", stoken, call, line);
                return -1;
            }

        } else if (strcmp(call, "if") == 0) {
            if (argCount == 1) {
                char *endptr;
                buffer = strtol(stoken, &endptr, 10);
                strcpy(cbuffer3, "A9");
                if (*endptr != '\0') {
                    int varIdx = findVariable(stoken);
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
                    strcpy(cbuffer2, "F0 03 4C ");
                } else if (strcmp(stoken, "!=") == 0) {
                    buffer2 = 2;
                    strcpy(cbuffer2, "D0 03 4C ");
                } else if (strcmp(stoken, ">=") == 0) {
                    buffer2 = 3;
                    strcpy(cbuffer2, "90 03 4C ");
                } else if (strcmp(stoken, "<") == 0) {
                    buffer2 = 4;
                    strcpy(cbuffer2, "B0 03 4C ");
                } else if (strcmp(stoken, ">") == 0) {
                    buffer2 = 4;
                    strcpy(cbuffer2, "F0 05 90 03 4C ");
                } else {
                    printf("\n\033[31mRail compile failure: unexpected token '%s' at line %d\033[0m\n", stoken, line);
                    return -1;
                }
            } else if (argCount == 3) {
                char *endptr;
                buffer3 = strtol(stoken, &endptr, 10);
                strcpy(cbuffer3, "C9");
                if (*endptr != '\0') {
                    int varIdx = findVariable(stoken);
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
                outputPos += sprintf(output + outputPos, "%sXX XX ", cbuffer2);

                branch(outputPos/3);
            } else {
                printf("\n\033[31mRail compile failure: unexpected argument '%s' for '%s' at line %d\033[0m\n", stoken, call, line);
                return -1;
            }

        } else if (strcmp(call, "while") == 0) {
            if (argCount == 1) {
                int varIdx = findVariable(stoken);
                if (varIdx != -1) {
                    buffer = vars[varIdx].address;
                } else {
                    printf("\n\033[31mRail compile failure: '%s' is undefined at line %d\033[0m\n", stoken, line);
                    return -1;
                }
            } else if (argCount == 2) {
                if (strcmp(stoken, "==") == 0) {
                    buffer2 = 1;
                } else if (strcmp(stoken, "!=") == 0) {
                    buffer2 = 2;
                } else if (strcmp(stoken, "<") == 0) {
                    buffer2 = 3;
                } else {
                    printf("\n\033[31mRail compile failure: unexpected token '%s' at line %d\033[0m\n", stoken, line);
                    return -1;
                }
            } else if (argCount == 3) {
                int varIdx = findVariable(stoken);
                if (varIdx != -1) {
                    buffer3 = vars[varIdx].address;
                    loopBranch(outputPos/3, buffer, buffer2, buffer3, false);
                } else {
                    char *endptr;
                    buffer3 = strtol(stoken, &endptr, 10);

                    if (*endptr != '\0') {
                        printf("\n\033[31mRail compile failure: '%s' is undefined at line %d\033[0m\n", stoken, line);
                        return -1;
                    }
                    loopBranch(outputPos/3, buffer, buffer2, buffer3, true);
                }
            } else {
                printf("\n\033[31mRail compile failure: unexpected argument '%s' for '%s' at line %d\033[0m\n", stoken, call, line);
                return -1;
            }
 
        } else if (strcmp(call, "addto") == 0) {
            if (argCount == 1) {
                int variable = findVariable(stoken);
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
                printf("\n\033[31mRail compile failure: unexpected argument '%s' for '%s' at line %d\033[0m\n", stoken, call, line);
                return -1;
            }
        } else if (strcmp(call, "subfrom") == 0) {
            if (argCount == 1) {
                int variable = findVariable(stoken);
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
                printf("\n\033[31mRail compile failure: unexpected argument '%s' for '%s' at line %d\033[0m\n", stoken, call, line);
                return -1;
            }
        } else if (findVariable(call) != -1) {
            if (argCount == 1) {
                if (strcmp(stoken, "+") == 0) {
                    buffer = 1;
                } else if (strcmp(stoken, "-") == 0) {
                    buffer = 2;
                } else {
                    printf("\n\033[31mRail compile failure: unexpected token '%s' at line %d\033[0m\n", stoken, line);
                    return -1;
                }
            } else if (argCount == 2) {
                char *endptr;
                int value = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    int varIdx = findVariable(stoken);
                    if (varIdx != -1) {
                        buffer3 = 1;
                        buffer2 = vars[varIdx].address;
                    } else {
                        printf("\n\033[31mRail compile failure: '%s' is undefined at line %d\033[0m\n", stoken, line);
                        return -1;
                    }
                } else {
                    buffer2 = strtol(stoken, NULL, 10);
                    buffer3 = 0;
                }
                
                if (buffer == 1) {
                    if (buffer3 == 1) {
                        outputPos += sprintf(output + outputPos, "6D %02X %02X ", buffer2 & 0xFF, (buffer2 >> 8) & 0xFF);
                    } else {
                        int stokenValue = strtol(stoken, NULL, 10);
                        outputPos += sprintf(output + outputPos, "69 %02X ", stokenValue & 0xFF);
                    }
                } else if (buffer == 2) {
                    if (buffer3 == 1) {
                        outputPos += sprintf(output + outputPos, "ED %02X %02X ", buffer2 & 0xFF, (buffer2 >> 8) & 0xFF);
                    } else {
                        int stokenValue = strtol(stoken, NULL, 10);
                        outputPos += sprintf(output + outputPos, "E9 %02X ", stokenValue & 0xFF);
                    }
                }
            } else {
                printf("\n\033[31mRail compile failure: unexpected argument '%s' for '%s' at line %d\033[0m\n", stoken, call, line);
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
    bool nestCallEnded = false;

    bool nestIsFunc = false;

    int nestBuffer = buffer;
    int nestBuffer2 = buffer2;
    int nestBuffer3 = buffer3;

    char nestCBuffer[32];
    char nestCBuffer2[32];
    char nestCBuffer3[32];

    char nestCall[32];

    int nestArgCount = 0;

    for (int c = 0; c < strlen(code); c++) {
        bool ignoring = false;
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
            continue;
        }

        if (ch == ';' && inNest) nestCallEnded = true;

        if (ch == '(') {
            ignoring = true;
            if (!newCall) {
                if (inNest) {
                    printf("\n\033[31mRail compile failure: cannot have nested call within nested call at line %d\033[0m\n", line);
                    return -1;
                }
                inNest = true;

                nestIsFunc = isFunc;

                nestBuffer = buffer;
                nestBuffer2 = buffer2;
                nestBuffer3 = buffer3;

                strcpy(nestCBuffer, cbuffer);
                strcpy(nestCBuffer2, cbuffer2);
                strcpy(nestCBuffer3, cbuffer3);

                nestCall[32];
                strcpy(nestCall, call);
                newCall = true;

                nestArgCount = argCount;

                argCount = 0;
                strcpy(call, "");
            } else {
                printf("\n\033[31mRail compile failure: unexpected '(' at line %d\033[0m\n", line);
                return -1;
            }
        }
        if (ch == ')') {
            if (!nestCallEnded) {
                printf("\n\033[31mRail compile failure: expected ';' before ')' at line %d\033[0m\n", line);
                return -1;
            }
            nestCallEnded = false;
            ignoring = true;

            token[tokenIdx] = '\0';
            if (parseToken(token) == -1) return -1;
            tokenIdx = 0;

            if (inNest) {
                inNest = false;
                newCall = true;

                isFunc = nestIsFunc;

                buffer = nestBuffer;
                buffer2 = nestBuffer2;
                buffer3 = nestBuffer3;

                strcpy(cbuffer, nestCBuffer);
                strcpy(cbuffer2, nestCBuffer2);
                strcpy(cbuffer3, nestCBuffer3);

                strcpy(call, nestCall);
                argCount = nestArgCount;
                newCall = false;

                if (code[c+1] == ';') {
                    if (parseToken("nest;") == -1) return -1;
                } else {
                    if (parseToken("nest") == -1) return -1;
                }
                
            } else {
                printf("\n\033[31mRail compile failure: unexpected ')' at line %d\033[0m\n", line);
                return -1;
            }
            continue;
        }

        if (isspace(ch) && !inComment && !inString) {
            if (tokenIdx > 0 && strcmp(token, "") != 0) {
                token[tokenIdx] = '\0';
                if (parseToken(token) == -1) return -1;
                tokenIdx = 0;
            }
        } else {
            if (tokenIdx < (int)sizeof(token) - 1 && !ignoring) {
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
    printf("RailPlay Binary Creation Tool\n");
    printf("Copyright (C) Innovation Incorporated 2025. All rights reserved.\n");
    while (true) {
        newCall = true;
        call[32];
        line = 1;
        inMain = false;
        inFunc = false;
        isFunc = false;
        inNest = false;
        currentVarIdx = 0;
        argCount = 0;
        buffer = 0;
        buffer2 = 0;
        buffer3 = 0;
        outputPos = 0;
        varCount = 0;
        nextHeapAddr = 0x0200;
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

                    unsigned char low  = mainAddr & 0xFF;
                    unsigned char high = (mainAddr >> 8) & 0xFF;

                    bytes[outputSize++] = hexStringToByte("4C");
                    bytes[outputSize++] = low;
                    bytes[outputSize++] = high;

                    for (int i = 0; i < (int)strlen(output); i += 3) {
                        char hex[3] = {0};
                        if (output[i] == ' ' || output[i] == '\0' || output[i] == '\n') continue;
                        hex[0] = output[i];
                        hex[1] = output[i+1];
                        int byte = hexStringToByte(hex);
                        bytes[outputSize++] = (unsigned char)byte;
                    }

                    bytes[0x7FFC] = 0x00;
                    bytes[0x7FFD] = 0x80;

                    fwrite(bytes, 1, 32768, binary);
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