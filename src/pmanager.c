#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "common.h"
#include "message.h"
#include "proc_tree.h"
#include "handlers.h"

// Global variables accessed by cleanup().
// File descriptor of FIFO.
int fifo_fd = -1;
// Input stream (stdin or file).
FILE * input_stream = NULL;
// Tree of processes.
proc_node * proc_tree_root = NULL;

// Utility functions.
// Parses and executes commands from stream.
int parse_commands(FILE * stream);
// Executes a command with arguments, waiting for command termination.
int exec_command(const char * command, char ** argv);
// Default handler for new messages, used as dispatcher.
void message_handler(const message_t * msg);
// Performs memory cleanup.
void cleanup();
// Handler for SIGTERM, SIGINT.
void terminate(int signum);

void main(int argc, char ** argv) {

  // Register cleanup function to be called at process termination.
  atexit(cleanup);

  // Set custom PATH environment variable.
  if (setenv("PATH", PATH, 1) != 0) {
    fprintf(stderr, "Error: failed to set PATH.\n");
    exit(EXIT_FAILURE);
  }

  // Check arguments.
  if (argc == 1) {
    // If there are no arguments, read commands from stdin.
		input_stream = stdin;
	} else if (argc == 2) {
    // If there is an argument, use it as the name of file to open for reading
    // commands.
		input_stream = fopen(argv[1], "r");
		if (input_stream == NULL) {
			fprintf(stderr, "Error: cannot open \"%s\" for reading.\n", argv[1]);
			exit(EXIT_FAILURE);
		}
	} else {
    // Syntax not recognized. Print help before exiting.
		exec_command("phelp", NULL);
		exit(EXIT_FAILURE);
	}

  // Create FIFO for process communication.
  // Defaul umask is 0002, thus final permissions will be: 0602 - 0002 = 0600.
  mode_t mode = 0602;
  if (mkfifo(FIFO_NAME, mode) != 0) {
    fprintf(stderr, "Error: failed to create FIFO.\n");
    exit(EXIT_FAILURE);
  }

  // Open FIFO.
  fifo_fd = open(FIFO_NAME, O_RDWR | O_NONBLOCK);
  if (fifo_fd == -1) {
    fprintf(stderr, "Error: failed to open FIFO.\n");
    exit(EXIT_FAILURE);
  }

  // Setup process communication.
  if (message_setup(fifo_fd) != 0) {
    fprintf(stderr, "Error: failed to setup process communication.\n");
    exit(EXIT_FAILURE);
  }

  // Register handler for SIGTERM, SIGINT.
  struct sigaction action;
  sigemptyset(&action.sa_mask);
  action.sa_handler = terminate;
  action.sa_flags = 0;
  if (sigaction(SIGTERM, &action, NULL) != 0 ||
      sigaction(SIGINT, &action, NULL) != 0) {
    fprintf(stderr, "Error: failed to set signal handlers.\n");
    exit(EXIT_FAILURE);
  }

  // Create process tree with pmanager as root node.
  proc_tree_root = proc_node_init(getpid(), getppid(), "pmanager");
  if (proc_tree_root == NULL) {
    fprintf(stderr, "Error: failed to create process tree.\n");
    exit(EXIT_FAILURE);
  }

  // Print welcome message if stream is stdin.
  if (input_stream == stdin) {
    printf("Welcome to CustomShell!\n\n");
    printf("Type \"phelp\" for information.\n");
  }

  // Parse and execute commands commands.
  if (parse_commands(input_stream) != 0) {
    fprintf(stderr, "Error: something went wrong before reaching EOF.\n");
    exit(EXIT_FAILURE);
  }

	exit(EXIT_SUCCESS);

}

// Parses and executes commands from stream. This function blocks until one of
// the following condition is true:
// - "quit" command was read
// - stream reached EOF
// - getline() returned an error
//
// stream: the FILE pointer of the stream to parse
//
// Returns: on success (EOF or quit command), the function returns 0. On error,
// 1 is returned.
int parse_commands(FILE * stream) {

  // If stream is stdin, show prompt.
  if (stream == stdin) {
		printf("> ");
  }

  // Buffer to store current input line.
  char * buffer = NULL;
  // Size of the buffer.
  size_t bufsize = 0;
  // Flag for quit command.
  int quit = 0;

  // Read input until EOF, getline() error, or quit command.
  while (!quit && getline(&buffer, &bufsize, stream) != -1) {
    // Used in for loops.
    int i;
    // Tokens in current line.
    char ** tokens = NULL;
    // Split line into tokens.
    int tokens_count = tokenize(buffer, &tokens, "\n ");

    // If there are tokens, try to execute command.
    if (tokens_count != 0) {
      // Copy tokens into a new NULL-terminated array. Required by exec_command().
      char * argv[tokens_count + 1];
      for (i = 0; i < tokens_count; i++) {
        argv[i] = malloc(sizeof(char) * (strlen(tokens[i]) + 1));
        strcpy(argv[i], tokens[i]);
      }
      argv[i] = NULL;

      // Execute command with arguments.
      int status = exec_command(argv[0], argv);
      // Print erros based on return value.
      switch (status) {
        case -2:
          fprintf(stderr, "Error: failed to fork process.\n");
          break;
        case -1:
          fprintf(stderr, "Error: command not found.\n");
          break;
        case 1:
          quit = 1;
          break;
      }

      // Free tokens.
      for (i = 0; i < tokens_count; i++) {
        free(tokens[i]);
      }
      free(tokens);
      // Free argv.
      for (i = 0; i < tokens_count + 1; i++) {
        free(argv[i]);
      }

    }

    // If stream is stdin and last command was not quit, show prompt again.
    if (stream == stdin && !quit) {
      printf("> ");
    }

  }

  free(buffer);

  return !(feof(stream) || quit);
}

// Executes command with arguments specified by argv. argv *must* be terminated
// by a NULL pointer, just as exec() system call.
//
// command: the name of the executable
// argv: the arguments passed to the program
//
// Returns:
//  -2 : fork failed
//  -1 : command not found
//   0 : command executed successfully
//   1 : quit command
int exec_command(const char * command, char ** argv) {

  // Check if command is "quit".
  if (strcmp(command, "quit") == 0) {
    return 1;
  }

  // Check if executable exists.
  char * pathname = malloc(sizeof(char) * (strlen(PATH) + strlen(command) + 1));
  pathname[0] = '\0';
  strcat(pathname, PATH);
  strcat(pathname, command);
  int x_ok = access(pathname, X_OK);
  free(pathname);
  if (x_ok != 0) {
    return -1;
  }

  // Fork and execute command.
  pid_t pid = fork();
  if (pid == -1) {
    // Fork failed.
    return -2;
  } else if (pid == 0) {
    // Child executes requested program.
    if (execvp(command, argv) == -1) {
      fprintf(stderr, "Error: failed to exec program.\n");
      exit(EXIT_FAILURE);
    }
  } else {
    // Parent waits until command termination. Meanwhile, any message that is
    // received is handled by message_handler().
    do {
      if (message_unread()) {
        message_t * msg = message_read();
        message_handler(msg);
        message_deinit(msg);
      }
    } while (waitpid(pid, NULL, 0) <= 0);
  }

	return 0;
}

// Generic message handler. It takes care of calling the suitable handler based
// on the type of message received.
//
// msg: the message received
void message_handler(const message_t * msg) {

  if (msg == NULL) {
    fprintf(stderr, "Error: failed to read message.\n");
    return;
  }

  if (strcmp(msg->type, MSG_ADD) == 0) {
    // Add new process to tree.
    msg_add_handler(msg, proc_tree_root);
  } else if (strcmp(msg->type, MSG_INFO) == 0) {
    // Reply with information about process.
    msg_info_handler(msg, proc_tree_root);
  } else if (strcmp(msg->type, MSG_REMOVE) == 0) {
    // Remove process from tree.
    msg_remove_handler(msg, proc_tree_root);
  } else if (strcmp(msg->type, MSG_LIST) == 0) {
    // Reply with information about *all* processes.
    msg_list_handler(msg, proc_tree_root);
  } else {
    // Reply with error message.
    if (message_send(msg->pid_sender, MSG_ERROR, "unrecognized message type") != 0) {
      fprintf(stderr, "Error: failed to send message.\n");
    }
  }

}

// Performs cleanup operations. Called on normal exit.
void cleanup() {
  // Execute prmall to kill processes started by the shell.
  if (proc_tree_root != NULL) {
    printf("Killing remaining processes...\n");
    char * args[] = { "prmall", "pmanager", NULL};
    if (exec_command(args[0], args) != 0) {
      fprintf(stderr, "Failed to kill remaining processes.\n");
    }
    proc_node_deinit(proc_tree_root);
  }
  // Close stream.
  if (input_stream != NULL) {
    fclose(input_stream);
  }
  // Close and unlink FIFO.
  if (fifo_fd != -1) {
    close(fifo_fd);
  }
  unlink(FIFO_NAME);
  printf("Exiting...\n");
}

// Signal handler for SIGTERM and SIGINT. Calling exit() also causes cleanup()
// to be called, because of atexit() function registration in main().
void terminate(int signum) {
  exit(EXIT_SUCCESS);
}
