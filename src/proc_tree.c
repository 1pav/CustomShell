#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "proc_tree.h"
#include "common.h"

// Special characters used for tree print.
#define BCS_CBL "\x6D"
#define BORDER_MODE "\x1b(0"
#define NORMAL_MODE "\x1b(B"

// Private functions.
// Populates *array with all the nodes contained in node recursively.
void proc_node_get_array_rec(const proc_node * node, proc_node *** array,
                             int * size, int * count);
// Prints the names of all processes contained in node as a tree recursively.
void proc_node_print_tree_rec(const proc_node * node, int depth);
// Finds node in root by pid.
proc_node * proc_node_find_by_pid(proc_node * root, pid_t pid);
// Removes node from children array of another node.
int remove_child(proc_node * node, pid_t pid);

// Initializes a new node representing a process with pid, ppid and name.
//
// pid: the PID of the process
// ppid: the PPID of the process
// name: the name of the process
//
// Returns: a new node.
proc_node * proc_node_init(pid_t pid, pid_t ppid, const char * name) {
  proc_node * new_node = malloc(sizeof(proc_node));
  new_node->pid = pid;
  new_node->ppid = ppid;
  new_node->name = malloc(sizeof(char) * (strlen(name) + 1));
  strcpy(new_node->name, name);
  new_node->children_size = 0;
  new_node->children_count = 0;
  new_node->children = NULL;
  return new_node;
}

// Frees memory allocated for node recursively.
// node: the node to deallocate
void proc_node_deinit(proc_node * node) {
  if (node != NULL) {
    int i;
    for (i = 0; i < node->children_count; i++) {
      proc_node_deinit(node->children[i]);
    }
    free(node->name);
    free(node->children);
    free(node);
  }
}

// Adds a node to the process tree represented by root. The node is added as
// child of the node in root whose pid equals the ppid of the node being added.
//
// root: the root node of the tree where the node is added
// node: the node to add
//
// Returns: on success, 0 is returned; if no suitable parent node is found, -1
// is returned.
int proc_node_add(proc_node * root, const proc_node * node) {
  // Find parent node.
  proc_node * parent = proc_node_find_by_pid(root, node->ppid);
  if (parent == NULL) {
    return -1;
  }
  // Realloc memory if necessary.
  if (parent->children_size == parent->children_count) {
    parent->children_size = (parent->children_size == 0) ? 2 : 2 * parent->children_size;
    parent->children = realloc(parent->children, sizeof(proc_node *) * parent->children_size);
  }
  // Add child node.
  proc_node *child = proc_node_init(node->pid, node->ppid, node->name);
  parent->children[parent->children_count] = child;
  parent->children_count++;
  return 0;
}

// Removes node from children array of another node.
//
// node: the node that contains the child to remove
// pid: the pid of the child to remove
//
// Returns: on success, 0 is returned; on failure, 1 is returned.
int remove_child(proc_node * node, pid_t pid) {
  // Fail if node is NULL.
  if (node == NULL) {
    return 1;
  }
  int found = 0;
  int i;
  for (i = 0; (i < node->children_count) && !found; i++) {
    if (node->children[i]->pid == pid) {
      // Deallocate node.
      proc_node_deinit(node->children[i]);
      // Move elements after child one position left.
      int j;
      for (j = i; j < (node->children_count - 1); j++) {
        node->children[j] = node->children[j+1];
      }
      // Update children_count.
      (node->children_count)--;
      // Stop loop.
      found = 1;
    }
  }
  return !found;
}

// Removes a *leaf* node from the tree represented by root.
//
// root: the root node of the tree
// pid: the pid of the node to remove
//
// Returns: on success, 0 is returned, on failure, 1 is returned. Failure
// can occur if, for example, the node to remove is not a leaf node, or if a
// node with specified pid does not exist in root.
int proc_node_remove(proc_node * root, pid_t pid) {
  proc_node * node = proc_node_find_by_pid(root, pid);
  if (node == NULL) {
    return 1;
  }
  // If node is not leaf, we cannot remove it.
  if (node->children_count != 0) {
    return 1;
  }
  // Retrieve parent node to remove child.
  proc_node * parent = proc_node_find_by_pid(root, node->ppid);
  if (node == NULL) {
    return 1;
  }
  return remove_child(parent, pid);
}

// Finds node by pid recursively.
//
// node: the node from which search is performed
// pid: the process id used to match the searched node
//
// Returns: a pointer to the matching node, or NULL if no matching node was found.
proc_node * proc_node_find_by_pid(proc_node * node, pid_t pid) {
  if (node == NULL || node->pid == pid) {
    return node;
  }
  proc_node * result = NULL;
  int i;
  for (i = 0; (i < node->children_count) && (result == NULL); i++) {
    result = proc_node_find_by_pid(node->children[i], pid);
  }
  return result;
}

// Finds node by name recursively.
//
// node: the node from which search is performed
// name: the name of the process used to match the searched node
//
// Returns: a pointer to the matching node, or NULL if no matching node was found.
proc_node * proc_node_find_by_name(proc_node * node, char * name) {
  if (node == NULL || strcmp(node->name, name) == 0) {
    return node;
  }
  proc_node * result = NULL;
  int i;
  for (i = 0; (i < node->children_count) && (result == NULL); i++) {
    result = proc_node_find_by_name(node->children[i], name);
  }
  return result;
}

// Returns an array of pointers to proc_node, representing all the nodes
// contained in root recursively. Please see proc_node_get_array_rec() for the
// implementation.
//
// root: the root node of the tree
// count: the number of elements contained in the array returned
//
// Returns: the array of nodes (pointers to proc_node) contained in root.
proc_node ** proc_node_get_array(const proc_node * root, int * count) {
  proc_node ** array = NULL;
  *count = 0;
  int size = 0;
  proc_node_get_array_rec(root, &array, &size, count);
  // Realloc array size to match count before returning.
  if (size > *count) {
    array = realloc(array, sizeof(proc_node *) * (*count));
  }
  return array;
}

// Populates *array with all the nodes contained in node recursively.
//
// node: the node containing the nodes to add
// array: a pointer to the array of proc_node to be populated
// size: the size of *array
// count: the number of elements contained in *array
void proc_node_get_array_rec(const proc_node * node, proc_node *** array, int * size, int * count) {
  if (node != NULL) {
    // Before adding node to array, check if a realloc is necessary.
    if (*count == *size) {
      *size = (*size == 0) ? 10 : 2 * (*size);
      *array = realloc(*array, sizeof(proc_node *) * (*size));
    }
    proc_node * new_node = proc_node_init(node->pid, node->ppid, node->name);
    (*array)[*count] = new_node;
    (*count)++;
    int i;
    for (i = 0; i < node->children_count; i++) {
      proc_node_get_array_rec(node->children[i], array, size, count);
    }
  }
}

// Prints the names of all processes contained in root as a tree. Please see
// proc_node_print_tree_rec() for the implementation.
//
// root: the root node of the tree
void proc_node_print_tree(const proc_node * root) {
  proc_node_print_tree_rec(root, 0);
  printf("\n");
}

// Prints the names of all processes contained in node as a tree recursively.
//
// node: the node being printed
// depth: depth of the tree, used for indentation
void proc_node_print_tree_rec(const proc_node * node, int depth) {
  int i, j;
  if (node != NULL) {
    for (j = 1; j <= depth; j++) {
      printf("\t");
    }
    if (depth == 0)
      printf("%s", node->name);
    else
      printf(BORDER_MODE BCS_CBL NORMAL_MODE " %s", node->name);
    depth++;
    for (i = 0; i < node->children_count; i++) {
      printf("\n");
      proc_node_print_tree_rec(node->children[i], depth);
    }
    depth--;
  }
}

// Creates a string representation of a proc_node struct. The string is
// formatted as: <pid>;<ppid>;<name>.
//
// node: a pointer to the proc_node to convert to string
// proc_str: a pointer to the string where the node string is copied
//
// Returns: when successful, it returns the number of characters contained in
// proc_str. If memory allocation wasn't possible, or some other error occurs,
// the functions will return -1, and the contents of proc_str are undefined.
int proc_node_tostr(const proc_node * node, char ** proc_str) {
  return asprintf(proc_str, "%ld;%ld;%s", (long) node->pid, (long) node->ppid, node->name);
}

// Creates a new proc_node from its string representation. The string is assumed
// to be formatted as: <pid>;<ppid>;<name>. See also: proc_node_tostr().
//
// node_str: the string representation the node to create
//
// Returns: on success, a pointer to the new proc_node is returned. On failure,
// NULL is returned (e.g. malformed string).
proc_node * proc_node_fromstr(const char * node_str) {
  proc_node *new_node = NULL;
  char ** tokens;
  int tokens_count = tokenize(node_str, &tokens, ";");
  if (tokens_count == 3) {
    pid_t pid = atol(tokens[0]);
    pid_t ppid = atol(tokens[1]);
    char * name = tokens[2];
    new_node = proc_node_init(pid, ppid, name);
  }
  // Free tokens
  int i;
  for (i = 0; i < tokens_count; i++) {
    free(tokens[i]);
  }
  free(tokens);
  return new_node;
}
