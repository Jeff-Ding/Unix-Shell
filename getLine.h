// getLine.h                                      Stan Eisenstat (10/27/09)

// Read a line of text using the file pointer *fp and returns a pointer to a
// null-terminated string that contains the text read, include the newline (if
// any) that ends the line.  Storage for the line is allocated with malloc()
// and realloc().  If the end of the file is reached before any characters are
// read, then the NULL pointer is returned.

#include <stdio.h>

char *getLine(FILE *fp);
