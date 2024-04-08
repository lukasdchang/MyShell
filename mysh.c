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
        // Inside the else block for batch mode processing with argc == 2
    if (argc == 2) {
        int script_fd = open(argv[1], O_RDONLY);
        if (script_fd == -1) {
            perror("Error opening script file");
            exit(EXIT_FAILURE);
        }
        char script_buffer[MAX_CMD_LEN * MAX_ARGS]; // Adjust size as necessary
        ssize_t total_bytes_read = read(script_fd, script_buffer, sizeof(script_buffer));
        if (total_bytes_read < 0) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        script_buffer[total_bytes_read] = '\0'; // Null-terminate the string

        char *command_start = script_buffer;
        for (ssize_t i = 0; i < total_bytes_read; ++i) {
            if (script_buffer[i] == '\n' || script_buffer[i] == '\0') {
                script_buffer[i] = '\0'; // Replace newline with null terminator
                process_command(command_start, &continue_shell);
                command_start = script_buffer + i + 1; // Move to the start of the next command
            }
        }
        if (command_start < script_buffer + total_bytes_read) {
            process_command(command_start, &continue_shell);
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
    
    int original_stdin = dup(STDIN_FILENO);
    int original_stdout = dup(STDOUT_FILENO);
    if (original_stdin == -1 || original_stdout == -1) {
        perror("dup");
        exit(EXIT_FAILURE);
    }

    expand_wildcards(args);
    
    handle_redirection(args);
    
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
        //print args here//
        // printf("Arguments: ");
        // for (int i = 0; args[i] != NULL; i++) {
        //     printf("%s ", args[i]);
        // }
        // printf("\n");
        execute_command(args);
    }
    
    if (dup2(original_stdin, STDIN_FILENO) == -1 || dup2(original_stdout, STDOUT_FILENO) == -1) {
        perror("dup2");
        exit(EXIT_FAILURE);
    }
    close(original_stdin);
    close(original_stdout);
}

void execute_command(char *args[MAX_ARGS]) {
    // Execute the command using execvp()
    // Fork a child process
    pid_t pid = fork();
    // printf("pid: %i\n", pid);
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        // printf("Child Process\n");
        execvp(args[0], args);
        // If execvp returns, there was an error
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        // Wait for child to terminate
        // printf("Parent Process\n");
        int status;
        // printf("status%i\n", &status);
        waitpid(pid, &status, 0);
        // Set last exit status
        // Use WIFEXITED and WEXITSTATUS macros to get child exit status
    }
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
        //printf("Command: %s\n", token);
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
    int i;
    char* infile = NULL;
    char* outfile = NULL;
    int in_index = -1;
    int out_index = -1;

    for(i = 0; args[i] != NULL; i++){
        if(strcmp(args[i], "<") == 0){
            infile = args[i + 1];
            in_index = i;
        } else if(strcmp(args[i], ">") == 0){
            outfile = args[i + 1];
            out_index = i;
        }        
    }

    if(infile != NULL) {
        int fd_in = open(infile, O_RDONLY);
        if(fd_in < 0) {
            perror("Error opening input file");
            exit(EXIT_FAILURE);
        }

        dup2(fd_in, STDIN_FILENO);
        close(fd_in);

        if(in_index >= 0){
            args[in_index] = NULL;
            args[in_index + 1] = NULL;
        }
    }

    if(outfile != NULL) {
        int fd_out = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(fd_out == -1) {
            perror("Error opening output file");
            exit(EXIT_FAILURE);
        }

        dup2(fd_out, STDOUT_FILENO);
        close(fd_out);

        if(out_index >= 0){
            args[out_index] = NULL;
            args[out_index + 1] = NULL;
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
