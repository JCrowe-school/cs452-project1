#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <sys/wait.h>
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
    } else if(strcmp(argv[0], "jobs") == 0) {
        job_flag = true;
        return true;
    }

    return false;
}

void sh_init(struct shell *sh) {
    //set non-pointer fields, assuming shell is interactive and STDIN is the input
    sh->shell_is_interactive = 1;
    sh->shell_pgid = getpid();
    sh->shell_terminal = STDIN_FILENO;
    job_count = 0;
    job_flag = false;

    //creating new session, grabbing control of the terminal, and setting process group to current
    if(tcgetattr(sh->shell_terminal, &sh->shell_tmodes) < 0) {perror("Failed to get terminal attributes!"); exit(1);}
    if(setsid() < 0) {perror("setsid failed!"); exit(1);}
    int term = open("/dev/tty", O_RDWR);
    if(term == -1) {perror("Terminal failed to open!"); exit(1);}
    if(tcsetpgrp(term, sh->shell_pgid) < 0) {perror("tcsetpgrp failed!"); exit(1);}

    //check for MY_PROMPT, use default otherwise
    sh->prompt = get_prompt("MY_PROMPT");

    //start bg_jobs with null values
    for(int i = 0; i < MAX_JOBS; i++) bg_jobs[i] = NULL;

    //ignore signals for ctrl-c, quit, stop, background read/write
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

void sh_destroy(struct shell *sh) {
    if(sh->prompt != NULL) {free(sh->prompt); sh->prompt = NULL;}

    //reset terminal to original settings and process group
    if(tcsetattr(sh->shell_terminal, TCSADRAIN, &sh->shell_tmodes) < 0) {perror("Failed to reset terminal settings!");}
    if(tcsetpgrp(sh->shell_terminal, getpid()) < 0) {perror("Failed to reset terminal process group!");}

    for(int i = job_count - 1; i >= 0; i--) {
        free(bg_jobs[i]->command);
        free(bg_jobs[i]);
    }

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

unsigned int assign_job_id() {
    return (job_count > 0) ? bg_jobs[job_count - 1]->job_id + 1 : 1;
}

void add_background_job(unsigned int job_id, pid_t pid, char *command) {
    if(job_count < MAX_JOBS) {
        bg_jobs[job_count] = malloc(sizeof(struct background_job));
        if(bg_jobs[job_count] == NULL) {perror("Failed to allocate memory for background job!"); return;}

        bg_jobs[job_count]->job_id = job_id;
        bg_jobs[job_count]->pid = pid;
        bg_jobs[job_count]->command = strdup(command);
        if(bg_jobs[job_count]->command == NULL) {perror("Failed to allocate memory for background command"); free(bg_jobs[job_count]); return;}
        job_count++;

        printf("[%d] %d %s\n", job_id, pid, command);
    } else {
        fprintf(stderr, "Too many background jobs!\n");
    }
}

void check_background_jobs() {
    for(int i = 0; i < job_count; i++) {
        int status;
        pid_t pid = waitpid(bg_jobs[i]->pid, &status, WNOHANG);
        if(pid > 0 && !WIFSTOPPED(status)) {
            if(WIFEXITED(status)) {
                printf("[%d] Done %s\n", bg_jobs[i]->job_id, bg_jobs[i]->command);
            } else if (WIFSIGNALED(status)) {
                printf("[%d] Killed %s\n", bg_jobs[i]->job_id, bg_jobs[i]->command);
            }

            free(bg_jobs[i]->command);
            free(bg_jobs[i]);
            bg_jobs[i] = NULL;

            //shuffle down remaining jobs, then set last job to null before decrementing i and job_count to maintain position in loop
            for(int j = i; j < job_count -1; j++) {
                bg_jobs[j] = bg_jobs[j+1];
            }
            bg_jobs[job_count - 1] = NULL;
            job_count--;
            i--;
        } else if(job_flag) {
            if(WIFSTOPPED(status)) {
                printf("[%d] %d Stopped %s\n", bg_jobs[i]->job_id, bg_jobs[i]->pid, bg_jobs[i]->command);
            } else if (pid == 0) {
                printf("[%d] %d Running %s\n", bg_jobs[i]->job_id, bg_jobs[i]->pid, bg_jobs[i]->command);
            }
        }

        if(pid == -1) {
            if(errno == EINVAL) {
                perror("Invalid arguments for process check!");
            } else if(errno == EINTR) {
                i--;
            }
        }
    }

    if(job_flag) job_flag = false; //reset job flag if it was set by jobs
}
