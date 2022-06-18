#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include "common.h"

void main() {

  // Change directory to default PATH from common.h
  if (chdir(PATH) != 0) {
    fprintf(stderr, "Error: failed to set working directory.\n");
    exit(EXIT_FAILURE);
  }

  // Print help information.
  printf("Usage:\n");
  printf(" pmanager [FILE]\n");
  printf(" Execute commands from standard input or [FILE].\n");
  printf(" To show help about a command, you can use the -h option.\n");
  printf("\n");
  printf("Commands:\n");

  // List all regular files that are also executable in current directory.
  DIR *current_dir = opendir(".");
  struct dirent *file;
  if (current_dir != NULL) {
    while ((file = readdir(current_dir)) != NULL) {
      if (access(file->d_name, X_OK) == 0 && (file->d_type == DT_REG)) {
        printf(" %s\n", file->d_name);
      }
    }
    closedir(current_dir);
  } else {
    fprintf(stderr, "Error: failed to read directory contents.\n");
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
