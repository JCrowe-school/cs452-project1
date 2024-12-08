#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include "../src/lab.h"

int main(int argc, char **argv)
{
  parse_args(argc, argv);

  struct shell shell;
  sh_init(&shell);

  char *line;
  using_history();
  while ((line=readline(shell.prompt))){
    if(line == NULL) {free(line); break;} //break out of loop to exit naturally on Ctrl-d or EOF

    char *trimmed = trim_white(line);
    if(trimmed[0] == '\0') {free(trimmed); free(line); continue;} //loop again on empty input

    //add to history, then check for if it's a background process before parsing
    add_history(trimmed);

    int bg = 0; //flag for if the command is a background command or not
    if(trimmed[strlen(trimmed) - 1] == '&') {
      bg = 1;
      trimmed[strlen(trimmed) - 1] = '\0';
    }
    char **args = cmd_parse(trimmed);

    pid_t ppgid = tcgetpgrp(shell.shell_terminal);
    if(!do_builtin(&shell, args)) {
      pid_t pid = fork();
      if(pid == -1) {
        fprintf(stderr, "Fork failed for %s: %s\n", args[0], strerror(errno));
      } else {
        if(pid == 0) { //child process
          pid_t child = getpid();
          setpgid(child, child);

          //set to foreground if not background
          if(bg == 0) tcsetpgrp(shell.shell_terminal, child);

          //reset signals
          signal (SIGINT, SIG_DFL);
          signal (SIGQUIT, SIG_DFL);
          signal (SIGTSTP, SIG_DFL);
          signal (SIGTTIN, SIG_DFL);
          signal (SIGTTOU, SIG_DFL);

          //execute child process
          execvp(args[0], args);
          //if execvp fails, then print error message
          fprintf(stderr, "execvp failed for %s: %s\n", args[0], strerror(errno));
          exit(EXIT_FAILURE);
        } else { //parent process
          if(bg == 0) {
            int status;
            if(waitpid(pid, &status, 0) == -1) {fprintf(stderr, "waitpid failed for %s: %s\n", args[0], strerror(errno));}
            
            //return control of terminal after child has finished
            tcsetpgrp(shell.shell_terminal, ppgid);
          } else {
            int job_id = assign_job_id();
            add_background_job(job_id, pid, trimmed);
          }
        }
      }
    }

    check_background_jobs();

    cmd_free(args);
    free(trimmed);
    free(line);
  }

  sh_destroy(&shell);

  return 0;
}
