/*
Name: Mohammad Aatif Mohammad Tariq Shaikh
UNumber: U61212522
NetID: mohammadaatif@usf.edu

"This is my code and my code only"
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

// one standard error message that I can reuse everywhere.
char error_message[30] = "An error has occurred\n";

// my internal storage of valid search paths.
// this is how I mimic the $PATH variable in a real shell.
char *paths[10];
int num_paths = 0; 

// helper to reset and update my path list.
// I wipe whatever was stored before and replace it with the new ones.
// this way when "path" command is used
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
    // my shell is meant to be run without extra args.
    // if you pass args, I just error out and stop
    if (argc != 1) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    char *line = NULL;
    size_t bufsize = 0;
    ssize_t length;

    // start with a default path so common commands (like ls) work right away.
    num_paths = 1;
    paths[0] = strdup("/bin");

    // core loop: print prompt → read input → parse → run
    while (1) {
        printf("rush> ");
        fflush(stdout);

        // read user input, if EOF (ctrl+d), exit cleanly
        length = getline(&line, &bufsize, stdin);
        if (length == -1) {
            free(line);
            for (int i = 0; i < num_paths; i++) free(paths[i]);
            exit(0);
        }

        // clean trailing newline and ignore empty lines
        if (line[length - 1] == '\n') line[length - 1] = '\0';
        if (strlen(line) == 0) continue;

        // -------- handle output redirection --------
        // why: shell needs to know if user wants to redirect output.
        // I split the command from the filename, and also validate misuse like ">>" or missing filename.
        char *cmd_part = NULL;
        char *out_file = NULL;

        char *redir_ptr = strstr(line, ">");
        if (redir_ptr != NULL) {
            // more than one ">" → invalid
            char *second = strstr(redir_ptr + 1, ">");
            if (second != NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }

            // cut the command part before ">", then trim spaces to find filename
            *redir_ptr = '\0';
            redir_ptr++;
            while (*redir_ptr == ' ' || *redir_ptr == '\t') redir_ptr++;

            if (*redir_ptr == '\0') { // no file given
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }

            out_file = redir_ptr;

            // check if filename has only one token (no extra args after it)
            char *tmp = strpbrk(out_file, " \t");
            if (tmp != NULL && *(tmp + 1) != '\0') {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }

            // terminate filename cleanly
            if (tmp != NULL) *tmp = '\0';
            cmd_part = line;
        } else {
            cmd_part = line;
            out_file = NULL;
        }

        // -------- tokenize command into args --------
        // why: execv expects argv-style arrays, so I split the input here.
        char *args[100];
        int argc_cmd = 0;
        char *saveptr;
        char *token = strtok_r(cmd_part, " \t", &saveptr);
        while (token != NULL && argc_cmd < 100) {
            args[argc_cmd++] = token;
            token = strtok_r(NULL, " \t", &saveptr);
        }
        args[argc_cmd] = NULL;
        if (argc_cmd == 0) continue;

        // -------- built-in commands --------
        // why: these commands can’t/shouldn’t run as executables.
        // they need special handling by the shell itself.

        // exit: only valid with no args, no redirection
        if (strcmp(args[0], "exit") == 0) {
            if (argc_cmd != 1 || out_file != NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            free(line);
            for (int i = 0; i < num_paths; i++) free(paths[i]);
            exit(0);
        }

        // cd: must have exactly one arg, changes current directory
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

        // path: modifies internal search paths for executables
        if (strcmp(args[0], "path") == 0) {
            if (out_file != NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            set_paths(&args[1], argc_cmd - 1);
            continue;
        }

        // -------- external commands --------
        // why: if it’s not built-in, I try to run it like a real shell does.
        // search through my paths to see if this program exists and is executable.
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

        // -------- fork + exec --------
        // why: we need a child process so the parent shell stays alive.
        // parent: waits. child: does the work (redirection + exec).
        pid_t pid = fork();
        if (pid < 0) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            continue;
        } else if (pid == 0) {
            // child handles redirection if needed
            if (out_file != NULL) {
                int fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd == -1) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO); 
                close(fd);
            }
            execv(prog_path, args);
            // if exec fails, we land here
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        } else {
            // parent just waits for child to finish
            wait(NULL);
        }
    }

    // final cleanup (not usually hit unless loop breaks)
    free(line);
    for (int i = 0; i < num_paths; i++) free(paths[i]);
    return 0;
}
