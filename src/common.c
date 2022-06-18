#include <string.h>
#include <stdlib.h>

// Copy tokens separated by delimiters found in str into the array pointed by
// tokens_ptr.
//
// str: the string to tokenize
// tokens: a pointer to the array to populate with tokens
// delimiters: a string containing the delimiter characters
//
// Returns: the number of tokens found.
int tokenize(const char * str, char *** tokens_ptr, const char * delimiters) {

  char ** tokens = NULL;

  // Size of tokens array.
  size_t tokens_size = 0;
  // Number of tokens found.
  int tokens_count = 0;

  // Make local copy of str to use with strtok(). This avoids altering
  // the original string.
  char * str_tmp = malloc(sizeof(char) * (strlen(str) + 1));
  strcpy(str_tmp, str);

  // Populate tokens array. If no tokens were found or the end of the string is
  // reached, strtok() returns NULL.
  char * current_token = strtok(str_tmp, delimiters);
  while (current_token != NULL) {

    // Realloc memory if necessary.
    if (tokens_count == tokens_size) {
      // Using 2 as default number of tokens: typically, a command is composed of
      // command name + one argument.
      tokens_size = (tokens_size == 0) ? 2 : 2 * tokens_size;
      tokens = realloc(tokens, sizeof(char *) * tokens_size);
    }

    // Add current_token to tokens.
    tokens[tokens_count] = malloc(sizeof(char) * (strlen(current_token) + 1));
    strcpy(tokens[tokens_count], current_token);
    tokens_count++;

    current_token = strtok(NULL, delimiters);

  }

  free(str_tmp);

  // Reallocate memory to match the number of tokens actually found.
  if (tokens_count < tokens_size) {
    tokens = realloc(tokens, sizeof(char *) * tokens_count);
  }

  *tokens_ptr = tokens;

  return tokens_count;
}
