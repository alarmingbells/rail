#include <stdio.h>
#include <string.h>

//\033[31m  \033[0m

char commandEntered[99];

int appStart(int appId) {
    char appname[99] = "";
    switch (appId) {
        case 1:
            strcpy(appname, "FILE");
            break;
        default:
            break;
    }
    while (1) {
        printf("RAIL/\033[33m%s\033[0m>", appname);
        fgets(commandEntered, sizeof(commandEntered), stdin);

        size_t len = strlen(commandEntered);
        if (len > 0 && commandEntered[len - 1] == '\n') {
            commandEntered[len - 1] = '\0';
        }

        if (strcmp(commandEntered, "exit;") == 0) {
            printf("Exiting \033[33m%s\033[0m\n", appname);
            break;
        }
        printf("%s\n", commandEntered);
    }

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

        command[commandIdx] = '\0';
        operand[operandIdx] = '\0';
        
        if (!semiReached) {
            printf("\033[31mRAIL command error:\033[0m Unexpected end of statement at line 1 collumn %d\n", (int)strlen(command));
        } else {
            if (strcmp(command, "exit") == 0) {
                printf("Exiting RAIL\n");
                break;
            } else if (strcmp(command, "help") == 0) {
                printf("Available commands: help, exit\n");
            } else if (strcmp(command, "rail") == 0) {
                
            } else if (strcmp(command, "start") == 0) {
                if (strcmp(operand, "file") == 0) {
                    appStart(1);
                } else {
                    printf("\033[31mRAIL app start error:\033[0m Unknown app '%s'", operand);
                }
            } else {
                printf("\033[31mRAIL command error:\033[0m Unknown command '%s'\n", command);
            }
        }
    }
}