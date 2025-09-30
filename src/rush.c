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

int main(int argc, char *argv[]) {
    // REQUIRED: exit immediately with error if called with args!
    if (argc != 1) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    char *line = NULL;
    size_t bufsize = 0;
    ssize_t length;

    num_paths = 1;
    paths[0] = strdup("/bin"); // use /bin only (never /usr/bin unless by path cmd)

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

        // Output redirection logic
        char *cmd_part = NULL;
        char *out_file = NULL;
        char *redir_ptr = strstr(line, ">");
        if (redir_ptr != NULL) {
            char *second = strstr(redir_ptr + 1, ">");
            if (second != NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            *redir_ptr = '\0';
            redir_ptr++;
            while (*redir_ptr == ' ' || *redir_ptr == '\t') redir_ptr++;
            if (*redir_ptr == '\0') {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            out_file = redir_ptr;
        char *tmp = strpbrk(out_file, " \t");
        if (tmp != NULL) {
            // Skip all whitespace to see if there are more arguments
            char *check = tmp;
            while (*check == ' ' || *check == '\t') check++;
            if (*check != '\0') {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            *tmp = '\0';  // Trim trailing whitespace from filename
        }

            if (tmp != NULL) *tmp = '\0';
            cmd_part = line;
        } else {
            cmd_part = line;
            out_file = NULL;
        }

        char *args[100];
        int argc_cmd = 0;
        char *saveptr;
        char *token = strtok_r(cmd_part, " \t", &saveptr);
        while (token != NULL && argc_cmd < 100) {
            args[argc_cmd++] = token;
            token = strtok_r(NULL, " \t", &saveptr);
        }
        if (argc_cmd == 0) {
            if (out_file != NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
            continue;
        }



        // Built-ins
        if (strcmp(args[0], "exit") == 0) {
            if (argc_cmd != 1 || out_file != NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            free(line);
            for (int i = 0; i < num_paths; i++) free(paths[i]);
            exit(0);
        }
        if (strcmp(args[0], "cd") == 0) {
            if (argc_cmd != 2 || out_file != NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            if (chdir(args[1]) != 0) {
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
            continue;
        }
        if (strcmp(args[0], "path") == 0) {
            if (out_file != NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            set_paths(&args[1], argc_cmd - 1);
            continue;
        }

        // External commands search - strictly left-to-right
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
            if (out_file != NULL) {
                int fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd == -1) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO); // redirect ONLY stdout!
                close(fd);
            }
            args[0] = prog_path;
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
