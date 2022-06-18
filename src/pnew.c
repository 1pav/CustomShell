#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include "common.h"
#include "message.h"
#include "proc_tree.h"
#include "child.h"

// Global variables accessed by cleanup().
// File descriptor of FIFO.
int fifo_fd = -1;
// Flag for --help
int help_flag = 0;

// Option arguments.
const char * short_options = "h";
const struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {0, 0, 0, 0}
};

// Utility functions.
// Parses arguments and sets global flags.
char * parse_args(int argc, char ** argv);
// Prints help about this command.
void print_help();
// Performs cleanup operations on exit.
void cleanup();
// Kills forked process when it is not possible adding it in pmanager's process tree.
void abort_fork(pid_t pid);

void main(int argc, char ** argv) {

  // Check argv options.
  char * proc_name = parse_args(argc, argv);

  // If help_flag was set by parse_args(), print help and exit.
  if (help_flag || proc_name == NULL) {
    print_help();
    exit(EXIT_FAILURE);
  }

  // Register cleanup function to be called at process termination.
  atexit(cleanup);

  // Open FIFO.
  if ((fifo_fd = open(FIFO_NAME, O_RDWR | O_NONBLOCK)) == -1) {
    fprintf(stderr, "Error: failed to open FIFO.\n");
    exit(EXIT_FAILURE);
  }

  // Setup process communication.
  if (message_setup(fifo_fd) != 0) {
    fprintf(stderr, "Error: failed to setup process communication.\n");
    exit(EXIT_FAILURE);
  }

  // Save pid of pmanager for use in the forked process.
  pid_t pid_pmanager = getppid();

  // Before forking, check if a process with name proc_name already exists.
  if (message_send(pid_pmanager, MSG_INFO, proc_name) != 0) {
    fprintf(stderr, "Error: failed to send message.\n");
    exit(EXIT_FAILURE);
  }
  // Wait response from pmanager.
  message_t * response = message_wait(pid_pmanager);
  int duplicate = strcmp(response->type, MSG_INFO) == 0;
  message_deinit(response);
  // If proc_name already exists, exit.
  if (duplicate) {
    fprintf(stderr, "Error: a process with name \"%s\" already exists.\n", proc_name);
    exit(EXIT_FAILURE);
  }

  // Fork process.
  pid_t pid = fork();
  if (pid == -1) {
    fprintf(stderr, "Error: failed to fork process.\n");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    // Child
    // Wait for messages/signals.
    child_init(proc_name, pid_pmanager);
  } else {
    // Parent
    // Send information about new process to pmanager.
    // Create proc_node.
    proc_node * proc = proc_node_init(pid, getppid(), proc_name);
    if (proc == NULL) {
      abort_fork(pid);
      exit(EXIT_FAILURE);
    }
    // Convert proc_node to string.
    char * proc_str;
    int status = proc_node_tostr(proc, &proc_str);
    proc_node_deinit(proc);
    if (status == -1) {
      abort_fork(pid);
      exit(EXIT_FAILURE);
    }
    // Send message containing proc_str to pmanager.
    status = message_send(getppid(), MSG_ADD, proc_str);
    free(proc_str);
    if (status != 0) {
      abort_fork(pid);
      exit(EXIT_FAILURE);
    }
    // Wait response from pmanager.
    message_t * result = message_wait(getppid());
    if (result == NULL) {
      abort_fork(pid);
      exit(EXIT_FAILURE);
    }
    // Check if add was successful.
    status = strcmp(result->type, MSG_ERROR);
    message_deinit(result);
    if (status == 0) {
      abort_fork(pid);
      exit(EXIT_FAILURE);
    }
    printf("Process \"%s\" successfully started.\n", proc_name);
    exit(EXIT_SUCCESS);
  }
}

// Shows error and sends SIGTERM to pid. Called when fork() was successful but
// it was not possibile to add the forked process in pmanager's process tree.
// Killing the forked process is necessary to avoid inconsistency between
// tree in pmanager and processes that are actually alive.
//
// pid: the pid of the process to kill
void abort_fork(pid_t pid) {
  fprintf(stderr, "Error: failed to add process in pmanager. Sending SIGTERM "
          "to %ld...\n", (long) pid);
  // Send SIGTERM to pid.
  if (kill(pid, SIGTERM) != 0) {
      fprintf(stderr, "Error: failed to send SIGTERM. Information shown by pmanager are "
             "now inconsistent.\n");
  } else {
    // By default, the child's SIGTERM handler will reply with a MSG_SUCCESS.
    message_t * response = message_wait(pid);
    message_deinit(response);
  }
}

// Parses arguments from main()'s argv and sets global flags.
//
// argc: the number of arguments
// argv: the array of strings containing arguments
//
// Returns: the process name passed as argument, or NULL if not present.
char * parse_args(int argc, char ** argv) {
  if (argc != 2) {
    return NULL;
  }
  char option;
  while ((option = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
    switch (option) {
      case 'h':
        help_flag = 1;
        break;
    }
  }
  // Check that process name was supplied.
  if (optind >= argc) {
    return NULL;
  }
  return argv[optind];
}

// Prints help about this command. Called on -h (--help) option.
void print_help() {
  printf("Usage:\n");
  printf(" pnew <NAME>\n");
  printf(" Start a new process with name <NAME>.\n");
  printf("\n");
  printf("Options:\n");
  printf(" -h, --help          show this help\n");
}

// Performs cleanup operations.
void cleanup() {
  // Close FIFO.
  if (fifo_fd != -1) {
    close(fifo_fd);
  }
}
