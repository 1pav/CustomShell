#ifndef MESSAGE_H
#define MESSAGE_H

#include <sys/types.h>

// Message types used in message_t.
#define MSG_ADD "a"
#define MSG_REMOVE "r"
#define MSG_INFO "i"
#define MSG_ERROR "e"
#define MSG_SUCCESS "s"
#define MSG_LIST "l"
#define MSG_SPAWN "p"

// Represents a message exchanged between processes.
typedef struct message_t {
  // The PID of the process that sent this message.
  pid_t pid_sender;
  // The type of this message. See macros defined above.
  char * type;
  // The content of this message.
  char * content;
} message_t;

// Sets file descriptor of the FIFO used for exchanging messages, and registers
// handler for SIGUSR1.
int message_setup(int fd);
// Frees memory allocated for a message_t struct.
void message_deinit(message_t *msg);
// Send a message to pid. The message string is encoded as:
// <pid_sender>:<type>:<content>.
int message_send(pid_t pid, const char * type, const char * content);
// Reads a message from FIFO.
message_t * message_read();
// Returns true if a message was received; otherwise, it returns false.
int message_unread();
// Waits until a message from pid is received. If from is -1, then a message from
// any pid is waited. If a message was already received, the function returns
// immediatly.
message_t * message_wait(pid_t from);

#endif
