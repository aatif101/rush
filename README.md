#rush
A minimal Linux shell written in C, built from scratch as part of my Operating Systems coursework.
The goal was to understand how a real shell interprets commands, manages processes, and interacts with the kernel at a system-call level.

---

## **Overview**

`rush` (short for “run shell”) is a lightweight custom shell that mimics basic Unix shell behavior.
It can execute commands, manage search paths, handle input and output redirection, and gracefully report errors — all while maintaining a single process model that forks and waits for child executions.

This project helped me understand how shells like `bash` and `zsh` actually work behind the scenes — from parsing user input and spawning processes to handling system calls like `execv`, `fork`, and `waitpid`.

---

## **Features**

* Executes standard Linux commands (e.g., `ls`, `echo`, `cat`, etc.)
* Customizable executable search paths (`path` command)
* Input and output redirection using `>`
* Handles multiple commands per prompt
* Proper error handling for invalid commands and missing paths
* Minimal external dependencies (only uses standard C libraries)
* Exits cleanly with the `exit` command

---

## **Example Usage**

```
rush> ls
rush> echo Hello World
rush> path /bin /usr/bin
rush> ls /abc
/bin/ls: cannot access '/abc': No such file or directory
rush> exit
```

---

## **Core Concepts**

This project was my hands-on introduction to:

* **Process creation** using `fork()`
* **Command execution** using `execv()`
* **Parent-child synchronization** using `waitpid()`
* **Error handling** via standard error streams
* **Dynamic path management** for locating executables
* **String parsing** and tokenization with `strtok()`

Instead of relying on any framework or helper library, everything is done at the syscall level.
This forced me to understand what actually happens when you run a command in a shell.

---

## **Project Structure**

```
rush/
│
├── rush.c             # main program file
├── Makefile           # build configuration
├── README.md          # project documentation
└── testcases.txt      # sample test cases for validation
```

---

## **How to Run**

1. Compile the program:

   ```
   make
   ```
2. Run the shell:

   ```
   ./rush
   ```
3. Use commands as you would in a normal shell:

   ```
   rush> ls
   rush> echo Hello World
   rush> exit
   ```

---

## **Reflection**

This was one of the first projects that made me feel like I was working close to the metal.
Every bug taught me how fragile process management can be, a missing file descriptor or fork gone wrong could break everything.
By the end, I had a much deeper understanding of how operating systems manage processes and execute programs, beyond just theory.


