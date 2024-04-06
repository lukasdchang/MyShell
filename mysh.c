#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "parser.h"
#include "executor.h"

#define MAX_INPUT_SIZE 1024
#define MAX_TOKENS 64

int main(int argc, char *argv[]) {
    // Determine mode: interactive or batch
    int interactive_mode = isatty(STDIN_FILENO);

    // Print welcome message in interactive mode
    if (interactive_mode) {
        printf("Welcome to my shell!\n");
    }

    // Read commands from specified file or standard input
    FILE *input_stream;
    if (argc == 2) {
        input_stream = fopen(argv[1], "r");
        if (input_stream == NULL) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
    } else {
        input_stream = stdin;
    }

    // Main loop for reading and executing commands
    char input[MAX_INPUT_SIZE];
    while (fgets(input, MAX_INPUT_SIZE, input_stream) != NULL) {
        // Parse input
        char *tokens[MAX_TOKENS];
        tokenize_input(input, tokens);

        // Execute command
        execute_command(tokens, interactive_mode);

        // Print prompt in interactive mode
        if (interactive_mode) {
            printf("mysh> ");
            fflush(stdout);
        }
    }

    // Print exit message in interactive mode
    if (interactive_mode) {
        printf("Exiting my shell.\n");
    }

    // Close input stream if not standard input
    if (argc == 2) {
        fclose(input_stream);
    }

    return 0;
}
