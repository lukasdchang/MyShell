#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <glob.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h> // Importing PATH_MAX

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64
int last_command_success = 1; // Tracks the success of the last executed command (1 = success, 0 = failure)

// Function declarations
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
    int continue_shell = 1; // Determines whether to continue running the shell
    
    // Check for batch mode versus interactive mode
    if (argc == 2 && !isatty(STDIN_FILENO)) {
        // Operating in batch mode with input redirection
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
        // Check for batch mode by directly providing a script file
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
            // Operating in interactive mode
            welcome_message();
            while (continue_shell) {
                printf("mysh> ");
                fflush(stdout);
                if ((bytes_read = read(STDIN_FILENO, command, sizeof(command))) < 0) {
                    perror("read");
                    exit(EXIT_FAILURE);
                } else if (bytes_read == 0) {
                    // Reached end of input
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
    // Skipping leading whitespace and comments
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    
    if (*cmd == '#' || *cmd == '\0') {
        return; // Skip comments and empty lines
    }

    char *args[MAX_ARGS];
    tokenize_command(cmd, args);

    // If no command is given, just return
    if (args[0] == NULL) {
        return;
    }

    // Handling conditional execution based on previous command's success
    if (strcmp(args[0], "then") == 0) {
        if (last_command_success != 1) {
            // Skip the current command if the last one failed
            return;
        }
        // Execute the command following 'then'
        for (int i = 0; args[i] != NULL; i++) {
            args[i] = args[i + 1];
        }
    } else if (strcmp(args[0], "else") == 0) {
        if (last_command_success != 0) {
            // Skip the current command if the last one succeeded
            return;
        }
        // Execute the command following 'else'
        for (int i = 0; args[i] != NULL; i++) {
            args[i] = args[i + 1];
        }
    }

    // Expand any wildcards before further processing
    expand_wildcards(args);

    // Look for pipes in the command and handle if found
    int pipe_pos = -1;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_pos = i;
            break;
        }
    }

    if (pipe_pos != -1) {
        // If a pipe is found, handle it and then return
        handle_pipes(args);
        return;
    }
    
    // Processing built-in commands
    if (strcmp(args[0], "cd") == 0) {
        handle_cd(args);
    } else if (strcmp(args[0], "pwd") == 0) {
        handle_pwd();
    } else if (strcmp(args[0], "which") == 0) {
        handle_which(args);
    } else if (strcmp(args[0], "exit") == 0) {
        *continue_shell = handle_exit(args);
    } else {
        // If it's not a built-in command, try executing it
        execute_command(args);
    }
}

char *find_command_path(const char *cmd, char *fullpath) {
    // Check if command already includes a path
    if (strchr(cmd, '/')) {
        strcpy(fullpath, cmd);
        return fullpath;
    }

    // Search for command in PATH directories
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

void execute_command(char *args[MAX_ARGS]) {
    // Forking a new process to execute the command
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // In the child process
        char fullpath[PATH_MAX];
        if (!find_command_path(args[0], fullpath)) {
            fprintf(stderr, "%s: Command not found\n", args[0]);
            exit(EXIT_FAILURE);
        }
        
        // Handle any redirections before executing
        handle_redirection(args);

        execv(fullpath, args);
        perror("execv"); // execv only returns on error
        exit(EXIT_FAILURE);
    } else {
        // Parent process waits for child to complete
        int status;
        waitpid(pid, &status, 0);
        // Update last_command_success based on child's exit status
        if (WIFEXITED(status)) {
            last_command_success = (WEXITSTATUS(status) == 0) ? 1 : 0;
        } else {
            last_command_success = 0; // Treat as failure if child didn't exit normally
        }
    }

    if (strcmp(args[0], "cat") == 0) { // Add new line for formatting if 'cat' was used
        printf("\n");
    }
}

// Definitions for other functions like handle_cd, handle_pwd, etc., are omitted for brevity.

void handle_cd(char *args[MAX_ARGS]) {
    // Check if the correct number of arguments is provided.
    if (args[1] == NULL || args[2] != NULL) {
        fprintf(stderr, "cd: wrong number of arguments\n");
        return;
    }

    // Attempt to change to the directory specified by args[1].
    if (chdir(args[1]) != 0) {
        // On failure, perror will display the error relative to the command 'cd'.
        perror("cd");
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
    int i = 0;
    char *cursor = cmd;
    char *end;
    while (*cursor != '\0' && i < MAX_ARGS - 1) {
        while (isspace((unsigned char)*cursor)) cursor++;  // Skip leading spaces

        char quote_char = '\0';
        if (*cursor == '"' || *cursor == '\'') {  // Start of a quoted argument
            quote_char = *cursor;  // Remember the type of quote
            cursor++;  // Skip the opening quote
        }
        end = cursor;

        if (quote_char != '\0') {
            // Find the closing quote that matches the opening quote
            while (*end != '\0' && *end != quote_char) end++;
            if (*end == '\0') {
                // Syntax error: unmatched quote
                fprintf(stderr, "Unmatched quote in command.\n");
                args[i++] = NULL;  // Terminate arguments list
                return;
            }
        } else {
            // Find the end of the unquoted argument
            while (*end != '\0' && !isspace((unsigned char)*end)) end++;
        }

        if (*end == '\0') {  // End of command
            args[i++] = cursor;
            break;
        } else {  // Middle of command
            *end = '\0';  // Terminate the current argument
            args[i++] = cursor;
            cursor = end + 1;  // Move past the end of the current argument
            if (quote_char != '\0') cursor++;  // If inside quotes, move past the closing quote
        }
    }
    args[i] = NULL;  // Null-terminate the list of arguments
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
    // Pipe handling needs to be adjusted to correctly split and execute commands
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    int pipe_pos;
    for (pipe_pos = 0; args[pipe_pos] != NULL; pipe_pos++) {
        if (strcmp(args[pipe_pos], "|") == 0) {
            args[pipe_pos] = NULL; // Split the command at the pipe symbol
            break;
        }
    }

    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid1 == 0) {
        // First child: executes the command before the pipe
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execute_command(args);
        exit(EXIT_FAILURE);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid2 == 0) {
        // Second child: executes the command after the pipe
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        execute_command(&args[pipe_pos + 1]);
        exit(EXIT_FAILURE);
    }

    // Close pipe ends in the parent
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}