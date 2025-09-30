/*Name: Mohammad Aatif Mohammad Tariq Shaikh
UNumber: U61212522
"This is my code and my code only"
*/


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

char error_message[30] = "An error has occurred\n";
char *paths[10];
int num_paths = 0; 

void set_paths(char **new_paths, int count) {
    for (int i = 0; i < num_paths; i++) {
        free(paths[i]);
        paths[i] = NULL;
    }
    num_paths = 0;
    for (int i = 0; i < count; i++) {
        paths[num_paths] = strdup(new_paths[i]);
        num_paths++;
    }
}

int main() {
    char *line = NULL;
    size_t bufsize = 0;
    ssize_t length;

    num_paths = 1;
    paths[0] = strdup("/bin");

    while (1) {
        printf("rush> ");
        fflush(stdout);

        length = getline(&line, &bufsize, stdin);
        if (length == -1) {
            free(line);
            for (int i = 0; i < num_paths; i++) free(paths[i]);
            exit(0);
        }
        if (line[length - 1] == '\n') line[length - 1] = '\0';
        if (strlen(line) == 0) continue;

        //Output redirection section 
        char *cmd_part = NULL;
        char *out_file = NULL;

        // Look for '>' and split only once
        char *redir_ptr = strstr(line, ">");
        if (redir_ptr != NULL) {
            // error if more than one '>'
            char *second = strstr(redir_ptr + 1, ">");
            if (second != NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            // Separate command from output filename
            *redir_ptr = '\0'; // Terminate command before '>'
            redir_ptr++;
            // Skip spaces/tabs after '>'
            while (*redir_ptr == ' ' || *redir_ptr == '\t') redir_ptr++;
            if (*redir_ptr == '\0') {
                // No filename provided
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            // Get output filename
            out_file = redir_ptr;
            // Multiple tokens after output filename = error
            char *tmp = strpbrk(out_file, " \t");
            if (tmp != NULL && *(tmp + 1) != '\0') {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            cmd_part = line;
        } else {
            cmd_part = line;
            out_file = NULL;
        }

        // --- Tokenize the command line window ---
        char *args[100];
        int argc = 0;
        char *saveptr;
        char *token = strtok_r(cmd_part, " \t", &saveptr);
        while (token != NULL && argc < 100) {
            args[argc++] = token;
            token = strtok_r(NULL, " \t", &saveptr);
        }
        args[argc] = NULL;
        if (argc == 0) continue;

        // Built-in exit, cd, path
        if (strcmp(args[0], "exit") == 0) {
            if (argc != 1 || out_file != NULL) { // exit can't take args or redirection
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            free(line);
            for (int i = 0; i < num_paths; i++) free(paths[i]);
            exit(0);
        }
        if (strcmp(args[0], "cd") == 0) {
            if (argc != 2 || out_file != NULL) { // cd needs one arg, can't have redirection
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            if (chdir(args[1]) != 0) {
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
            continue;
        }
        if (strcmp(args[0], "path") == 0) {
            if (out_file != NULL) { // no redirection allowed for built-ins
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            set_paths(&args[1], argc - 1);
            continue;
        }
        // External commands
        char prog_path[256];
        int found = 0;
        for (int i = 0; i < num_paths; i++) {
            snprintf(prog_path, sizeof(prog_path), "%s/%s", paths[i], args[0]);
            if (access(prog_path, X_OK) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            continue;
        }
        // Fork and run the thing
        pid_t pid = fork();
        if (pid < 0) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            continue;
        } else if (pid == 0) {
            // CHILD: handle redirection if needed
            if (out_file != NULL) {
                int fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd == -1) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO); // now stdout goes to file!
                close(fd);
            }
            execv(prog_path, args);
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        } else {
            wait(NULL);
        }
    }
    free(line);
    for (int i = 0; i < num_paths; i++) free(paths[i]);
    return 0;
}
