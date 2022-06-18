#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include "child.h"
#include "message.h"
#include "proc_tree.h"

// Keeps track of how many times this process has been cloned.
int clones_count = 0;
// PID of pmanager.
pid_t pid_pmanager;
// Name of this process.
char * child_name = NULL;
// Flag for SIGTERM handler.
int sigterm_flag = 0;
// PID of the last process that sent SIGTERM to this process.
pid_t sigterm_sender;

// Private functions.
// Terminates child.
void child_terminate();
// Creates a clone of child.
void child_clone(pid_t sender);
// Removes zombie child process.
void remove_zombie(int sig);
// Resumes a process waiting for MSG_SUCCESS from this child.
void resume_process(pid_t pid);
// Sets sigterm_flag to true.
void set_sigterm_flag(int flag);
// Returns current value of sigterm_flag.
int get_sigterm_flag();
// Returns the PID of the last process that sent SIGTERM to this process.
pid_t get_sigterm_sender();
// SIGTERM signal handler.
void sigterm_handler(int signum, siginfo_t * siginfo, void * context);
// Sends information about a new clone to pmanager.
int send_proc_to_pmanager(const char * name, pid_t pid, pid_t ppid);

// Handler for SIGTERM. It sets sigterm_flag to true and saves the PID of the
// process that sent the signal in sigterm_sender.
//
// signum: signal number that was received
// siginfo: contains information about received signal (PID)
// context: not used. See `man sigaction`.
void sigterm_handler(int signum, siginfo_t * siginfo, void * context) {
  sigterm_flag = 1;
  sigterm_sender = siginfo->si_pid;
}

// Returns the PID of the last process that sent SIGTERM to this process.
pid_t get_sigterm_sender() {
  return sigterm_sender;
}

// Sets the value of sigterm_flag.
//
// flag: the new value for sigterm_flag
void set_sigterm_flag(int flag) {
  sigterm_flag = flag;
}

// Returns the current value of sigterm_flag.
int get_sigterm_flag() {
  return sigterm_flag;
}

// Removes any child of this process that is in zombie state. This is done by
// calling wait() once.
void remove_zombie(int sig) {
  wait(NULL);
}

// Resumes a process that is waiting for MSG_SUCCESS.
//
// pid: the PID of the process to resume
void resume_process(pid_t pid) {
  if (message_send(pid, MSG_SUCCESS, NULL) != 0) {
    fprintf(stderr, "%s: Error: failed to resume %ld.\n", child_name, (long) pid);
  }
}

// Sets the value of child_name.
//
// name: the new value for child_name
void child_set_name(const char * name) {
  child_name = realloc(child_name, sizeof(char) * (strlen(name) + 1));
  strcpy(child_name, name);
}

// Sets the value of pid_pmanager.
//
// pmanager: the PID of pmanager
void child_set_pmanager(pid_t pmanager) {
  pid_pmanager = pmanager;
}

// Sets child_name, pid_pmanager, and signal SIGTERM, SIGCHLD handlers. Puts
// child in wait for signals/messages.
//
// name: the name of the child process
// pmanager: the PID of pmanager
void child_init(const char * name, pid_t pmanager) {

  // Set child name.
  child_set_name(name);
  // Set PID of pmanager.
  child_set_pmanager(pmanager);

  // Register SIGCHLD handler to wait terminated children. This is necessary to
  // remove zombie processes.
  struct sigaction action_chld;
  sigemptyset(&action_chld.sa_mask);
  action_chld.sa_handler = remove_zombie;
  action_chld.sa_flags = 0;
  sigaction(SIGCHLD, &action_chld, NULL);

  // Register SIGTERM handler.
  struct sigaction action_term;
  action_term.sa_sigaction = sigterm_handler;
  action_term.sa_flags = SA_SIGINFO;
  sigaction(SIGTERM, &action_term, NULL);

  // Signal mask for sigsuspend.
  sigset_t mask;
  sigemptyset(&mask);
  while (1) {
    // Suspend the process until delivery of a signal. Normally, these will be:
    // - SIGUSR1 for a new message (FIFO)
    // - SIGTERM for a termination request by pclose/prmall
    // - SIGCHLD for a terminated child
    sigsuspend(&mask);
    // At this point, a signal was received.
    // If signal is SIGTERM, terminate the process.
    if (get_sigterm_flag() == 1) {
      child_terminate();
    }
    // If there is a message, read it.
    if (message_unread()) {
      message_t * msg = message_read();
      // If type of message is MSG_SPAWN, clone this process.
      if(strcmp(msg->type, MSG_SPAWN) == 0) {
        child_clone(msg->pid_sender);
      }
      message_deinit(msg);
    }
  }
}

// Terminates child after requesting pmanager its removal from tree. If pmanager
// replies with MSG_SUCCESS, this process can be safely killed; if MSG_ERROR is
// received, it means that removal from tree failed, and this process cannot be
// killed. MSG_ERROR is received if, for example, this process has children.
void child_terminate() {

  // Send MSG_REMOVE to pmanager.
  if (message_send(pid_pmanager, MSG_REMOVE, NULL) != 0) {
    fprintf(stderr, "%s: Error: failed to send message.\n", child_name);
    resume_process(get_sigterm_sender());
    return;
  }

  // Read response from pmanager.
  message_t * response = message_wait(pid_pmanager);
  // If reply is MSG_SUCCESS, it means that this node is a leaf, and pmanager
  // already removed this node from tree. We can exit().
  int success = 0;
  if (strcmp(response->type, MSG_SUCCESS) == 0) {
    printf("%s: Killing myself...\n", child_name);
    success = 1;
  } else if (strcmp(response->type, MSG_ERROR) == 0) {
    fprintf(stderr, "%s: Error: failed to kill myself. Maybe I have children?\n", child_name);
  } else {
    fprintf(stderr, "%s: Error: unexpected message.\n", child_name);
  }

  // Tell SIGTERM sender that I'm done.
  resume_process(get_sigterm_sender());

  // Exit only if MSG_SUCCESS was received.
  if (success) {
    free(child_name);
    exit(EXIT_SUCCESS);
  }

  // If it did not exit, reset sigterm flag for future use.
  set_sigterm_flag(0);

}

// Sends information about a new process to pmanager.
//
// name: the name of the new process
// pid: the PID of the new process
// ppid: the PPID of the new process
//
// Returns: on success, 0 is returned; on failure, -1 is returned.
int send_proc_to_pmanager(const char * name, pid_t pid, pid_t ppid) {

  // Create proc_node.
  proc_node * proc = proc_node_init(pid, ppid, name);
  if (proc == NULL) {
    return -1;
  }

  // Convert proc_node to string.
  char * proc_str;
  int status = proc_node_tostr(proc, &proc_str);
  proc_node_deinit(proc);
  if (status == -1) {
    return -1;
  }

  // Request pmanager to add the new process to its process tree.
  status = message_send(pid_pmanager, MSG_ADD, proc_str);
  free(proc_str);
  if (status != 0) {
    return -1;
  }

  // Wait response from pmanager.
  message_t * response = message_wait(pid_pmanager);
  if (response == NULL) {
    return -1;
  }
  status = strcmp(response->type, MSG_SUCCESS);
  message_deinit(response);
  // If type of message is not MSG_SUCCESS, return failure.
  if (status != 0) {
    return -1;
  }

  return 0;

}

// Creates a clone of this child by calling fork(). If the name for the new
// process already exists, fork() is aborted.
//
// sender: the PID of the process that requested clonation
void child_clone(pid_t sender) {

  printf("%s: Clonation request received.\n", child_name);

  // Create name for the new process.
  char * new_name;
  if (asprintf(&new_name, "%s_%i", child_name, clones_count + 1) == -1) {
    fprintf(stderr, "%s: Error: failed create name for clone. Clonation aborted.\n", child_name);
    // Resume process that sent clone request.
    resume_process(sender);
    return;
  }

  // Check if a process with name new_name already exists by requesting pmanager
  // information (MSG_INFO) about a process with name new_name.
  if (message_send(pid_pmanager, MSG_INFO, new_name) != 0) {
    fprintf(stderr, "%s: Error: unable to check for duplicates. Clonation aborted.\n",
            child_name);
    resume_process(sender);
    free(new_name);
    return;
  }

  // Wait response from pmanager.
  message_t * response = message_wait(pid_pmanager);
  // If response type is MSG_INFO, it means that a process with name new_name
  // already exists and we must abort clonation.
  int exists = strcmp(response->type, MSG_INFO) == 0;
  message_deinit(response);
  if (exists) {
    fprintf(stderr, "%s: Error: a process with name \"%s\" already exists. "
            "Clonation aborted.\n",
            child_name, new_name);
    free(new_name);
    resume_process(sender);
    return;
  }

  // Fork process.
  pid_t pid = fork();

  if (pid == -1) {
    // Fork failed.
    fprintf(stderr, "%s: Error: failed to fork.\n", child_name);
  } else if (pid == 0) {
    // Child
    // Reset number of clones.
    clones_count = 0;
    // Set child name.
    child_set_name(new_name);
    free(new_name);
  } else {
    // Parent
    // Send process to pmanager.
    if (send_proc_to_pmanager(new_name, pid, getpid()) != 0) {
      fprintf(stderr, "%s: Error: failed to send process information to pmanager. "
                      "Killing the new clone...", child_name);
      // If pmanager failed to add the new process, we need to kill it to avoid
      // inconsistency between tree in pmanager and processes that are actually alive.
      if (kill(pid, SIGTERM) != 0) {
        printf("%s: Failed to send SIGTERM to the new clone. Information shown "
               "by pmanager are now inconsistent.\n", child_name);
      }
    } else {
      // Update number of clones.
      clones_count++;
      printf("%s: Process \"%s\" successfully created.\n", child_name, new_name);
      free(new_name);
    }
    resume_process(sender);
  }

}
