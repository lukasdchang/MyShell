#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <glob.h>
#include <errno.h>
#include <limits.h> // For PATH_MAX

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
char *find_command_path(const char *cmd, char *fullpath);

int main(int argc, char *argv[]) {
    char command[MAX_CMD_LEN];
    ssize_t bytes_read;
    int continue_shell = 1;
    
    if (argc == 2 && !isatty(STDIN_FILENO)) {
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
        if (argc == 2) {
            int script_fd = open(argv[1], O_RDONLY);
            if (script_fd == -1) {
                perror("Error opening script file");
                exit(EXIT_FAILURE);
            }
            char script_buffer[MAX_CMD_LEN * MAX_ARGS];
            ssize_t total_bytes_read = read(script_fd, script_buffer, sizeof(script_buffer));
            if (total_bytes_read < 0) {
                perror("read");
                exit(EXIT_FAILURE);
            }
            script_buffer[total_bytes_read] = '\0';

            char *command_start = script_buffer;
            for (ssize_t i = 0; i < total_bytes_read; ++i) {
                if (script_buffer[i] == '\n' || script_buffer[i] == '\0') {
                    script_buffer[i] = '\0';
                    process_command(command_start, &continue_shell);
                    command_start = script_buffer + i + 1;
                }
            }
            if (command_start < script_buffer + total_bytes_read) {
                process_command(command_start, &continue_shell);
            }

            close(script_fd);
        } else {
            welcome_message();
            while (continue_shell) {
                printf("mysh> ");
                fflush(stdout);
                if ((bytes_read = read(STDIN_FILENO, command, sizeof(command))) < 0) {
                    perror("read");
                    exit(EXIT_FAILURE);
                } else if (bytes_read == 0) {
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
    char *args[MAX_ARGS];
    tokenize_command(cmd, args);
    expand_wildcards(args);
    handle_pipes(args);

    if (strcmp(args[0], "cd") == 0) {
        handle_cd(args);
    } else if (strcmp(args[0], "pwd") == 0) {
        handle_pwd();
    } else if (strcmp(args[0], "which") == 0) {
        handle_which(args);
    } else if (strcmp(args[0], "exit") == 0) {
        *continue_shell = handle_exit(args);
    } else {
        execute_command(args);
    }
}

void execute_command(char *args[MAX_ARGS]) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        char fullpath[PATH_MAX];
        if (!find_command_path(args[0], fullpath)) {
            fprintf(stderr, "%s: Command not found\n", args[0]);
            exit(EXIT_FAILURE);
        }
        handle_redirection(args);
        execv(fullpath, args);
        perror("execv");
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
    }
}

char *find_command_path(const char *cmd, char *fullpath) {
    if (strchr(cmd, '/')) {
        strcpy(fullpath, cmd);
        return fullpath;
    }

    const char *path_env = getenv("PATH");
    if (!path_env) return NULL;

    char *path = strdup(path_env);
    char *dir = strtok(path, ":");

    while (dir != NULL) {
        snprintf(fullpath, PATH_MAX, "%s/%s", dir, cmd);
        if (access(fullpath, X_OK) == 0) {
            free(path);
            return fullpath;
        }
        dir = strtok(NULL, ":");
    }

    free(path);
    return NULL;
}


void handle_cd(char *args[MAX_ARGS]) {
    if (args[1] == NULL) {
        // No directory provided
        fprintf(stderr, "cd: missing argument\n");
        return;
    }
    
    // Pathnames
    if (strchr(args[1], '/') != NULL) {
        // Attempt to change directory to the provided path
        if (chdir(args[1]) != 0) {
            perror("cd");
        }
        return;
    }
    
    // Barenames
    char *directories[] = {"/usr/local/bin", "/usr/bin", "/bin", NULL};
    char path[MAX_CMD_LEN];
    for (int i = 0; directories[i] != NULL; i++) {
        snprintf(path, sizeof(path), "%s/%s", directories[i], args[1]);
        // Check if the directory exists in the current path
        printf("%s\n", path);
        if (access(path, F_OK) == 0) {
            // Attempt to change directory to the found path
            // printf("%s\n", path);
            if (chdir(path) != 0) {
                perror("cd");
            }
            return;
        }
    }
    
    // Directory not found in the specified directories
    fprintf(stderr, "cd: %s: No such file or directory\n", args[1]);
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
    // Check if the correct number of arguments is provided
    if (args[1] == NULL) {
        fprintf(stderr, "which: missing argument\n");
        return;
    } else if (args[2] != NULL) {
        fprintf(stderr, "which: too many arguments\n");
        return;
    }
    
    // Check if the program is a built-in command
    if (strcmp(args[1], "cd") == 0 || strcmp(args[1], "pwd") == 0 || strcmp(args[1], "which") == 0 || strcmp(args[1], "exit") == 0) {
        fprintf(stderr, "which: %s is a built-in command\n", args[1]);
        return;
    }
    
    // Search for the program in the specified directories
    char *directories[] = {"/usr/local/bin", "/usr/bin", "/bin", NULL};
    char path[MAX_CMD_LEN];
    for (int i = 0; directories[i] != NULL; i++) {
        snprintf(path, sizeof(path), "%s/%s", directories[i], args[1]);
        // Check if the program exists in the current path
        if (access(path, F_OK | X_OK) == 0) {
            // Print the path and return
            printf("%s\n", path);
            return;
        }
    }
    
    // Program not found
    fprintf(stderr, "which: %s: command not found\n", args[1]);
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
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0) {
            int fd;
            if (strcmp(args[i], "<") == 0) {
                fd = open(args[i + 1], O_RDONLY);
            } else { // ">"
                fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0640);
            }
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd, strcmp(args[i], "<") == 0 ? STDIN_FILENO : STDOUT_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(fd);

            // Shift args left over redirection operators
            for (int j = i; args[j - 1] != NULL; j++) {
                args[j] = args[j + 2];
            }
            i--; // Adjust index to reflect shifted elements
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