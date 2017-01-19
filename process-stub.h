// process-stub.h                                 Stan Eisenstat (11/06/13)
//
// Backend for Bsh.  See spec for details.

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include "parse.h"

// Execute command list CMDLIST and return status of last command executed
int process (CMD *cmdList);
