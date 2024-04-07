#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <glob.h>
#include <errno.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64

void welcome_message();
void goodbye_message();
void process_command(char *cmd, int *continue_shell);
void execute_command(char *args[MAX_ARGS]);
void handle_cd(char *args[MAX_ARGS]);
void handle_pwd();
void handle_which(char *args[MAX_ARGS]);
int handle_exit(char *args[MAX_ARGS]);
void tokenize_command(char *cmd, char *args[MAX_ARGS]);
void expand_wildcards(char *args[MAX_ARGS]);
void handle_redirection(char *args[MAX_ARGS]);
void handle_pipes(char *args[MAX_ARGS]);

int main(int argc, char *argv[]) {
    char command[MAX_CMD_LEN];
    ssize_t bytes_read;
    int continue_shell = 1; // Flag to indicate whether the shell should continue running
    
    // Check if running in batch mode or interactive mode
    if (argc == 2 && !isatty(STDIN_FILENO)) {
        // Batch mode (input redirected)
        int script_fd = open(argv[1], O_RDONLY);
        if (script_fd == -1) {
            perror("Error opening script file");
            exit(EXIT_FAILURE);
        }
        while ((bytes_read = read(script_fd, command, sizeof(command))) > 0) {
            command[bytes_read] = '\0';
            process_command(command, &continue_shell);
        }
        close(script_fd);
    } else {
        // If script file is provided as command line argument, execute it
        if (argc == 2) {
            int script_fd = open(argv[1], O_RDONLY);
            if (script_fd == -1) {
                perror("Error opening script file");
                exit(EXIT_FAILURE);
            }
            while ((bytes_read = read(script_fd, command, sizeof(command))) > 0) {
                command[bytes_read] = '\0';
                process_command(command, &continue_shell);
            }
            close(script_fd);
        } else {
            // Interactive mode
            welcome_message();
            while (continue_shell) {
                printf("mysh> ");
                fflush(stdout);
                if ((bytes_read = read(STDIN_FILENO, command, sizeof(command))) < 0) {
                    perror("read");
                    exit(EXIT_FAILURE);
                } else if (bytes_read == 0) {
                    // End of input stream
                    break;
                }
                command[bytes_read] = '\0';
                process_command(command, &continue_shell);
            }
            goodbye_message();
        }
    }
    
    return 0;
}

void welcome_message() {
    printf("Welcome to my shell!\n");
}

void goodbye_message() {
    printf("Exiting my shell.\n");
}

void process_command(char *cmd, int *continue_shell) {
    // Tokenize the command
    char *args[MAX_ARGS];
    tokenize_command(cmd, args);
    
    // Expand wildcards
    expand_wildcards(args);
    
    // Handle redirection
    handle_redirection(args);
    
    // Handle pipes
    handle_pipes(args);
    
    // Check for built-in commands
    if (strcmp(args[0], "cd") == 0) {
        handle_cd(args);
    } else if (strcmp(args[0], "pwd") == 0) {
        handle_pwd();
    } else if (strcmp(args[0], "which") == 0) {
        handle_which(args);
    } else if (strcmp(args[0], "exit") == 0) {
        *continue_shell = handle_exit(args); // Modify continue_shell flag
    } else {
        // External command
        execute_command(args);
    }
}

void execute_command(char *args[MAX_ARGS]) {
    // Execute the command using execvp()
    // Fork a child process
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        execvp(args[0], args);
        // If execvp returns, there was an error
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        // Wait for child to terminate
        int status;
        waitpid(pid, &status, 0);
        // Set last exit status
        // Use WIFEXITED and WEXITSTATUS macros to get child exit status
    }
}

void handle_cd(char *args[MAX_ARGS]) {
    if (args[1] == NULL) {
        // No directory provided
        fprintf(stderr, "cd: missing argument\n");
    } else {
        // Check if the first argument contains a slash character
        if (strchr(args[1], '/') != NULL) {
            // Attempt to change directory to the provided path
            if (chdir(args[1]) != 0) {
                perror("cd");
            }
        } else {
            // No slash character, treat it as a regular directory name
            // Change directory using the provided name
            if (chdir(args[1]) != 0) {
                perror("cd");
            }
        }
    }
}

void handle_pwd() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("getcwd");
    }
}

void handle_which(char *args[MAX_ARGS]) {
    if (args[1] == NULL) {
        // No program name provided
        fprintf(stderr, "which: missing argument\n");
    } else {
        // Search for the program in PATH
        // Implement your search logic here
        char *path = getenv("PATH");
        char *token = strtok(path, ":");
        while (token != NULL) {
            char command[MAX_CMD_LEN];
            snprintf(command, sizeof(command), "%s/%s", token, args[1]);
            if (access(command, X_OK) == 0) {
                printf("%s\n", command);
                return;
            }
            token = strtok(NULL, ":");
        }
        // Program not found
        fprintf(stderr, "%s: command not found\n", args[1]);
    }
}

int handle_exit(char *args[MAX_ARGS]) {
    // Print arguments
    for (int i = 1; args[i] != NULL; i++) {
        printf("%s ", args[i]);
    }
    printf("\n");
    return 0; // Indicate that the shell should continue running
}

void tokenize_command(char *cmd, char *args[MAX_ARGS]) {
    // Tokenize the command
    char *token;
    int i = 0;
    token = strtok(cmd, " \n");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \n");
    }
    args[i] = NULL;
}

void expand_wildcards(char *args[MAX_ARGS]) {
    // Expand wildcards in arguments
    for (int i = 0; args[i] != NULL; i++) {
        glob_t globbuf;
        int glob_flags = 0;
        if (strchr(args[i], '*') != NULL || strchr(args[i], '?') != NULL) {
            glob(args[i], glob_flags, NULL, &globbuf);
            if (globbuf.gl_pathc > 0) {
                // Replace wildcard argument with expanded list
                for (int j = 0; j < globbuf.gl_pathc; j++) {
                    args[i + j] = globbuf.gl_pathv[j];
                }
                args[i + globbuf.gl_pathc] = NULL;
                i += globbuf.gl_pathc - 1;
            } else {
                // No matches found
                globfree(&globbuf);
            }
        }
    }
}

void handle_redirection(char *args[MAX_ARGS]) {
    // Handle input/output redirection
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            // Input redirection
            int fd = open(args[i + 1], O_RDONLY);
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDIN_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(fd);
            // Remove redirection tokens and filename from argument list
            args[i] = NULL;
            args[i + 1] = NULL;
        } else if (strcmp(args[i], ">") == 0) {
            // Output redirection
            int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(fd);
            // Remove redirection tokens and filename from argument list
            args[i] = NULL;
            args[i + 1] = NULL;
        }
    }
}

void handle_pipes(char *args[MAX_ARGS]) {
    // Handle pipes
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            // Create pipe
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
            // Fork a child process for the left side of the pipe
            pid_t pid_left = fork();
            if (pid_left == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            } else if (pid_left == 0) {
                // Child process (left side)
                // Replace stdout with write end of pipe
                if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                // Close unused read end of pipe
                close(pipefd[0]);
                close(pipefd[1]);
                // Execute command before pipe
                execute_command(args);
                exit(EXIT_SUCCESS);
            }
            // Fork another child process for the right side of the pipe
            pid_t pid_right = fork();
            if (pid_right == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            } else if (pid_right == 0) {
                // Child process (right side)
                // Replace stdin with read end of pipe
                if (dup2(pipefd[0], STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                // Close unused write end of pipe
                close(pipefd[1]);
                close(pipefd[0]);
                // Execute command after pipe
                execute_command(args + i + 1);
                exit(EXIT_SUCCESS);
            }
            // Close both ends of the pipe in the parent process
            close(pipefd[0]);
            close(pipefd[1]);
            // Wait for both child processes to finish
            waitpid(pid_left, NULL, 0);
            waitpid(pid_right, NULL, 0);
            return;
        }
    }
}
