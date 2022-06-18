#ifndef PROC_TREE_H
#define PROC_TREE_H

// Represents a process.
typedef struct proc_node {
  pid_t pid;
  pid_t ppid;
  char * name;
  struct proc_node ** children;
  int children_count;
  int children_size;
} proc_node;

// Initializes a new node representing a process with pid, ppid and name.
proc_node * proc_node_init(pid_t pid, pid_t ppid, const char * name);
// Frees memory allocated for a node recursively.
void proc_node_deinit(proc_node * node);
// Adds a node to the process tree represented by root. The node is added as
// child of the node in root whose pid equals the ppid of the node being added.
int proc_node_add(proc_node * root, const proc_node * node);
// Removes a *leaf* node from the tree represented by root.
int proc_node_remove(proc_node * root, pid_t pid);
// Finds node by name recursively.
proc_node * proc_node_find_by_name(proc_node * node, char * name);
// Returns an array of pointers to proc_node, representing all the nodes
// contained in root recursively
proc_node ** proc_node_get_array(const proc_node * root, int * count);
// Prints the names of all processes contained in root as a tree.
void proc_node_print_tree(const proc_node * root);
// Creates a string representation of a proc_node struct. The string is
// formatted as: <pid>;<ppid>;<name>.
int proc_node_tostr(const proc_node * node, char ** proc_str);
// Creates a new proc_node from its string representation. The string is assumed
// to be formatted as: <pid>;<ppid>;<name>.
proc_node * proc_node_fromstr(const char *node_str);

#endif
