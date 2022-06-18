#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include "common.h"
#include "message.h"

// Global variables accessed by cleanup().
// Output stream of pinfo.
FILE * pinfo_output = NULL;
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

  // Execute pinfo to obtain pid of the process to close.
  // stderr is redirected to /dev/null to ensure that the only output pinfo
  // can print is the pid of the requested process.

  // Create string of command to execute.
  char * cmd;
  if (asprintf(&cmd, "pinfo --pid-pmanager %ld --pid-only %s 2>/dev/null",
      (long) getppid(), proc_name) == -1) {
    fprintf(stderr, "Error: failed to obtain information about process \"%s\".\n", proc_name);
    exit(EXIT_FAILURE);
  }

  // Execute pinfo and read its output.
  pinfo_output = popen(cmd, "r");
  free(cmd);
  if (pinfo_output == NULL) {
    fprintf(stderr, "Error: failed to obtain information about process \"%s\".\n", proc_name);
    exit(EXIT_FAILURE);
  }
  // Read a single line from output.
  char * pid_str;
  size_t n = 0;
  ssize_t count = getline(&pid_str, &n, pinfo_output);
  // If there is no output, it means that pinfo did not found the requested
  // process, because stderr is redirected to /dev/null.
  if (count == -1) {
    fprintf(stderr, "Error: process not found.\n");
    exit(EXIT_FAILURE);
  }

  // Send SIGTERM to process.
  int success = 0;
  // Convert string to pid_t.
  pid_t pid = atol(pid_str);
  free(pid_str);
  printf("Sending SIGTERM to %ld...\n", (long) pid);
  if (kill(pid, SIGTERM) == 0) {
    success = 1;
  } else {
    fprintf(stderr, "Error: failed to send SIGTERM.\n");
  }

  // By default, the process' SIGTERM handler will reply with a MSG_SUCCESS.
  message_t * response =  message_wait(pid);
  message_deinit(response);

  exit(success ? EXIT_SUCCESS : EXIT_FAILURE);

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
  printf(" pclose <NAME>\n");
  printf(" Close process with name <NAME>.\n");
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
  // Close pinfo output stream.
  if (pinfo_output != NULL) {
    pclose(pinfo_output);
  }
}
