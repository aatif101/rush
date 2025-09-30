#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

char error_message[30] = "An error has occurred\n";

// store our search paths (max 10 paths for now)
char *paths[10];
int num_paths = 0;

// function to add or reset search paths
void set_paths(char **new_paths, int count) {
    // clear old paths
    for (int i = 0; i < num_paths; i++) {
        free(paths[i]);
        paths[i] = NULL;
    }
    num_paths = 0;

    // add each new path
    for (int i = 0; i < count; i++) {
        paths[num_paths] = strdup(new_paths[i]);
        num_paths++;
    }
}

int main() {
    char *line = NULL;
    size_t bufsize = 0;
    ssize_t length;

    // Set default path to just "/bin"
    num_paths = 1;
    paths[0] = strdup("/bin");

    while (1) {
        printf("rush> ");
        fflush(stdout);

        length = getline(&line, &bufsize, stdin);
        if (length == -1) {
            free(line);
            // free paths on quit
            for (int i = 0; i < num_paths; i++) free(paths[i]);
            exit(0);
        }

        if (line[length - 1] == '\n') line[length - 1] = '\0';
        if (strlen(line) == 0) continue;

        // Tokenize line for max 100 words
        char *args[100];
        int argc = 0;
        char *saveptr; // For strtok_r (threadsafe)
        char *token = strtok_r(line, " \t", &saveptr);
        while (token != NULL && argc < 100) {
            args[argc++] = token;
            token = strtok_r(NULL, " \t", &saveptr);
        }
        args[argc] = NULL;

        if (argc == 0) continue; // empty

        // =====================
        // Built-in: exit
        // =====================
        if (strcmp(args[0], "exit") == 0) {
            if (argc != 1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            free(line);
            for (int i = 0; i < num_paths; i++) free(paths[i]);
            exit(0);
        }

        // =====================
        // Built-in: cd
        // =====================
        if (strcmp(args[0], "cd") == 0) {
            if (argc != 2) { // must have exactly one argument
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            // try to change directory
            if (chdir(args[1]) != 0) {
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
            continue;
        }

        // =====================
        // Built-in: path
        // =====================
        if (strcmp(args[0], "path") == 0) {
            // "path" by itself clears all paths (no programs can run)
            set_paths(&args[1], argc - 1);
            continue;
        }

        // =====================
        // Run external programs
        // =====================
        // For each path in our shell search path, try to find the executable
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

        pid_t pid = fork();
        if (pid < 0) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            continue;
        } else if (pid == 0) {
            // in child: run the program, args[0] must be prog name
            execv(prog_path, args);

            // execv only returns if it failed!
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        } else {
            // parent: wait for child to finish
            wait(NULL);
        }
    }

    // clean up (should never hit)
    free(line);
    for (int i = 0; i < num_paths; i++) free(paths[i]);
    return 0;
}

/*
    Notes (Gen-Z style):

    - If you type "cd directory", it'll use chdir() to change to that place.
    - If you type "path /bin /usr/bin", it sets the search path so you can run stuff from both.
    - Try "path" alone: No dirs = no commands work except built-ins!
    - "ls" and other commands look in your search paths (default: just /bin).
    - Only ever prints one error message, like they asked.
*/
