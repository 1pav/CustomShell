#ifndef UTIL_H
#define UTIL_H

// Custom PATH environment variable.
#define PATH "./bin/"
// Default name for FIFO.
#define FIFO_NAME "tmp"

// Utility functions.
// Copy tokens separated by delimiters found in str into the array pointed by
// tokens_ptr.
int tokenize(const char * str, char *** tokens_ptr, const char * delimiters);

#endif
