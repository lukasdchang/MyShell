#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include "parser.h"

#define MAX_TOKENS 64
#define MAX_TOKEN_LENGTH 64

void tokenize_input(char *input, char *tokens[]);
void expand_wildcards(char *tokens[]);

void tokenize_input(char *input, char *tokens[]) {
    // Tokenize input based on whitespace and special characters
    char *token = strtok(input, " \n\t\r");
    int num_tokens = 0;
    while (token != NULL && num_tokens < MAX_TOKENS - 1) {
        tokens[num_tokens++] = token;
        token = strtok(NULL, " \n\t\r");
    }
    tokens[num_tokens] = NULL;  // Null-terminate the token array
}

void expand_wildcards(char *tokens[]) {
    // Iterate through tokens and expand any wildcard patterns
    glob_t glob_result;
    for (int i = 0; tokens[i] != NULL; i++) {
        if (strchr(tokens[i], '*') != NULL) {
            // Wildcard pattern found, perform expansion
            if (glob(tokens[i], GLOB_TILDE, NULL, &glob_result) == 0) {
                // Replace token with expanded list of filenames
                for (size_t j = 0; j < glob_result.gl_pathc; j++) {
                    strcpy(tokens[i + j], glob_result.gl_pathv[j]);
                }
                // Shift remaining tokens to accommodate expanded filenames
                for (int k = i + glob_result.gl_pathc; tokens[k] != NULL; k++) {
                    tokens[k] = tokens[k + glob_result.gl_pathc - 1];
                }
                i += glob_result.gl_pathc - 1;  // Update index
            }
        }
    }
    globfree(&glob_result);
}
