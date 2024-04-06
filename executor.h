#ifndef EXECUTOR_H
#define EXECUTOR_H

void execute_command(char *tokens[], int interactive_mode);
void handle_builtin_commands(char *tokens[]);
void handle_redirection(char *tokens[]);
void handle_pipes(char *tokens[]);

#endif
