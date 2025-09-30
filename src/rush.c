/*
Name: Mohammad Aatif Mohammad Tariq Shaikh
UNumber: U61212522
NetID: mohammadaatif@usf.edu

Description:
This project is like a mini shell (like a small version of bash). 
The idea is: the shell runs in a loop, prints "rush>", waits for the user to type something, 
then figures out if it's a built-in command (like exit, cd, or path) or if I want to 
run some other program from /bin. 

To handle this, I break the input down into tokens (split by spaces/tabs), check for 
special stuff like ">" redirection, and then either execute the command directly (for 
built-ins) or fork a new process and run the external program.  

So the main approach is:
- Keep printing a prompt and reading input
- Parse the input for commands + arguments, and also check if output redirection is used
- Handle my own built-in commands separately
- If it's not built-in, try to find the program inside the list of paths I’m tracking (by default /bin)
- Run it by forking a child process and using exec
- If anything is wrong, always print the same error message
*/

// --- includes for system calls and functions we’ll need ---
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

// standard error message we use everywhere
char error_message[30] = "An error has occurred\n";

// array of paths where we look for executables (default: just /bin)
char *paths[10];
int num_paths = 0;


// helper to update paths (like when user types "path /bin /usr/bin")
void set_paths(char **new_paths, int count) {
    // first free up old paths so we don’t leak memory
    for (int i = 0; i < num_paths; i++) {
        free(paths[i]);
        paths[i] = NULL;
    }
    num_paths = 0;

    // then store all the new paths
    for (int i = 0; i < count; i++) {
        paths[num_paths] = strdup(new_paths[i]);
        num_paths++;
    }
}


int main(int argc, char *argv[]) {
    // The assignment says: if you run this program with args (like ./rush file.txt)ERROR and quit
    if (argc != 1) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    char *line = NULL;   // will hold input string
    size_t bufsize = 0;
    ssize_t length;

    // By default, path is just /bin so programs like "ls" work
    num_paths = 1;
    paths[0] = strdup("/bin");

    // main shell loop: print prompt, read command, execute
    while (1) {
        printf("rush> ");
        fflush(stdout);

        // getline: read a full line from stdin
        length = getline(&line, &bufsize, stdin);
        if (length == -1) { // ctrl+d (EOF)
            free(line);
            for (int i = 0; i < num_paths; i++) free(paths[i]);
            exit(0);
        }

        // strip trailing newline
        if (line[length - 1] == '\n') line[length - 1] = '\0';
        if (strlen(line) == 0) continue; // blank line = do nothing

        // -------- handle redirection (">") --------
        char *cmd_part = NULL;
        char *out_file = NULL;
        char *redir_ptr = strstr(line, ">");
        if (redir_ptr != NULL) {
            // if more than one ">", it's invalid
            char *second = strstr(redir_ptr + 1, ">");
            if (second != NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            *redir_ptr = '\0'; // cut the line before the ">"
            redir_ptr++;

            // skip spaces after ">"
            while (*redir_ptr == ' ' || *redir_ptr == '\t') redir_ptr++;

            // if nothing after ">", invalid
            if (*redir_ptr == '\0') {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }

            out_file = redir_ptr;

            // if filename has spaces after it, also invalid
            char *tmp = strpbrk(out_file, " \t");
            if (tmp != NULL) {
                char *check = tmp;
                while (*check == ' ' || *check == '\t') check++;
                if (*check != '\0') {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    continue;
                }
                *tmp = '\0';
            }

            cmd_part = line; // command part = left side of ">"
        } else {
            cmd_part = line;
            out_file = NULL;
        }

        // -------- split input into tokens (args[]) --------
        char *args[100];
        int argc_cmd = 0;
        char *saveptr;
        char *token = strtok_r(cmd_part, " \t", &saveptr);
        while (token != NULL && argc_cmd < 100) {
            args[argc_cmd++] = token;
            token = strtok_r(NULL, " \t", &saveptr);
        }
        args[argc_cmd] = NULL; // null terminate list

        // if user typed nothing but had ">", it’s an error
        if (argc_cmd == 0) {
            if (out_file != NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
            continue;
        }

        // -------- built-in commands --------
        if (strcmp(args[0], "exit") == 0) {
            // exit should not have args or redirection
            if (argc_cmd != 1 || out_file != NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            free(line);
            for (int i = 0; i < num_paths; i++) free(paths[i]);
            exit(0);
        }

        if (strcmp(args[0], "cd") == 0) {
            // cd must have exactly 1 argument
            if (argc_cmd != 2 || out_file != NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            // if chdir fails (invalid directory), print error
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

        // -------- external programs --------
        char prog_path[256];
        int found = 0;
        for (int i = 0; i < num_paths; i++) {
            snprintf(prog_path, sizeof(prog_path), "%s/%s", paths[i], args[0]);
            if (access(prog_path, X_OK) == 0) { // found executable
                found = 1;
                break;
            }
        }
        if (!found) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            continue;
        }

        // fork/exec model: parent waits, child runs program
        pid_t pid = fork();
        if (pid < 0) {
            // fork failed
            write(STDERR_FILENO, error_message, strlen(error_message));
            continue;
        } else if (pid == 0) {
            // child
            if (out_file != NULL) {
                // stackoverflow + AI helped me figure out the safest way to do redirection
                int fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd == -1) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO); // send stdout to file
                close(fd);
            }
            args[0] = prog_path; // execv wants full path in argv[0]
            execv(prog_path, args);

            // if exec fails, print error then exit
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        } else {
            // parent just waits
            wait(NULL);
        }
    }

    // cleanup (though loop usually exits with exit())
    free(line);
    for (int i = 0; i < num_paths; i++) free(paths[i]);
    return 0;
}
