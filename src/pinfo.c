#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <getopt.h>
#include "common.h"
#include "message.h"
#include "proc_tree.h"

// Global variables accessed by cleanup().
// File descriptor of FIFO.
int fifo_fd = -1;
// Flag for --pid-only
int pid_only_flag = 0;
// Flag for --help
int help_flag = 0;
// Flag for --pid-pmanager
pid_t pid_pmanager;

// Option arguments. Normally used by pclose.
const char * short_options = "m:ph";
const struct option long_options[] = {
    {"pid-pmanager", required_argument, NULL, 'm'},
    {"pid-only", no_argument, NULL, 'p'},
    {"help", no_argument, NULL, 'h'},
    {0, 0, 0, 0}
};

// Utility functions.
// Performs cleanup operations on exit.
void cleanup();
// Parses arguments and sets global flags.
char * parse_args();
// Prints help about this command.
void print_help();

void main(int argc, char ** argv) {

  // Default PID of pmanager is assumed to be the the PPID of this process.
  pid_pmanager = getppid();

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

  // Request information about process with name proc_name to pmanager.
  if (message_send(pid_pmanager, MSG_INFO, proc_name) != 0) {
    fprintf(stderr, "Error: failed to send message.\n");
    exit(EXIT_FAILURE);
  }

  // Wait response from pmanager.
  message_t * response = message_wait(pid_pmanager);

  // If message is not valid, exit.
  if (response == NULL) {
    fprintf(stderr, "Error: failed to read message.\n");
    exit(EXIT_FAILURE);
  }
  if (strcmp(response->type, MSG_ERROR) == 0) {
    fprintf(stderr, "Error: %s\n", response->content);
    exit(EXIT_FAILURE);
  }
  if (strcmp(response->type, MSG_INFO) != 0) {
    fprintf(stderr, "Error: unrecognized message.\n");
    exit(EXIT_FAILURE);
  }

  // Convert process string to proc_node.
  proc_node * proc = proc_node_fromstr(response->content);
  if (proc == NULL) {
    fprintf(stderr, "Error: failed to get process node from string.\n");
    exit(EXIT_FAILURE);
  }
  // Print process information based on pid_only_flag.
  if (pid_only_flag) {
    printf("%ld\n", (long) proc->pid);
  } else {
    printf("Name : %s\nPID  : %ld\nPPID : %ld\n", proc->name, (long) proc->pid, (long) proc->ppid);
  }

  // Free memory.
  proc_node_deinit(proc);
  message_deinit(response);

  exit(EXIT_SUCCESS);
}

// Parses arguments from main()'s argv and sets global flags.
//
// argc: the number of arguments
// argv: the array of strings containing arguments
//
// Returns: the process name passed as argument, or NULL if not present.
char * parse_args(int argc, char ** argv) {
  char option;
  while ((option = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
    switch (option) {
      case 'm':
        pid_pmanager = atol(optarg);
        break;
      case 'p':
        pid_only_flag = 1;
        break;
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
  printf(" pinfo [OPTIONS] <NAME>\n");
  printf(" Show information about process with name <NAME>.\n");
  printf("\n");
  printf("Options:\n");
  printf(" -m, --pid-pmanager=PID    use PID as pid for pmanager\n");
  printf(" -p, --pid-only            print only pid of the process\n");
  printf(" -h, --help                show this help\n");
}

// Performs cleanup operations.
void cleanup() {
  // Close FIFO.
  if (fifo_fd != -1) {
    close(fifo_fd);
  }
}
