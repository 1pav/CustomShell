#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include "message.h"
#include "proc_tree.h"
#include "common.h"

// Global variables accessed by cleanup().
// File descriptor of FIFO.
int fifo_fd = -1;
// Process tree.
proc_node * proc_tree_root = NULL;
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
void parse_args(int argc, char ** argv);
// Prints help about this command.
void print_help();
// Performs cleanup operations on exit.
void cleanup();
// Adds process to local process tree for later print.
void add_process_to_tree(proc_node ** root, const char * proc_str);

void main(int argc, char ** argv) {

  // Check argv options.
  parse_args(argc, argv);

  // If help_flag was set by parse_args(), print help and exit.
  if (help_flag) {
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

  // Ask pmanager to send information about *all* processes started by pmanager.
  if (message_send(getppid(), MSG_LIST, "pmanager") != 0) {
    fprintf(stderr, "Error: failed to send message.\n");
    exit(EXIT_FAILURE);
  }

  int list_end = 0;
  int error = 0;

  // Populate proc_tree_root with processes received from pmanager, until
  // there are no more processes left or an error is encountered.
  while (!list_end && !error) {
      // Wait response from pmanager.
      message_t * response = message_wait(getppid());

      // Check response type.
      if (response == NULL) {
        fprintf(stderr, "Error: failed to read message.\n");
        error = 1;
      } else if (strcmp(response->type, MSG_INFO) == 0) {
        // Add received process to process tree.
        add_process_to_tree(&proc_tree_root, response->content);
        // Inform pmanager that message was received.
        if (message_send(getppid(), MSG_SUCCESS, NULL) != 0) {
          fprintf(stderr, "Error: failed to send message.\n");
          error = 1;
        }
      } else if (strcmp(response->type, MSG_SUCCESS) == 0) {
        // MSG_SUCCESS means pmanager has finished sending processes, so end loop.
        list_end = 1;
      } else if (strcmp(response->type, MSG_ERROR) == 0) {
        fprintf(stderr, "Error: %s.\n", response->content);
        error = 1;
      } else {
        fprintf(stderr, "Error: unrecognized message.\n");
        error = 1;
      }
      message_deinit(response);
  }

  // If end of list was reached, print process tree.
  if (list_end) {
    proc_node_print_tree(proc_tree_root);
    exit(EXIT_SUCCESS);
  }

  exit(EXIT_FAILURE);

}

// Add process represented by proc_str to process tree pointed by *root.
//
// root: a pointer to the proc_node* to which the process is added
// proc_str: the string representation of the process to add
void add_process_to_tree(proc_node **root, const char * proc_str) {
  proc_node *new_node = proc_node_fromstr(proc_str);
  if(*root == NULL)
    *root = new_node;
  else {
    proc_node_add(*root, new_node);
    proc_node_deinit(new_node);
  }
}

// Parses arguments from main()'s argv and sets global flags.
//
// argc: the number of arguments
// argv: the array of strings containing arguments
void parse_args(int argc, char ** argv) {
  if (argc != 1) {
    help_flag = 1;
    return;
  }
  char option;
  while ((option = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
    switch (option) {
      case 'h':
        help_flag = 1;
        break;
    }
  }
}

// Prints help about this command. Called on -h (--help) option.
void print_help() {
  printf("Usage:\n");
  printf(" ptree\n");
  printf(" Show a tree or processes started by the shell.\n");
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
  // Free process tree.
  proc_node_deinit(proc_tree_root);
}
