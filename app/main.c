#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "lab.h"

int main(int argc, char **argv)
{
  parse_args(argc, argv);

  struct shell shell;
  sh_init(&shell);

  char *line;
  using_history();
  while ((line=readline(shell.prompt))){
    if(line == NULL) {free(line); break;} //break out of loop to exit naturally on Ctrl-d or EOF

    if(line[0] == '\0') {free(line); continue;} //loop again on empty input

    add_history(line);
    char *trimmed = trim_white(line);
    char **args = cmd_parse(trimmed);

    if(!do_builtin(&shell, args)) {
      pid_t pid = fork();
      if(pid == -1) {fprintf(stderr, "Fork failed for %s: %s\n", args[0], strerror(errno)); free(line); free(trimmed); cmd_free(args); continue;}

      if(pid == 0) {
        if(execvp(args[0], args) == -1) {fprintf(stderr, "execvp failed for %s: %s\n", args[0], strerror(errno)); exit(EXIT_FAILURE);}
      } else {
        int status;
        if(waitpid(pid, &status, 0) == -1) {fprintf(stderr, "waitpid failed for %s: %s\n", args[0], strerror(errno));}
      }
    }

    cmd_free(args);
    free(trimmed);
    free(line);
  }

  sh_destroy(&shell);

  return 0;
}
