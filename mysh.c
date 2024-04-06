#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_TOKENS 64
#define MAX_TOKEN_LENGTH 64

// Function prototypes
void execute_command(char *tokens[], int num_tokens);
void handle_redirection(char *tokens[], int num_tokens);
void handle_pipes(char *tokens[], int num_tokens);
void handle_builtin_commands(char *tokens[], int num_tokens);

int main(int argc, char *argv[]) {
    char input[MAX_COMMAND_LENGTH];
    char *tokens[MAX_TOKENS];
    int num_tokens;

    // Determine whether in interactive or batch mode
    int interactive_mode = isatty(STDIN_FILENO);

    if (interactive_mode) {
        printf("Welcome to my shell!\n");
        printf("mysh> ");
        fflush(stdout);
    }

    // Read commands from standard input or batch file
    while (fgets(input, sizeof(input), stdin) != NULL) {
        // Remove newline character
        input[strcspn(input, "\n")] = '\0';

        // Tokenize input
        char *token = strtok(input, " ");
        num_tokens = 0;
        while (token != NULL && num_tokens < MAX_TOKENS - 1) {
            tokens[num_tokens++] = token;
            token = strtok(NULL, " ");
        }
        tokens[num_tokens] = NULL;

        // Execute command
        execute_command(tokens, num_tokens);

        if (interactive_mode) {
            printf("mysh> ");
            fflush(stdout);
        }
    }

    if (interactive_mode) {
        printf("Exiting my shell.\n");
    }

    return 0;
}

void execute_command(char *tokens[], int num_tokens) {
    if (num_tokens == 0) {
        return;  // Empty command
    }

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

void handle_redirection(char *tokens[], int num_tokens) {
    // Check for redirection symbols "<" and ">"
    for (int i = 0; i < num_tokens; i++) {
        if (strcmp(tokens[i], "<") == 0) {
            // Input redirection
            freopen(tokens[i + 1], "r", stdin);
            tokens[i] = NULL;  // Remove redirection symbol
        } else if (strcmp(tokens[i], ">") == 0) {
            // Output redirection
            freopen(tokens[i + 1], "w", stdout);
            tokens[i] = NULL;  // Remove redirection symbol
        }
    }
}

void handle_pipes(char *tokens[], int num_tokens) {
    // Check for pipe symbol "|"
    for (int i = 0; i < num_tokens; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            // Create pipe
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }

            // Fork first child process
            pid_t pid1 = fork();
            if (pid1 == 0) {
                // Child process
                close(pipefd[0]);  // Close read end
                dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to pipe
                close(pipefd[1]);
                execvp(tokens[0], tokens);
                perror("execvp");
                exit(EXIT_FAILURE);
            } else if (pid1 < 0) {
                // Error forking
                perror("fork");
                exit(EXIT_FAILURE);
            }

            // Fork second child process
            pid_t pid2 = fork();
            if (pid2 == 0) {
                // Child process
                close(pipefd[1]);  // Close write end
                dup2(pipefd[0], STDIN_FILENO);  // Redirect stdin to pipe
                close(pipefd[0]);
                execvp(tokens[i + 1], &tokens[i + 1]);
                perror("execvp");
                exit(EXIT_FAILURE);
            } else if (pid2 < 0) {
                // Error forking
                perror("fork");
                exit(EXIT_FAILURE);
            }

            // Close pipe ends in parent process
            close(pipefd[0]);
            close(pipefd[1]);

            // Wait for child processes to finish
            wait(NULL);
            wait(NULL);

            return;
        }
    }
}

void handle_builtin_commands(char *tokens[], int num_tokens) {
    if (strcmp(tokens[0], "cd") == 0) {
        if (num_tokens != 2) {
            fprintf(stderr, "cd: Invalid number of arguments\n");
        } else {
            if (chdir(tokens[1]) != 0) {
                perror("chdir");
            }
        }
        exit(EXIT_SUCCESS);
    } else if (strcmp(tokens[0], "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("getcwd");
        }
        exit(EXIT_SUCCESS);
    } else if (strcmp(tokens[0], "which") == 0) {
        if (num_tokens != 2) {
            fprintf(stderr, "which: Invalid number of arguments\n");
        } else {
            // Implement which command
            // Example: which(tokens[1]);
        }
        exit(EXIT_SUCCESS);
    } else if (strcmp(tokens[0], "exit") == 0) {
        for (int i = 1; i < num_tokens; i++) {
            printf("%s ", tokens[i]);
        }
        printf("\n");
        exit(EXIT_SUCCESS);
    }
}
