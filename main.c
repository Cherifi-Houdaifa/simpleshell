#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

enum tokentype {
    cmd,
    and, // &&
    or, //  ||
    nop,
    redirectwrite,  // >
    redirectapprend // >>
};

struct cmdtoken {
    char* value;
    enum tokentype type;
    struct cmdtoken* next;
};

struct cmd {
    struct cmdtoken* command;
    enum tokentype operator;
    struct cmd* next;
};
struct cmd* commands = NULL;

struct cmdtoken* tokens = NULL;

void lexcommands(char* cmdstr, size_t len) {
    size_t i = 0;
    bool incmd = false;
    size_t cmdstart;

    while (i < len) {
        if (!incmd) {
            incmd = true;
            cmdstart = i;
        }
        if (cmdstr[i] == ' ' || cmdstr[i] == '\n') {
            cmdstr[i] = '\0';

            enum tokentype type;

            char* currentstr = &cmdstr[cmdstart];
            if (strcmp(currentstr, "&&") == 0) {
                type = and;
            } else if (strcmp(currentstr, "||") == 0) {
                type = or;
            } else if (strcmp(currentstr, ">") == 0) {
                type = redirectwrite;
            } else if (strcmp(currentstr, ">>") == 0) {
                type = redirectapprend;
            } else {
                type = cmd;
            }

            // insert command into list
            if (tokens == NULL) {
                tokens = malloc(sizeof(struct cmdtoken));
                tokens->value = currentstr;
                tokens->type = type;
                tokens->next = NULL;
            } else {
                struct cmdtoken* temp = tokens;
                while (temp->next != NULL)
                    temp = temp->next;
                temp->next = malloc(sizeof(struct cmdtoken));
                temp->next->value = currentstr;
                temp->next->type = type;
                temp->next->next = NULL;
            }
            incmd = false;
        }
        i++;
    }
    // struct cmdtoken* temp = tokens;
    // while (temp != NULL) {
    //     printf("%s: %s\n", temp->type == and ? "&&" : (temp->type == or ? "||" : "cmd"), temp->value);
    //     temp = temp->next;
    // }
}


// commands are represented in a list of lists
// each command stores the operator betweens it and the next command (|| or &&)
// command1 -- command2 -- ...
//   op=||
//    ls
//     |
//    -a

void parsecommands() {
    struct cmdtoken* temp = tokens;
    if (temp->type == and || temp->type == or) {
        perror("commands can't start with && or ||");
    }
    struct cmdtoken* cmdstart = temp;
    while (temp->next != NULL) {
        if (temp->next->type == and || temp->next->type == or) {
            if (temp->next->next == NULL) {
                perror("you can't && or || with nothing");
            }
            if (temp->next->next->type == and || temp->next->next->type == or) {
                perror("you can't && or || with && or ||");
            }


            // add a new command
            if (commands == NULL) {
                commands = malloc(sizeof(struct cmd));
                commands->command = cmdstart;
                commands->operator = temp->next->type;
                commands->next = NULL;
            } else {
                struct cmd* temp2 = commands;
                while (temp2->next != NULL)
                    temp2 = temp2->next;
                temp2->next = malloc(sizeof(struct cmd));
                temp2->next->command = cmdstart;
                temp2->next->operator = temp->next->type;
                temp2->next->next = NULL;
            }
            cmdstart = temp->next->next;

            // temp->next points to the operator token, we don't need it anymore
            free(temp->next);
            temp->next = NULL;
            temp = cmdstart;
            continue;
        }

        temp = temp->next;
    }
    if (commands == NULL) {
        commands = malloc(sizeof(struct cmd));
        commands->command = cmdstart;
        commands->operator = nop;
        commands->next = NULL;
    } else {
        struct cmd* temp2 = commands;
        while (temp2->next != NULL)
            temp2 = temp2->next;
        temp2->next = malloc(sizeof(struct cmd));
        temp2->next->command = cmdstart;
        temp2->next->operator = nop;
        temp2->next->next = NULL;
    }
    // for (struct cmd* i = commands; i != NULL; i = i->next) {
    //     for (struct cmdtoken* j = i->command; j!=NULL; j = j->next) {
    //         printf("%s ", j->value);
    //     }
    //     printf("  %s  ", i->operator == nop ? "end" : (i->operator == and ? "&&" : "||"));
    // }
    // puts("");
}

void runcommands() {
    int exitcode = 0;
    struct cmd* i = commands;
    while (i != NULL) {
        size_t cmdlen = 0;
        char* redirectpath = NULL;
        int redirectfd = -1;
        enum tokentype redirecttype;
        
        
        for (struct cmdtoken* j = i->command; j != NULL; j = j->next) {
            if (j->type == redirectwrite || j->type == redirectapprend) {
                if (j->next == NULL) {
                    perror("you have to add redirection path");
                }
                if (j->next->next != NULL) {
                    perror("you can't have anything after the redirect path");
                }
                redirectpath = j->next->value;
                redirecttype = j->type;
                break;
            }
            cmdlen++;
        }
        char** argv = malloc(sizeof(char*) * (cmdlen + 1));
        const char* path = i->command->value;
        size_t index = 0;
        for (struct cmdtoken* j = i->command; j != NULL && index < cmdlen; j = j->next) {
            argv[index] = j->value;
            index++;
        }
        argv[index] = NULL;
        
        if (redirectpath != NULL) {
            if (redirecttype == redirectwrite) {
                redirectfd = open(redirectpath, O_CREAT | O_TRUNC | O_WRONLY, 0644);
            } else if (redirecttype == redirectapprend) {
                redirectfd = open(redirectpath, O_CREAT | O_WRONLY | O_APPEND, 0644);
            }
        }
        
        
        pid_t pid = fork();
        if (pid == 0) {
            // in child
            if (redirectfd != -1) {
                dup2(redirectfd, 1);
            }
            execvp(path, argv);
            perror("execvp: ");
        } else {
            // in parent
            int status;
            waitpid(pid, &status, 0);
            close(redirectfd);
            free(argv);
            
            if (WIFEXITED(status)) {
                exitcode = WEXITSTATUS(status);
            }
            if (i->operator == and && exitcode != 0) {
                i = i->next->next;
                continue;
            } else if (i->operator == or && exitcode == 0) {
                i = i->next->next;
                continue;
            }
        }
        i = i->next;

    }
}


int main (int argc, char *argv[]) {   
    setbuf(stdin, 0);
    setbuf(stdout, 0);
    setbuf(stderr, 0);

    

    char* cmdstr = NULL;
    size_t len = 0;
    

    while (true) {
        printf("$ ");
        if (getline(&cmdstr, &len, stdin) == -1) {
            perror("getline");
        }

        lexcommands(cmdstr, len);
        parsecommands();
        runcommands();

        // free the commands and tokens
        // more like free only commands since it include items from tokens  
        tokens = NULL;
        struct cmd* i = commands;
        while (i != NULL) {
            struct cmdtoken* j = i->command;
            struct cmdtoken* temp1;
            while (j != NULL) {
                temp1 = j;
                j = j->next;
                free(temp1);
            }

            struct cmd* temp2 = i;
            i = i->next;
            free(temp2);
        }
        free(cmdstr);
        cmdstr = NULL;
        len = 0;
        commands = NULL;
    }
    

    return 0;
}