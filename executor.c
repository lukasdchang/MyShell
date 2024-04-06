#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_TOKENS 64

// Function prototypes
void execute_command(char *tokens[], int num_tokens);
void handle_builtin_commands(char *tokens[], int num_tokens);
void handle_redirection(char *tokens[], int num_tokens);
void handle_pipes(char *tokens[], int num_tokens);

void execute_command(char *tokens[], int num_tokens) {
    // Check for built-in commands
    handle_builtin_commands(tokens, num_tokens);

    // Handle redirection and pipes
    handle_redirection(tokens, num_tokens);
    handle_pipes(tokens, num_tokens);

    // Execute external command
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execvp(tokens[0], tokens);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Error forking
        perror("fork");
    } else {
        // Parent process
        wait(NULL); // Wait for child to finish
    }
}

void handle_builtin_commands(char *tokens[], int num_tokens) {
    // Check if the command is a built-in command and handle it if it is
    if (strcmp(tokens[0], "cd") == 0) {
        // Handle 'cd' command
        if (num_tokens == 2) {
            if (chdir(tokens[1]) != 0) {
                perror("cd");
            }
        } else {
            fprintf(stderr, "cd: Invalid number of arguments\n");
        }
    } else if (strcmp(tokens[0], "pwd") == 0) {
        // Handle 'pwd' command
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("pwd");
        }
    } else if (strcmp(tokens[0], "which") == 0) {
        // Handle 'which' command
        if (num_tokens == 2) {
            // Search for the command in the PATH
            char *path = getenv("PATH");
            if (path != NULL) {
                char *path_copy = strdup(path);
                char *token = strtok(path_copy, ":");
                while (token != NULL) {
                    char command_path[1024];
                    snprintf(command_path, sizeof(command_path), "%s/%s", token, tokens[1]);
                    if (access(command_path, X_OK) == 0) {
                        printf("%s\n", command_path);
                        free(path_copy);
                        return;
                    }
                    token = strtok(NULL, ":");
                }
                free(path_copy);
            }
            fprintf(stderr, "which: %s: Command not found\n", tokens[1]);
        } else {
            fprintf(stderr, "which: Invalid number of arguments\n");
        }
    } else if (strcmp(tokens[0], "exit") == 0) {
        // Handle 'exit' command
        exit(EXIT_SUCCESS);
    }
}

void handle_redirection(char *tokens[], int num_tokens) {
    // Check for redirection symbols ("<" and ">") and handle them if necessary
    int i;
    for (i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "<") == 0) {
            // Input redirection
            int fd_in = open(tokens[i + 1], O_RDONLY);
            if (fd_in == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
            tokens[i] = NULL; // Remove redirection token
        } else if (strcmp(tokens[i], ">") == 0) {
            // Output redirection
            int fd_out = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
            tokens[i] = NULL; // Remove redirection token
        }
    }
}

void handle_pipes(char *tokens[], int num_tokens) {
    // Check for pipe symbols ("|") and handle them if necessary
    int i;
    for (i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            // Found a pipe, split the command into two parts
            tokens[i] = NULL; // Terminate the first command
            char **cmd1 = tokens;
            char **cmd2 = &tokens[i + 1];

            // Create pipe
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }

            // Fork process for first command
            pid_t pid1 = fork();
            if (pid1 == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            } else if (pid1 == 0) {
                // Child process for first command
                // Redirect stdout to write end of pipe
                close(pipefd[0]); // Close read end of pipe
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]); // Close write end of pipe
                execvp(cmd1[0], cmd1);
                perror("execvp");
                exit(EXIT_FAILURE);
            }

            // Fork process for second command
            pid_t pid2 = fork();
            if (pid2 == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            } else if (pid2 == 0) {
                // Child process for second command
                // Redirect stdin to read end of pipe
                close(pipefd[1]); // Close write end of pipe
                dup2(pipefd[0], STDIN_FILENO);
                close(pipefd[0]); // Close read end of pipe
                execvp(cmd2[0], cmd2);
                perror("execvp");
                exit(EXIT_FAILURE);
            }

            // Close pipe ends in parent process
            close(pipefd[0]);
            close(pipefd[1]);

            // Wait for both child processes to finish
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);

            // Restore pipe symbol
            tokens[i] = "|";
            return;
        }
    }
}
