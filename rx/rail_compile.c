#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

#define START_ADDR 0xC003

bool newCall = true;
char call[32];
int line = 1;
int col = 0;

bool inMain = false;
bool inFunc = false;
bool inNest = false;
bool isFunc = false;
bool pressDefined = false;

int mainPos = 0;

int currentVarIdx = 0;

int argCount = 0;

char funcCallBuffer[32];

int interruptAddr = START_ADDR;
int funcReturnAddr = START_ADDR-3;
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

int strLength = 0;

bool warned = false;

bool inBytes = false;
char bytebuffer[8192];
int bytebufferPos = 0;

typedef struct {
    char name[32];
    int address;
} Variable;

typedef struct {
    char name[32];
    int address;
    int size;
} Array;

typedef struct {
    char name[32];
    int address;
} Function;

Variable vars[128];
int varCount = 0;
int nextHeapAddr = 0x0200;

Variable arrays[8];
int arrayCount = 0;

Function funcs[8];
int funcCount = 0;

int error(int type, char *token) {
    //1 - undefined
    //2 - expected integer
    //3 - output overflow
    //4 - invalid keyword
    //5 - unexpected argument
    //6 - bad logic
    //7 - bad colour
    //8 - no ';'
    //9 - no return
    //10- no parent
    //11- double definition
    //12- double nest
    //13- branch unclosed
    //14- invalid pixel
    //15- bad string

    switch (type) {
        case 1:
            printf("\n\033[31mRail compile failure: '%s' is undefined at line %d, column %d\n(error code 1)\033[0m\n", token, line, col);
            break;
        case 2:
            printf("\n\033[31mRail compile failure: Expected integer value, got '%s' at line %d, column %d\n(error code 2)\033[0m\n", token, line, col);
            break;
        case 3:
            printf("\n\033[31mRail compile failure: Program output exceeds 16KB cartridge limit!\n(error code 3)\033[0m\n");
            break;
        case 4:
            printf("\n\033[31mRail compile failure: Invalid keyword '%s' at line %d, column %d\n(error code 4)\033[0m\n", token, line, col);
            break;
        case 5:
            printf("\n\033[31mRail compile failure: Unexpected argument '%s' at line %d, column %d\n(error code 5)\033[0m\n", token, line, col);
            break;
        case 6:
            printf("\n\033[31mRail compile failure: Undefined logic symbol '%s' at line %d, column %d\n(error code 6)\033[0m\n", token, line, col);
            break;
        case 7:
            printf("\n\033[31mRail compile failure: Undefined colour symbol '%s' at line %d, column %d\n(error code 7)\033[0m\n", token, line, col);
            break;
        case 8:
            printf("\n\033[31mRail compile failure: Expected ';' before '%s' at line %d, column %d\n(error code 8)\033[0m\n", token, line, col);
            break;
        case 9:
            printf("\n\033[31mRail compile failure: Expected 'return' to close function before 'main' at line %d, column %d\n(error code 9)\033[0m\n", line, col);
            break;
        case 10:
            printf("\n\033[31mRail compile failure: Call '%s' has no parent function at line %d, column %d\n(error code 10)\033[0m\n", token, line, col);
            break;
        case 11:
            printf("\n\033[31mRail compile failure: '%s' is already defined at line %d, column %d\n(error code 11)\033[0m\n", token, line, col);
            break;
        case 12:
            printf("\n\033[31mRail compile failure: Double-nesting is not allowed at line %d, column %d\n(error code 12)\033[0m\n", line, col);
            break;
        case 13:
            printf("\n\033[31mRail compile failure: branch(es) unclosed at line %d, column %d\n(error code 13)\033[0m\n", line, col);
            break;
        case 14:
            printf("\n\033[31mRail compile failure: Invalid pixel value at line %d, column %d\n(error code 14)\033[0m\n", line, col);
            break;
        case 15:
            printf("\n\033[31mRail compile failure: String exceeds 16 byte buffer limit at line %d, column %d\n(error code 15)\033[0m\n", line, col);
            break;
        
    }
    return 0;
}

int warn(int type, const char *token) {
    //1 - out of memory
    //2 - unmatched end
    //3 - bytecode used

    warned = true;
    switch (type) {
        case 1:
            printf("\033[33mWarning: Variable '%s' is ignored due to a lack of memory (variables exceed 15.5KB).\n(warning code 1)\033[0m\n", token);
            break;
        case 2:
            printf("\033[33mWarning: unmatched 'end' is ignored at line %d\n(warning code 2)\033[0m\n", line);
            break;
        case 3:
            printf("\033[33mWarning: User-written bytecode is not checked by compiler, review output to ensure functionality.\n(warning code 3)\033[0m\n");
            break;
    }
    return 0;
}

int addVariable(const char *name) {
    if (nextHeapAddr >= 0x3FFF) {
        warn(1, name);
        return -1;
    }

    strncpy(vars[varCount].name, name, sizeof(vars[varCount].name) - 1);
    vars[varCount].name[sizeof(vars[varCount].name) - 1] = '\0';
    vars[varCount].address = nextHeapAddr++;

    return varCount++;
}

int addArray(const char *name, int size) {
    if (nextHeapAddr >= (0x3FFF-size)) {
        warn(1, name);
        return -1;
    }

    strncpy(arrays[arrayCount].name, name, sizeof(arrays[arrayCount].name) - 1);
    arrays[arrayCount].name[sizeof(arrays[arrayCount].name) - 1] = '\0';
    arrays[arrayCount].address = nextHeapAddr++;

    nextHeapAddr += size;

    return arrayCount++;
}

int addFunc(const char *name) {
    if (funcCount >= 8) return -1;

    strncpy(funcs[funcCount].name, name, sizeof(funcs[funcCount].name) - 1);
    funcs[funcCount].name[sizeof(funcs[funcCount].name) - 1] = '\0';
    funcs[funcCount].address = START_ADDR + outputPos/3;

    return funcCount++;
}

int findVariable(const char *name) {
    for (int i = 0; i < varCount; i++) {
        if (strcmp(vars[i].name, name) == 0) return i;
    }

    return -1;
}
int findArray(const char *name) {
    for (int i = 0; i < arrayCount; i++) {
        if (strcmp(arrays[i].name, name) == 0) return i;
    }

    return -1;
}

int findFunc(const char *name) {
    for (int i = 0; i < funcCount; i++) {
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

void endCall() {
    if (isFunc) outputPos += sprintf(output + outputPos, "%s", funcCallBuffer);
    newCall = true;
    isFunc = false;
    argCount = 0;
}

typedef struct Branch {
    int parent;
    int depth;
    int type;
    int bytePos;
    int isImmediate;
    int isUncond;
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

    bool unc = false;
    if (conditionVar1 == -1) unc = true; 

    branches[branch_count].isImmediate = isImmediate;
    branches[branch_count].parent = current_branch;
    branches[branch_count].depth = (current_branch == -1) ? 0 : branches[current_branch].depth + 1;
    branches[branch_count].type = 2;
    branches[branch_count].bytePos = position;

    if (!unc) {
        branches[branch_count].loopCond[0] = conditionVar1;
        branches[branch_count].loopCond[1] = operator;
        branches[branch_count].loopCond[2] = conditionVar2;
    } else {
        branches[branch_count].loopCond[0] = 0;
        branches[branch_count].loopCond[1] = 4;
        branches[branch_count].loopCond[2] = 0;
    }
    branches[branch_count].isUncond = unc;

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
            int outputByte = START_ADDR + outputPos/3;

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
            if (branches[current_branch].loopCond[1] != 4) {
                int var1Addr = branches[current_branch].loopCond[0];
                int var2Addr = branches[current_branch].loopCond[2];
                outputPos += sprintf(output + outputPos, "AD %02X %02X ", var1Addr & 0xFF, (var1Addr >> 8) & 0xFF);
                
                if (!branches[current_branch].isImmediate) outputPos += sprintf(output + outputPos, "CD %02X %02X ", var2Addr & 0xFF, (var2Addr >> 8) & 0xFF);
                else outputPos += sprintf(output + outputPos, "C9 %02X ", var2Addr);
            }

            int addr = START_ADDR + branches[current_branch].bytePos;
            if (branches[current_branch].loopCond[1] == 1) {
                outputPos += sprintf(output + outputPos, "D0 03 4C %02X %02X ", addr & 0xFF, (addr >> 8) & 0xFF);
            } else if (branches[current_branch].loopCond[1] == 2) {
                outputPos += sprintf(output + outputPos, "F0 03 4C %02X %02X ", addr & 0xFF, (addr >> 8) & 0xFF);
            } else if (branches[current_branch].loopCond[1] == 3) {
                outputPos += sprintf(output + outputPos, "B0 03 4C %02X %02X ", addr & 0xFF, (addr >> 8) & 0xFF);
            } else if (branches[current_branch].loopCond[1] == 4) {
                outputPos += sprintf(output + outputPos, "4C %02X %02X ", addr & 0xFF, (addr >> 8) & 0xFF);
            }
        }
        current_branch = branches[current_branch].parent;
    } else {
        if (!inMain) warn(2, "");
    }
}

const char *rxlib[] = {
    "clear",
    "A2 00 "    //LDX #0
    "A0 40 "    //LDY #40
    "99 00 40 " //STA $4000, Y
    "E8 "       //INX
    "D0 FA "    //BNE loop
    "C8 "       //INY
    "D0 F7 ",   //BNE loop,

    "reset",
    "A2 00 "    //LDX #0
    "9A "       //TXS
    "78 "       //SEI
    "4C 00 C0 " //JMP $C000
};

int parseToken(char *token) {
    if (outputPos/3 > 0x3FF0) {
        error(3, 0);
        return -1;
    }

    if (strcmp(token, ";") == 0) {
        if (isFunc) outputPos += sprintf(output + outputPos, "%s", funcCallBuffer);
        newCall = true;
        isFunc = false;
        argCount = 0;
        return 0;
    }

    char stoken[128];

    size_t len = strlen(token);
    while (len > 0 && (token[len - 1] == ';' || token[len - 1] == ':')) {
        len--;
    }

    int arrayIndex = 0;
    int arrayIndexVar = false;

    bool isIndex = false;
    char idx[3];
    int idxidx = 0;
    for (int i = 0; i < len; i++) {
        if (token[i] == '[') {
            isIndex = true;
            continue;
        }
        if (isIndex) {
            if (token[i] != ']') {
                idx[idxidx++] = token[i];
            } else {
                isIndex = false;
                idx[idxidx] = '\0';
                char *endptr;
                int value = strtol(idx, &endptr, 10);
                if (*endptr != '\0') {
                    int varIdx = findVariable(idx);
                    if (varIdx != -1) {
                        arrayIndexVar = true;
                        arrayIndex = vars[varIdx].address;
                    } else {
                        error(1, stoken);
                        return -1;
                    }
                } else {
                    arrayIndex = value;
                    arrayIndexVar = false;
                }
            }
        }
    }

    strncpy(stoken, token, len);
    stoken[len] = '\0';

    if (strcmp(stoken, "p1Up") == 0) {
        outputPos += sprintf(output + outputPos, 
            "AD 00 80 " //LDA $8000 (controller register)
            "29 01 "    //AND #01
            "85 00 02 "  //STA $0200
        );
    }
    if (strcmp(stoken, "p1Down") == 0) {
        outputPos += sprintf(output + outputPos, 
            "AD 00 80 " //LDA $8000 (controller register)
            "29 01 "    //AND #02
            "4A "       //LSR
            "85 01 02 "  //STA $0201
        );
    }
    if (strcmp(stoken, "p1Left") == 0) {
        outputPos += sprintf(output + outputPos, 
            "AD 00 80 " //LDA $8000 (controller register)
            "29 04 "    //AND #04
            "4A "       //LSR
            "4A "       //LSR
            "85 02 02 "  //STA $0202
        );
    }
    if (strcmp(stoken, "p1Right") == 0) {
        outputPos += sprintf(output + outputPos, 
            "AD 00 80 " //LDA $8000 (controller register)
            "29 08 "    //AND #08
            "4A "       //LSR
            "4A "       //LSR
            "4A "       //LSR
            "85 02 03 "  //STA $0203
        );
    }


    if (strcmp(stoken, "p2Up") == 0) {
        outputPos += sprintf(output + outputPos, 
            "AD 00 80 " //LDA $8000 (controller register)
            "29 10 "    //AND #16
            "4A "       //LSR
            "4A "       //LSR
            "4A "       //LSR
            "4A "       //LSR
            "85 00 02"  //STA $0204
        );
    }
    if (strcmp(stoken, "p2Down") == 0) {
        outputPos += sprintf(output + outputPos, 
            "AD 00 80 " //LDA $8000 (controller register)
            "29 20 "    //AND #32
            "2A "       //ROL
            "2A "       //ROL
            "2A "       //ROL
            "85 01 02"  //STA $0205
        );
    }
    if (strcmp(stoken, "p2Left") == 0) {
        outputPos += sprintf(output + outputPos, 
            "AD 00 80 " //LDA $8000 (controller register)
            "29 40 "    //AND #64
            "2A "       //ROL
            "2A "       //ROL
            "85 02 02"  //STA $0206
        );
    }
    if (strcmp(stoken, "p2Right") == 0) {
        outputPos += sprintf(output + outputPos, 
            "AD 00 80 " //LDA $8000 (controller register)
            "29 80 "    //AND #128
            "2A "       //ROL
            "85 02 03"  //STA $0207
        );
    }

    if (newCall) {
        newCall = false;

        size_t len = strlen(token);
        if (len > 0) {
            int end = (int)len - 1;
            while (end >= 0 && (token[end] == ';' || token[end] == ':')) end--;
            if (end >= 0 && token[end] == ']') {
                int i = end - 1;
                while (i >= 0 && token[i] != '[') i--;
                if (i >= 0 && token[i] == '[') {
                    len = i;
                } else {
                    len = end + 1;
                }
            } else {
            len = end + 1;
            }
        }
        if (len > 0 && token[len - 1] == ';') {
            strncpy(call, token, len - 1);
            call[len - 1] = '\0';
        } else {
            if (len > 0 && token[len - 1] == ':') {
            strncpy(call, token, len - 1);
            call[len - 1] = '\0';
            } else {
            strncpy(call, token, len);
            call[len] = '\0';
            }
        }

        if (strncmp(call, "main", 4) == 0) {
            if (!inFunc) {
                inMain = true;
                mainAddr = START_ADDR + outputPos/3;
            } else {
                error(9, 0);
                return -1;
            }
        } else {
            if (!inMain && !inFunc) {
                if (strcmp(call, "function") == 0) {
                    inFunc = true;
                } else {
                    error(10, call);
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
                    int funcIdx = findFunc(call + 1);
                    if (funcIdx != -1) {
                        int addr = funcs[funcIdx].address;
                        outputPos += sprintf(output + outputPos, "20 %02X %02X ", addr & 0xFF, (addr >> 8) & 0xFF);
                    } else {
                        error(1, call);
                        return -1;
                    }
                    strcpy(funcCallBuffer, "");
                }
            } else if (strcmp(call, "return") == 0) {
            } else if (strcmp(call, "end") == 0) {
                unbranch(false);
            } else if (strcmp(call, "else") == 0) {
                unbranch(true);
            } else if (strcmp(call, "if") == 0) {
            } else if (strcmp(call, "while") == 0) {
            } else if (strcmp(call, "forever") == 0) {
                loopBranch(outputPos/3, -1, 0, 0, false);
            } else if (strcmp(call, "var") == 0) {
            } else if (strcmp(call, "arr") == 0) {
            } else if (strcmp(call, "assign") == 0) {
            } else if (strcmp(call, "addto") == 0) {
            } else if (strcmp(call, "subfrom") == 0) {
            } else if (strcmp(call, "setPx") == 0) {
            } else if (strcmp(call, "setBg") == 0) {
            } else if (strcmp(call, "sleep") == 0) {
            } else if (strcmp(call, "bindPress") == 0) {
                    pressDefined = true;
                    outputPos += sprintf(output + outputPos, "58 ");
            } else if (strcmp(call, "") == 0) {
            } else if (findVariable(call) != -1) {
                int varIdx = findVariable(call);
                int VAddress = vars[varIdx].address;
                outputPos += sprintf(output + outputPos, "AD %02X %02X ", VAddress & 0xFF, (VAddress >> 8) & 0xFF);
            } else if (findArray(call) != -1) {
                int varIdx = findArray(call);
                int VAddress = arrays[varIdx].address;
                if (!arrayIndexVar) {
                    outputPos += sprintf(output + outputPos, "A2 %02X ", arrayIndex);
                } else {
                    outputPos += sprintf(output + outputPos, "9D %02X %02X ", arrays[currentVarIdx].address & 0xFF, (vars[currentVarIdx].address >> 8) & 0xFF);
                }
                outputPos += sprintf(output + outputPos, "BD %02X %02X ", VAddress & 0xFF, (VAddress >> 8) & 0xFF);
            } else {
                error(4, call);
                return -1;
            }                
        }
    } else {
        argCount++;

        char stoken[128];

        size_t len = strlen(token);
        if (len > 0) {
            int end = (int)len - 1;
            while (end >= 0 && (token[end] == ';' || token[end] == ':')) end--;
            if (end >= 0 && token[end] == ']') {
            int i = end - 1;
            while (i >= 0 && token[i] != '[') i--;
            if (i >= 0 && token[i] == '[') {
                len = i;
            } else {
                len = end + 1;
            }
            } else {
            len = end + 1;
            }
        }
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
                error(5, stoken);
                return -1;
            }
        } else if (strcmp(call, "return") == 0) {
            if (argCount == 1) {
                if (strcmp(stoken, "press") != 0) {
                    char *endptr;
                    int value = strtol(stoken, &endptr, 10);
                    if (*endptr != '\0') {
                        int varIdx = findVariable(stoken);
                        if (varIdx == -1) {
                            error(1, stoken);
                            return -1;
                        } else {
                            outputPos += sprintf(output + outputPos, "AD %02X %02X ", vars[varIdx].address & 0xFF, (vars[varIdx].address >> 8) & 0xFF);
                        }
                    } else {
                        outputPos += sprintf(output + outputPos, "A9 %02X ", strtol(stoken, NULL, 10) & 0xFF);
                    }

                    outputPos += sprintf(output + outputPos, "60 ");
                } else {
                    outputPos += sprintf(output + outputPos, "40 ");
                }
                inFunc = false;
            } else {
                error(5, stoken);
                return -1;
            }

        } else if (strcmp(call, "assign") == 0) {
            if (argCount == 1) {
                currentVarIdx = findVariable(stoken);
                if (currentVarIdx == -1) {
                    currentVarIdx = findArray(stoken);
                    if (!arrayIndexVar) buffer2 = 1; else buffer2 = 2;
                    buffer3 = arrayIndex;
                    if (currentVarIdx == -1) {
                        error(1, stoken);
                        return -1;
                    }
                } else {
                    buffer2 = 0;
                }
            } else if (argCount == 2) {
                if (strcmp(stoken, "nest") != 0) {
                    char *endptr;
                    buffer = strtol(stoken, &endptr, 10);
                    if (*endptr != '\0') {
                        error(2, stoken);
                        return -1;
                    }
                    if (buffer2 == 0) {
                        outputPos += sprintf(output + outputPos, "A9 %02X ", buffer);
                    } else if (buffer2 == 1) {
                        outputPos += sprintf(output + outputPos, "A9 %02X A2 %02X ", buffer, buffer3);
                    } else {
                        outputPos += sprintf(output + outputPos, "AE %02X %02X ", buffer3 & 0xFF, (buffer3 >> 8) & 0xFF);
                    }
                }
                if (buffer2 == 0) {
                    outputPos += sprintf(output + outputPos, "8D %02X %02X ", vars[currentVarIdx].address & 0xFF, (vars[currentVarIdx].address >> 8) & 0xFF);
                } else {
                    outputPos += sprintf(output + outputPos, "9D %02X %02X ", arrays[currentVarIdx].address & 0xFF, (vars[currentVarIdx].address >> 8) & 0xFF);
                }
            } else {
                error(5, stoken);
                return -1;
            }

        } else if (strcmp(call, "var") == 0) {
            if (argCount == 1) {
                if (findVariable(stoken) == -1) {
                    currentVarIdx = addVariable(stoken);
                } else {
                    error(11, stoken);
                    return -1;
                }
            } else if (argCount == 2) {
                if (strcmp(stoken, "nest") != 0) {
                    char *endptr;
                    int value = strtol(stoken, &endptr, 10);
                    if (*endptr != '\0') {
                        error(2, stoken);
                        return -1;
                    }
                    outputPos += sprintf(output + outputPos, "A9 %02X ", value);
                }
                outputPos += sprintf(output + outputPos, "8D %02X %02X ", vars[currentVarIdx].address & 0xFF, (vars[currentVarIdx].address >> 8) & 0xFF);
            } else {
                error(5, stoken);
                return -1;
            }

        } else if (strcmp(call, "arr") == 0) {
            if (argCount == 1) {
                if (findVariable(stoken) == -1) {
                    strcpy(cbuffer, stoken);
                } else {
                    error(11, stoken);
                    return -1;
                }
                
            } else if (argCount == 2) {
                char *endptr;
                int value = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    error(2, stoken);
                    return -1;
                }
                currentVarIdx = addArray(cbuffer, value);
                outputPos += sprintf(output + outputPos, "A9 00 8D %02X %02X ", arrays[currentVarIdx].address & 0xFF, (arrays[currentVarIdx].address >> 8) & 0xFF);
                
            } else {
                error(5, stoken);
                return -1;
            }

        } else if (strcmp(call, "if") == 0) {
            if (argCount == 1) {
                int varIdx = findVariable(stoken);
                int arrIdx = findArray(stoken);
                if (varIdx != -1) {
                    strcpy(cbuffer, "AD");
                    buffer = vars[varIdx].address;
                } else if (arrIdx != -1) {
                    strcpy(cbuffer, "AD");
                    buffer = (arrays[arrIdx].address + arrayIndex);
                } else {
                    error(1, stoken);
                    return -1;
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
                    error(6, stoken);
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
                        error(2, stoken);
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
                error(5, stoken);
                return -1;
            }

        } else if (strcmp(call, "while") == 0) {
            if (argCount == 1) {
                int varIdx = findVariable(stoken);
                if (varIdx != -1) {
                    buffer = vars[varIdx].address;
                } else {
                    error(1, stoken);
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
                    error(6, stoken);
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
                        error(1, stoken);
                        return -1;
                    }
                    loopBranch(outputPos/3, buffer, buffer2, buffer3, true);
                }
            } else {
                error(5, stoken);
                return -1;
            }
 
        } else if (strcmp(call, "addto") == 0) {
            if (argCount == 1) {
                int variable = findVariable(stoken);
                int array = findArray(stoken);
                if (variable != -1 ) {
                    buffer = vars[variable].address;
                } else if (array != -1) {
                    buffer = arrays[array].address + arrayIndex;
                } else {
                    error(1, stoken);
                    return -1;
                }                
            } else if (argCount == 2) {
                char *endptr;
                buffer2 = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    error(2, stoken);
                    return -1;
                }
                outputPos += sprintf(output + outputPos, "AD %02X %02X ", buffer & 0xFF, (buffer >> 8) & 0xFF);
                outputPos += sprintf(output + outputPos, "18 69 %02X ", buffer2 & 0xFF);
                outputPos += sprintf(output + outputPos, "8D %02X %02X ", buffer & 0xFF, (buffer >> 8) & 0xFF);
            } else {
                error(5, stoken);
                return -1;
            }
        } else if (strcmp(call, "subfrom") == 0) {
            if (argCount == 1) {
                int variable = findVariable(stoken);
                int array = findArray(stoken);
                if (variable != -1 ) {
                    buffer = vars[variable].address;
                } else if (array != -1) {
                    buffer = arrays[array].address + arrayIndex;
                } else {
                    error(1, stoken);
                    return -1;
                }  
            } else if (argCount == 2) {
                char *endptr;
                buffer2 = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    error(2, stoken);
                    return -1;
                }
                outputPos += sprintf(output + outputPos, "AD %02X %02X ", buffer & 0xFF, (buffer >> 8) & 0xFF);
                outputPos += sprintf(output + outputPos, "38 E9 %02X ", buffer2 & 0xFF);
                outputPos += sprintf(output + outputPos, "8D %02X %02X ", buffer & 0xFF, (buffer >> 8) & 0xFF);
            } else {
                error(5, stoken);
                return -1;
            }
        } else if (strcmp(call, "setPx") == 0) {
            if (argCount == 1) {
                char *endptr;
                buffer = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    int varIdx = findVariable(stoken);
                    int arrIdx = findArray(stoken);
                    if (varIdx != -1) {
                        buffer = vars[varIdx].address;
                        buffer3 = 1;
                    } else if (arrIdx != -1) {
                        buffer = arrays[arrIdx].address + arrayIndex;
                        buffer3 = 1;
                    } else {
                        error(1, stoken);
                        return -1;
                    }
                } else {
                    buffer3 = 0;
                }
            }
            if (argCount == 2) {
                char *endptr;
                buffer2 = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    if (buffer3 != 1) {
                        error(2, stoken);
                        return -1;
                    }
                    int varIdx = findVariable(stoken);
                    int arrIdx = findArray(stoken);
                    if (varIdx != -1) {
                        buffer2 = vars[varIdx].address;
                        buffer3 = 1;
                    } else if (arrIdx != -1) {
                        buffer = arrays[arrIdx].address + arrayIndex;
                        buffer3 = 1;
                    } else {
                        error(1, stoken);
                        return -1;
                    }
                } else {
                    if (buffer3 != 0) {
                        error(1, stoken);
                        return -1;
                    }
                }
            }
            if (argCount == 3) {
                if (strcmp(stoken, "r") == 0 || strcmp(stoken, "g") == 0 || strcmp(stoken, "g") == 0 || strcmp(stoken, "b") == 0 || strcmp(stoken, "x") == 0) {
                    strcpy(cbuffer, stoken);
                } else {
                    error(7, stoken);
                    return -1;
                }

                int isVariable;
                isVariable = buffer3;

                if (strcmp(stoken, "r") == 0) {
                    buffer3 = 0x01;
                } else if (strcmp(stoken, "g") == 0) {
                    buffer3 = 0x10;
                } else if (strcmp(stoken, "b") == 0) {
                    buffer3 = 0x11;
                } else if (strcmp(stoken, "x") == 0) {
                    buffer3 = 0x00;
                }
                
                
                if (isVariable == 0) {
                    if (buffer > 89 || buffer2 > 89) {
                        error(14, 0);
                        return -1;
                    }
                    int addr = (buffer2*90) + (buffer) + 0x4000;
                    outputPos += sprintf(output + outputPos, "A9 %02X ", buffer3);
                    outputPos += sprintf(output + outputPos, "8D %02X %02X ", addr & 0xFF, (addr >> 8) & 0xFF);
                } else {
                    outputPos += sprintf(output + outputPos,
                        "AC %02X %02X " //LDY ycoord
                        "A9 00 "        //LDA #0
                        "85 00 "        //STA $00
                        "85 01 "        //STA $01

                        "18 "           //CLC
                        "69 5A "        //ADC #90
                        "90 02 "        //BCC 02
                        "E6 01 "        //INC $01
                        "88 "           //DEY
                        "D0 F6 "        //BNE F6

                        "18 "           //CLC
                        "6D %02X %02X " //ADC xcoord
                        "90 02 "        //BCC 02
                        "E6 01 "        //INC $01
                        "85 00 "        //STA $00

                        "A5 01 "        //LDA $01
                        "18 "           //CLC
                        "69 40 "        //ADC #$40
                        "85 01 "        //STA $01
                        
                        "A0 00 "        //LDY #00
                        "A9 %02X "      //LDA colour
                        "91 00 ",       //STA
                        buffer & 0xFF, (buffer >> 8) & 0xFF, buffer2 & 0xFF, (buffer2 >> 8) & 0xFF, buffer3 & 0xFF);
                }
            }
        
        } else if (strcmp(call, "setBg") == 0) {
            if (argCount == 1) {
                char *endptr;
                buffer = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    error(2, stoken);
                    return -1;
                }
            } else if (argCount == 2) {
                char *endptr;
                buffer2 = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    error(2, stoken);
                    return -1;
                }
            } else if (argCount == 3) {
                char *endptr;
                buffer3 = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    error(2, stoken);
                    return -1;
                }
                outputPos += sprintf(output + outputPos,
                    "A9 %02X "      //LDA buffer 00000011
                    "8D F3 5F "     //STA 4000 + 1FF4
                    "A9 %02X "      //LDA buffer 00001100
                    "8D F4 5F "     //STA 4000 + 1FF5
                    "A9 %02X "      //LDA buffer 00110000
                    "8D F5 5F "     //STA 4000 + 1FF6
                    "A9 %02X "      //LDA buffer 11000000
                    "8D F6 5F "     //STA 4000 + 1FF7

                    "A9 %02X "      //LDA buffer2 00000011
                    "8D F8 5F "     //STA 4000 + 1FF8
                    "A9 %02X "      //LDA buffer2 00001100
                    "8D F9 5F "     //STA 4000 + 1FF9
                    "A9 %02X "      //LDA buffer2 00110000
                    "8D FA 5F "     //STA 4000 + 1FFA
                    "A9 %02X "      //LDA buffer2 11000000
                    "8D FB 5F "     //STA 4000 + 1FFB

                    "A9 %02X "      //LDA buffer3 00000011
                    "8D FC 5F "     //STA 4000 + 1FFC
                    "A9 %02X "      //LDA buffer3 00001100
                    "8D FD 5F "     //STA 4000 + 1FFD
                    "A9 %02X "      //LDA buffer3 00110000
                    "8D FE 5F "     //STA 4000 + 1FFE
                    "A9 %02X "      //LDA buffer3 11000000
                    "8D FF 5F ",    //STA 4000 + 1FFF
                buffer & 0x03, (buffer >> 2) & 0x03, (buffer >> 4) & 0x03, (buffer >> 6) & 0x03,
                buffer2 & 0x03, (buffer2 >> 2) & 0x03, (buffer2 >> 4) & 0x03, (buffer2 >> 6) & 0x03,
                buffer3 & 0x03, (buffer3 >> 2) & 0x03, (buffer3 >> 4) & 0x03, (buffer3 >> 6) & 0x03);
            } else {
                error(5, stoken);
                return -1;
            }

        } else if (strcmp(call, "sleep") == 0) {
            if (argCount == 1) {
                char *endptr;
                buffer = strtol(stoken, &endptr, 10);
                if (*endptr != '\0') {
                    error(2, stoken);
                    return -1;
                }
                outputPos += sprintf(output + outputPos,
                    "A0 %02X "      //LDY time
                    "A2 C8 "        //LDX #200
                    "CA "           //DEX
                    "D0 FD "        //BNE FD
                    "88 "           //DEY
                    "D0 FA "        //BNE FA
                    , buffer);
            }
        } else if (strcmp(call, "bindPress") == 0) {
            if (argCount == 1) {
                int funcIdx = findFunc(stoken);
                if (funcIdx != -1) {
                    int addr = funcs[funcIdx].address;
                    interruptAddr = addr;
                } else {
                    error(1, stoken);
                    return -1;
                }
            }
        } else if (findVariable(call) != -1) {
            if (argCount == 1) {
                if (strcmp(stoken, "+") == 0) {
                    buffer = 1;
                } else if (strcmp(stoken, "-") == 0) {
                    buffer = 2;
                } else {
                    error(6, stoken);
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
                        error(1, stoken);
                        return -1;
                    }
                } else {
                    buffer2 = strtol(stoken, NULL, 10);
                    buffer3 = 0;
                }
                
                if (buffer == 1) {
                    if (buffer3 == 1) {
                        outputPos += sprintf(output + outputPos, "18 6D %02X %02X ", buffer2 & 0xFF, (buffer2 >> 8) & 0xFF);
                    } else {
                        int stokenValue = strtol(stoken, NULL, 10);
                        outputPos += sprintf(output + outputPos, "18 69 %02X ", stokenValue & 0xFF);
                    }
                } else if (buffer == 2) {
                    if (buffer3 == 1) {
                        outputPos += sprintf(output + outputPos, "38 ED %02X %02X ", buffer2 & 0xFF, (buffer2 >> 8) & 0xFF);
                    } else {
                        int stokenValue = strtol(stoken, NULL, 10);
                        outputPos += sprintf(output + outputPos, "38 E9 %02X ", stokenValue & 0xFF);
                    }
                }
            } else {
                error(5, stoken);
                return -1;
            }
        } else if (isFunc) {
        } else {
            error(1, stoken);
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
    addVariable("p1Up");
    addVariable("p1Down");
    addVariable("p1Left");
    addVariable("p1Right");

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

        col++;
        if (ch == '\n') {
            line++;
            col = 1;
        }
        if (ch == '/') {inComment = !inComment; continue;}
        if (inComment) continue;

        if (ch == '"') {
            inString = !inString;
            if (!inString) parseToken("str"); else strLength = 0;
            strPos = 0;
            continue;
        }

        if (inString) {
            if (strPos > 15) {
                error(15, 0);
                return -1;
            }
            outputPos += sprintf(output + outputPos, "A9 %02X 85 %02X ", (unsigned char)ch, 0xE0 + strPos);
            strPos++;
            strLength++;
            continue;
        }

        if (ch == ';' && inNest) nestCallEnded = true;

        if (ch == '|') {
            if (!inBytes) {
                warn(3, "");
                inBytes = true;
                memset(bytebuffer, 0, sizeof(bytebuffer));
            } else {
                inBytes = false;
                strcpy(token, "");
                outputPos += sprintf(output + outputPos, "%s", bytebuffer);
            }
            continue;
        }

        if (ch == '(') {
            ignoring = true;
            if (!newCall) {
                if (inNest) {
                    error(12, 0);
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

                warned = false;

                argCount = 0;
                strcpy(call, "");
            } else {
                error(4, "(");
                return -1;
            }
        }
        if (ch == ')') {
            if (!nestCallEnded) {
                error(8, ")");
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
                error(8, ")");
                return -1;
            }
            continue;
        }

        if (inBytes) {
            if (isxdigit((unsigned char)ch)) {
                if (bytebufferPos < (int)sizeof(bytebuffer) - 1) {
                    if (bytebufferPos % 3 == 2 && bytebufferPos != 0) {
                        bytebuffer[bytebufferPos++] = ' ';
                    }
                    bytebuffer[bytebufferPos++] = ch;
                    bytebuffer[bytebufferPos] = '\0';
                }
            }
            continue;
        }
        if (isspace(ch) && !inComment && !inString && !inBytes) {
            if (tokenIdx > 0 && strcmp(token, "") != 0) {
                token[tokenIdx] = '\0';
                if (parseToken(token) == -1) return -1;
                tokenIdx = 0;
            }
        } else {
            if (tokenIdx < (int)sizeof(token) - 1 && !ignoring && !inBytes) {
                token[tokenIdx++] = ch;
            }
        }
        
    }
    
    if (tokenIdx > 0) {
        token[tokenIdx] = '\0';
        parseToken(token);
    }
    if (current_branch == -1) {
        if (!warned) {
            printf("\n\033[32mFile compiled successfully.\033[0m\n");
        } else {
            printf("\n\033[32mFile compiled with warning(s).\033[0m\n");
        }
        return 0;
    }
    error(13, 0);
    return -1;
}

int main() {
    printf("RailExperience Binary Creation Tool\n");
    printf("Rail V1.1\n");
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
        memset(funcs, 0, sizeof(funcs));
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

                    int outputSize = 0x4000;
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
                    bytes[0x7FFD] = 0xC0;

                    bytes[0x7FFE] = interruptAddr & 0xFF;
                    bytes[0x7FFF] = (interruptAddr >> 8) & 0xFF;

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