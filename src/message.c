#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include "message.h"
#include "common.h"

// File descriptor associated to FIFO used for exchanging messages.
int fd;
// Flag for unread messages.
int unread_flag = 0;
// PID of the last process that sent a message.
pid_t message_sender = -1;

// Private functions.
// Initializes a message_t struct.
message_t * message_init(pid_t pid_sender, const char * type, const char * content);
// Sets unread_flag to true.
void set_unread_flag(int signum, siginfo_t * siginfo, void * context);
// Sets unread_flag to false.
void reset_unread_flag();
// Sets message_sender value.
void set_message_sender(pid_t pid);
// Returns the current value of message_sender.
pid_t get_message_sender();

// Handler for SIGUSR1. It sets unread_flag to true and saves the PID of the
// process that sent the signal in message_sender.
//
// signum: signal number that was received
// siginfo: contains information about received signal (PID)
// context: not used. See `man sigaction`.
void set_unread_flag(int signum, siginfo_t * siginfo, void * context) {
  unread_flag = 1;
  message_sender = siginfo->si_pid;
}

// Sets unread_flag to false.
void reset_unread_flag() {
  unread_flag = 0;
}

// Returns the current value of unread_flag.
int message_unread() {
  return unread_flag;
}

// Sets the value of message_sender.
void set_message_sender(pid_t pid) {
  message_sender = pid;
}

// Returns the value of message_sender.
pid_t get_message_sender() {
  return message_sender;
}

// Sets file descriptor of the FIFO used for exchanging messages, and registers
// handler for SIGUSR1.
//
// fifo_fd: the file descriptor of the FIFO to use for exchanging messages.
//
// Returns: on success, 0 is returned; on failure -1 is returned.
int message_setup(int fifo_fd) {
  // Set global file descriptor.
  fd = fifo_fd;
  // Register signal handler for SIGUSR1.
  struct sigaction new_action;
  new_action.sa_sigaction = set_unread_flag;
  sigemptyset(&new_action.sa_mask);
  new_action.sa_flags = SA_SIGINFO;
  return sigaction(SIGUSR1, &new_action, NULL);
}

// Initializes a message_t struct.
//
// pid_sender: the PID of the process that is sending the message
// type: the type of the message
// content: the content of the message
//
// Returns: a pointer to the newly created message_t.
message_t * message_init(pid_t pid_sender, const char * type, const char * content) {
  message_t * msg = malloc(sizeof(message_t));
  if (msg == NULL) {
    return NULL;
  }
  msg->type = malloc(sizeof(char) * (strlen(type) + 1));
  if (msg->type == NULL) {
    free(msg);
    return NULL;
  }
  strcpy(msg->type, type);
  msg->content = malloc(sizeof(char) * (strlen(content) + 1));
  if (msg->content == NULL) {
    free(msg->type);
    free(msg);
    return NULL;
  }
  strcpy(msg->content, content);
  msg->pid_sender = pid_sender;
  return msg;
}

// Frees memory allocated for a message_t struct.
//
// msg: a pointer to the message struct to free
void message_deinit(message_t * msg) {
  if (msg != NULL) {
    free(msg->type);
    free(msg->content);
    free(msg);
  }
}

// Waits a message from PID. If a message was already received, the function
// returns immediately. If from is set to -1, a message from *any* pid is
// waited.
//
// from: the PID of the process from which a message is waited
//
// Returns: a pointer to the message received. If there was an error in reading
// the message, NULL is returned.
message_t * message_wait(pid_t from) {
  sigset_t mask;
  sigemptyset(&mask);
  // While unread_flag is false or message_sender is *not* equal to from, wait.
  // If from equals to -1 and there is an unread message, stop waiting.
  while (!message_unread() || (from != get_message_sender() && from != -1)) {
    sigsuspend(&mask);
  }
  return message_read();
}

// Sends a message to process. The message string is encoded as follows:
// <pid_sender>:<type>:<content>.
// The string is written to the FIFO associated to fd file descriptor, and the
// receiver is notified about the new message by sending SIGUSR1.
//
// pid: the pid of the process to which the message is sent
// type: the type of the message
// content: the content of the message
//
// Returns: on success, 0 is returned; on error, -1 is returned.
int message_send(pid_t pid, const char * type, const char * content) {
  // Assume failure.
  int status = -1;
  // Encode message.
  char * msg_str;
  // If content is NULL, replace content field with a default padding.
  const char * content_ok = (content == NULL) ? "NULL" : content;
  // Return error if asprintf() failed.
  if (asprintf(&msg_str, "%ld:%s:%s", (long) getpid(), type, content_ok) == -1) {
    return status;
  }
  // Write string to FIFO and notify receiver about message by sendining SIGUSR1.
  if ((write(fd, msg_str, strlen(msg_str) + 1) != -1) &&
      (kill(pid, SIGUSR1) == 0)) {
    // Success.
    status = 0;
  }
  free(msg_str);
  return status;
}

// Reads a null-terminated string from fifo into a message_t. This function is
// blocking unless the file descriptor has been opened with the O_NONBLOCK flag.
//
// Returns: on success, a pointer to message_t is returned. On error (malformed
// message string or read() error, NULL is returned. Please check errno for any
// error set by read() system call.
message_t * message_read() {

  int read_count = 0;
  int msg_size = 100; // Default size for msg_str
  char * msg_str = malloc(sizeof(char) * msg_size);

  char tmp; // Single character read from FIFO.
  int str_end = 0; // Flag for end of string.

  // Read one character at a time until null character or EOF/error.
  while (str_end == 0 && read(fd, &tmp, 1) > 0) {
    // Realloc memory pointed by msg_str if necessary.
    if (read_count == msg_size) {
      msg_size *= 2;
      msg_str = realloc(msg_str, sizeof(char) * msg_size);
    }
    // Copy character to msg_str.
    msg_str[read_count] = tmp;
    read_count++;
    // If it was null character, stop reading.
    if (tmp == '\0') {
      str_end = 1;
    }
  }

  // If end of string was not reached, return NULL.
  if (!str_end) {
    free(msg_str);
    return NULL;
  }

  // Parse string.
  message_t * msg = NULL;
  char ** msg_fields;
  // Split message into fields.
  int fields_count = tokenize(msg_str, &msg_fields, ":");
  // We expect the message to have exactly 3 fields (pid_sender, type, content).
  if (fields_count == 3) {
    // Return a non-NULL message.
    msg = message_init(atol(msg_fields[0]), msg_fields[1], msg_fields[2]);
  }

  // Free msg_fields.
  int i;
  for (i = 0; i < fields_count; i++) {
    free(msg_fields[i]);
  }
  free(msg_fields);
  // Free msg_str.
  free(msg_str);

  // Set unread_flag to false.
  reset_unread_flag();
  // Reset message_sender flag.
  set_message_sender(-1);

  return msg;
}
