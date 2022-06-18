#include <stdlib.h>
#include <stdio.h>
#include "proc_tree.h"
#include "message.h"

void msg_add_handler(const message_t * msg, proc_node * root) {

  int success = 0;

  proc_node * new_proc = proc_node_fromstr(msg->content);
  if (new_proc == NULL) {
    fprintf(stderr, "Error: failed to create node for process.\n");
  } else {
    if (proc_node_add(root, new_proc) != 0) {
      fprintf(stderr, "Error: failed to add new process to the process tree.\n");
    } else {
      success = 1;
    }
    proc_node_deinit(new_proc);
  }

  // Send result of add to msg->pid_sender
  char * reply_type = (success) ? MSG_SUCCESS : MSG_ERROR;
  message_send(msg->pid_sender, reply_type, NULL);

}

void msg_info_handler(const message_t * msg, proc_node * root) {
  const proc_node * proc = (const proc_node *) proc_node_find_by_name(root, msg->content);
  char * proc_str = NULL;
  int send_status;
  if (proc == NULL) {
    send_status = message_send(msg->pid_sender, MSG_ERROR, "process not found");
  } else if (proc_node_tostr(proc, &proc_str) == -1) {
    send_status = message_send(msg->pid_sender, MSG_ERROR, "failed to get process string");
  } else {
    send_status = message_send(msg->pid_sender, MSG_INFO, proc_str);
    free(proc_str);
  }
  if (send_status != 0) {
    fprintf(stderr, "Error: failed to send message.\n");
  }
}

void msg_remove_handler(const message_t * msg, proc_node * root) {
  int send_status;
  // The message sender is assumed to be also the process to remove.
  if (proc_node_remove(root, msg->pid_sender) == 0) {
    send_status = message_send(msg->pid_sender, MSG_SUCCESS, NULL);
  } else {
    send_status = message_send(msg->pid_sender, MSG_ERROR, "failed to remove process from tree");
  }
  if (send_status != 0) {
    fprintf(stderr, "Error: failed to send message.\n");
  }
}

void msg_list_handler(const message_t * msg, proc_node * root) {

  proc_node * initial_node = proc_node_find_by_name(root, msg->content);

  if (initial_node == NULL) {
    if (message_send(msg->pid_sender, MSG_ERROR, "process not found") != 0) {
      fprintf(stderr, "Error: failed to send message.\n");
    }
    return;
  }

  int count;
  proc_node ** procs = proc_node_get_array(initial_node, &count);

  if (procs == NULL) {
    if (message_send(msg->pid_sender, MSG_ERROR, "failed to get process list") != 0) {
      fprintf(stderr, "Error: failed to send message.\n");
    }
    return;
  }

  // Send processes to plist/ptree.
  int i;
  for (i = 0; i < count; i++) {
    char * proc_str = NULL;
    int send_status;
    if (proc_node_tostr(procs[i], &proc_str) == -1) {
      send_status = message_send(msg->pid_sender, MSG_ERROR, "failed to get process string");
    } else {
      send_status = message_send(msg->pid_sender, MSG_INFO, proc_str);
      free(proc_str);
    }
    if (send_status != 0) {
      fprintf(stderr, "Error: failed to send message.\n");
    }
    // Wait read confirmation from plist/ptree before sending next process.
    message_t * response = message_wait(msg->pid_sender);
    message_deinit(response);
  }

  // Inform plist/ptree that there are no more processes left.
  if (message_send(msg->pid_sender, MSG_SUCCESS, NULL) != 0) {
    fprintf(stderr, "Error: failed to send message.\n");
  }

  // Free memory.
  for (i = 0; i < count; i++) {
    proc_node_deinit(procs[i]);
  }
  free(procs);

}
