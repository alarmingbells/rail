#include <stdio.h>
#include <string.h>

char commandEntered[99];

int compile(char file[99]) {
    return 0;
}

int main() {
    while (1) {
        printf("RAIL>");
        fgets(commandEntered, sizeof(commandEntered), stdin);

        char command[99];
        char operand[99];

        int semiReached = 0;
        int operandReached = 0;
        int commandIdx = 0, operandIdx = 0;
        for (int i = 0; i < strlen(commandEntered); i++) {
            if (!semiReached) {
                if (commandEntered[i] == ';') {
                    semiReached = 1;
                } else if (commandEntered[i] == ' ') {
                    operandReached = 1;
                } else {
                    if (!operandReached) {
                        command[commandIdx++] = commandEntered[i];
                    } else {
                        operand[operandIdx++] = commandEntered[i];
                    }
                }
            }
        }
        
        if (!semiReached) {
            printf("RAIL command error: Unexpected end of statement at line 1 collumn %d\n", (int)strlen(command));
            continue;;
        }

        if (strcmp(command, "exit") == 0) {
            printf("Exiting RAIL\n");
            break;
        } else if (strcmp(command, "help") == 0) {
            printf("Available commands: help, exit\n");
        } else if (strcmp(command, "rail") == 0) {
            printf("Starting RAIL compilation of '%s'\n", operand);
            int sucess = compile(operand);
            if (sucess) {
                printf("RAIL COMPILER: Compiled sucessfully.\n");
            } else {
                printf("RAIL COMPILER: Failed to compile '%s'\n", operand);
            }
        } else {
            printf("RAIL command error: Unknown command: %s\n", command);
        }
    }
}