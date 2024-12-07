#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "lab.h"

char *get_prompt(const char *env) {
    char *prompt = NULL;
    char *env_prompt = getenv(env);
    if(env_prompt != NULL && *env_prompt != '\0') {
        prompt = strdup(env_prompt);
    } else {
        prompt = strdup("shell>");
    }

    if(prompt == NULL) {perror("Failed to allocate memory for prompt!"); exit(1);}
    return prompt;
}

int change_dir(char **dir) {
    char *tdir = NULL;

    if(dir == NULL || *dir == NULL) {
        const char *hdir = getenv("HOME");
        if(hdir == NULL) {
            struct passwd *pw = getpwuid(getuid());
            if(pw == NULL) {perror("Failed to retrieve home directory!"); return -1;}
            hdir = pw->pw_dir;
        }
        tdir = hdir;
    } else {
        tdir = *dir;
    }

    if(chdir(tdir) == -1) {
        switch(errno) {
            case ENOENT:
                fprintf(stderr, "No such file or directory: %s\n", tdir);
                break;
            case ENOTDIR:
                fprintf(stderr, "Not a directory: %s\n", tdir);
                break;
            case EACCES:
                fprintf(stderr, "Permission denied: %s\n", tdir);
                break;
            case EIO:
                fprintf(stderr, "I/O error while checking directory: %s\n", tdir);
                break;
            default:
                perror("Failed to change directory!");
                break;
        }

        return -1;
    }

    return 0;
}

char **cmd_parse(char const *line) {
    int count = 0;
    long max = sysconf(_SC_ARG_MAX);
    if(max == -1) {perror("Failed to retrieve ARG_MAX!"); exit(1);}

    char **args = malloc((max + 1) * sizeof(char *));
    if(args == NULL) {perror("Failed to allocate memory for cmd_parse!"); exit(1);}

    char *dupe = strdup(line);
    if(dupe == NULL) {perror("Failed to duplicate line!"); exit(1);}

    char *token = strtok(dupe, " \t\n\r\f\v");
    while(token != NULL) {
        args[count] = strdup(token);
        if(args[count] == NULL) {perror("Failled to allocate memory for token!"); exit(1);}

        count++;
        if(count >= max) break;

        token = strtok(NULL, " \t\n\r\f\v");
    }

    args[count] = NULL; //null terminate args
    free(dupe);

    return args;
}

void cmd_free(char ** line) {
    if(line == NULL) return; //prevent null dereference

    for(int i = 0; line[i] != NULL; i++) free(line[i]);
    free(line);
}

char *trim_white(char *line) {
    line += strspn(line, " \t\n\r\f\v"); //trims leading whitespace
    if(*line) {line[strcspn(line, " \t\n\r\f\v")] = '\0';} //if line isn't null, terminate trailing whitespace
    return line;
}

bool do_builtin(struct shell *sh, char **argv) {
    if(argv == NULL || argv[0] == NULL) return false; //return false if invalid or empty command

    if(strcmp(argv[0], "exit") == 0) {
        sh_destroy(sh);
    } else if(strcmp(argv[0], "cd") == 0) {
        (argv[1] == NULL) ? change_dir(NULL) : change_dir(argv[1]);
        return true;
    } else if(strcmp(argv[0], "history") == 0) {
        HIST_ENTRY **hist = history_list();
        if(hist != NULL) {
            for(int i = 0; hist[i] != NULL; i++) {
                printf("%d %s\n", i + 1, hist[i]->line);
            }
        }
        return true;
    } else if(strcmp(argv[0], "pwd") == 0) {
        char cwd[1024];
        if(getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("Unable to get currend working directory!");
        }
        return true;
    }

    return false;
}

void sh_init(struct shell *sh) {
    //set non-pointer fields, assuming shell is interactive and STDIN is the input
    sh->shell_is_interactive = 1;
    sh->shell_pgid = getpid();
    sh->shell_terminal = STDIN_FILENO;

    //creating new session, grabbing control of the terminal, and setting process group to current
    if(tcgetattr(sh->shell_terminal, &sh->shell_tmodes) < 0) {perror("Failed to get terminal attributes!"); exit(1);}
    if(setsid() < 0) {perror("setsid failed!"); exit(1);}
    int term = open("/dev/tty", O_RDWR);
    if(term == -1) {perror("Terminal failed to open!"); exit(1);}
    if(tcsetpgrp(term, sh->shell_pgid) < 0) {perror("tcsetpgrp failed!"); exit(1);}

    //check for MY_PROMPT, use default otherwise
    sh->prompt = get_prompt("MY_PROMPT");
}

void sh_destroy(struct shell *sh) {
    if(sh->prompt != NULL) {free(sh->prompt); sh->prompt = NULL;}

    //reset terminal to original settings and process group
    if(tcsetattr(sh->shell_terminal, TCSADRAIN, &sh->shell_tmodes) < 0) {perror("Failed to reset terminal settings!");}
    if(tcsetpgrp(sh->shell_terminal, getpid()) < 0) {perror("Failed to reset terminal process group!");}

    exit(EXIT_SUCCESS);
}

void parse_args(int argc, char **argv) {
    int c;
    opterr = 0;

    while ((c = getopt (argc, argv, "v")) != -1) {
        switch (c) {
            case 'v':
                printf("Version %d.%d", lab_VERSION_MAJOR, lab_VERSION_MINOR);
                exit(EXIT_SUCCESS);
            default:
                abort();
        }
    }

    for (int i = optind; i < argc; i++) printf ("Non-option argument %s\n", argv[i]);
}
