#ifndef CHILD_H
#define CHILD_H

// Sets child name, pmanager PID, and signal handlers. Puts child in wait for
// signal/messages.
void child_init(const char * name, pid_t pmanager);
// Sets name for child process.
void child_set_name(const char * name);
// Sets PID of pmanager for child process.
void child_set_pmanager(pid_t pid);

#endif
