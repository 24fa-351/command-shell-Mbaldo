#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>

#define MAX_INPUT 1024
#define MAX_ARGS 100
#define MAX_ENV_VARS 100

char *resolve_variable(const char *str, char **env_vars, int env_count) {
    static char result[MAX_INPUT];
    result[0] = '\0';


    const char *start = str;
    while (*start) {
        if (*start == '$') {
            start++;
            const char *var_start = start;

            while (*start && (isalnum(*start) || *start == '_')) {
                start++;
            }
            char var_name[MAX_INPUT];
            strncpy(var_name, var_start, start - var_start);
            var_name[start - var_start] = '\0';

            char *value = NULL;
            for (int i = 0; i < env_count; i++) {
                char *eq = strchr(env_vars[i], '=');
                if (eq) {
                    size_t key_length = eq - env_vars[i];
                    if (strncmp(var_name, env_vars[i], key_length) == 0 && key_length == strlen(var_name)) {
                        value = eq + 1;
                        break;
                    }
                }
            }

            if (value) {
                strncat(result, value, MAX_INPUT - strlen(result) - 1);
            }
        } else {
            char temp[2] = {*start, '\0'};
            strncat(result, temp, MAX_INPUT - strlen(result) - 1);
            start++;
        }
    }

    return result;
}

void handle_set_unset(char **args, int *env_count, char **env_vars) {
    if (strcmp(args[0], "set") == 0 && args[1] && args[2]) {
        snprintf(env_vars[*env_count], MAX_INPUT, "%s=%s", args[1], args[2]);
        (*env_count)++;
    } else if (strcmp(args[0], "unset") == 0 && args[1]) {
        for (int i = 0; i < *env_count; i++) {
            if (strncmp(env_vars[i], args[1], strlen(args[1])) == 0 && env_vars[i][strlen(args[1])] == '=') {
                free(env_vars[i]);
                env_vars[i] = NULL;
                for (int j = i; j < *env_count - 1; j++) {
                    env_vars[j] = env_vars[j + 1];
                }
                (*env_count)--;
                break;
            }
        }
    }
}

void handle_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "cd: missing argument\n");
    } else if (chdir(args[1]) == -1) {
        perror("cd");
    }
}

void handle_pwd() {
    char cwd[MAX_INPUT];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("pwd");
    }
}

void handle_echo(char **args, char **env_vars, int env_count) {
    for (int i = 1; args[i]; i++) {
        printf("%s ", resolve_variable(args[i], env_vars, env_count));
    }
    printf("\n");
}

void execute_external_command(char **args) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execvp(args[0], args);
        perror("Command not found");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process
        wait(NULL);
    } else {
        perror("fork");
    }
}

int main() {
    char input[MAX_INPUT];
    char *env_vars[MAX_ENV_VARS];
    int env_count = 0;

    for (int i = 0; i < MAX_ENV_VARS; i++) {
        env_vars[i] = malloc(MAX_INPUT);
        env_vars[i][0] = '\0';
    }

    while (1) {
        printf("xsh# ");
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            break;
        }

        char *cmd = strdup(input);
        char *args[MAX_ARGS];
        char *token = strtok(cmd, " ");
        int i = 0;

        while (token) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        if (args[0] == NULL) {
            free(cmd);
            continue;
        }

        if (strcmp(args[0], "set") == 0 || strcmp(args[0], "unset") == 0) {
            handle_set_unset(args, &env_count, env_vars);
        } else if (strcmp(args[0], "echo") == 0) {
            handle_echo(args, env_vars, env_count);
        } else if (strcmp(args[0], "cd") == 0) {
            handle_cd(args);
        } else if (strcmp(args[0], "pwd") == 0) {
            handle_pwd();
        } else {
            execute_external_command(args);
        }

        free(cmd);
    }

    for (int i = 0; i < env_count; i++) {
        free(env_vars[i]);
    }

    return 0;
}
