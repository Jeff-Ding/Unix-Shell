# Unix-Shell
Backend implementation of UNIX shell from scratch. Project for Stanley Eisenstat's Systems Programming &amp; Computer Organization course at Yale (CS323).

This implementation is based on the Bourne shell, a baby brother of the Bourne-again shell
bash, and offers a limited subset of bash's functionality (plus some extras):
* local variables
* simple command execution with zero or more arguments
* redirection of the standard input (<)
* redirection of the standard output (>, >>)
* pipelines (|) consisting of an arbitrary number of commands, each having zero or more arguments
* backgrounded commands;
* multiple commands per line, separated by ; or & or && or ||
* groups of commands (aka subcommands), enclosed in parentheses
* directory manipulation:
  + cd directoryName
  + cd (equivalent to "cd $HOME", where HOME is an environment variable)
  + dirs (print to stdout the current working directory as reported by getcwd())
* other built-in commands:
  + wait (Wait until all children of the shell process have died.)
* reporting the status of the last simple command, pipeline, or subcommand executed in the foreground by setting the environment variable $? to its "printed" value (e.g., "0" if the value is zero).

## Front End (parse.h)
The syntax for a command is
```
  <stage>    = <simple> / (<command>)
  <pipeline> = <stage> / <pipeline> | <stage>
  <and-or>   = <pipeline> / <and-or> && <pipeline> / <and-or> || <pipeline>
  <sequence> = <and-or> / <sequence> ; <and-or> / <sequence> & <and-or>
  <command>  = <sequence> / <sequence> ; / <sequence> &
```
where a <simple> is a single command with arguments and I/O redirection, but no
|, &, ;, &&, ||, (, or ).

A command is represented by a tree of CMD structs corresponding to its simple
commands and the "operators" PIPE, && (SEP_AND), || (SEP_OR), ; (SEP_END), &
(SEP_BG), and SUBCMD.  The tree corresponds to how the command is parsed by a
bottom-up using the grammar above.

Note that I/O redirection is associated with a <stage> (i.e., a <simple> or
subcommand), but not with a <pipeline> (input/output redirection for the
first/last stage is associated with the stage, not the pipeline).

One way to write such a parser is to associate a function with each syntactic
type.  That function calls the function associated with its first alternative
(e.g., <stage> for <pipeline>), which consumes all tokens immediately following
that could be part of it.  If at that point the next token is one that could
lead to its second alternative (e.g., | in <pipeline> | <stage>), then that
token is consumed and the associated function called again.  If not, then the
tree is returned.

A CMD struct contains the following fields:
```
 typedef struct cmd {
   int type;             // Node type (SIMPLE, PIPE, SEP_AND, SEP_OR,
			 //   SEP_END, SEP_BG, SUBCMD, or NONE)

   int nLocal;           // Number of local variable assignments
   char **locVar;        // Array of local variable names and values to assign
   char **locVal;        //   to them when command executes or NULL (default)

   int argc;             // Number of command-line arguments
   char **argv;          // Null-terminated argument vector

   int fromType;         // Redirect stdin?
			 //  (NONE (default), RED_IN, RED_IN_HERE, RED_IN_CLS)
   char *fromFile;       // File to redirect stdin. contents of here
			 //   document, or NULL (default)

   int toType;           // Redirect stdout?
			 //  (NONE (default), RED_OUT, RED_OUT_APP, RED_OUT_CLS,
			 //   RED_OUT_ERR, RED_OUT_RED)
   char *toFile;         // File to redirect stdout or NULL (default)

   struct cmd *left;     // Left subtree or NULL (default)
   struct cmd *right;    // Right subtree or NULL (default)
 } CMD;
```
The tree for a <simple> is a single struct of type SIMPLE that specifies its
local variables (nLocal, locVar[], locVal[]) and arguments (argc, argv[]);
and whether and where to redirect its standard input (fromType, fromFile) and
its standard output (toType, toFile).  The left and right children are NULL.

The tree for a <stage> is either the tree for a <simple> or a struct
of type SUBCMD (which may have redirection) whose left child is the tree
representing the <command> and whose right child is NULL.

The tree for a <pipeline> is either the tree for a <stage> or a struct
of type PIPE whose left child is the tree representing the <pipeline> and
whose right child is the tree representing the <stage>.

The tree for an <and-or> is either the tree for a <pipeline> or a struct
of type && (= SEP_AND) or || (= SEP_OR) whose left child is the tree
representing the <and-or> and whose right child is the tree representing
the <pipeline>.

The tree for a <sequence> is either the tree for an <and-or> or a struct of
type ; (= SEP_END) or & (= SEP_BG) whose left child is the tree representing
the <sequence> and whose right child is the tree representing the <and-or>.

The tree for a <command> is either the tree for a <sequence> or a struct of
type ; (= SEP_END) or & (= SEP_BG) whose left child is the tree representing
the <sequence> and whose right child is NULL.

While the grammar above captures the syntax of bash commands, it does not
reflect the semantics of &, which specify that only the preceding <and-or>
should be executed in the background, not the entire preceding <sequence>.


Examples (where A, B, C, D, and E are <simple>):
```
//                              Expression Tree
//
//   A                          A
//
//   < a A | B | C | D > d                     PIPE
//                                            /    \
//                                        PIPE      D >d
//                                       /    \
//                                   PIPE      C
//                                  /    \
//                              <a A      B
//
//   A && B || C && D                   &&
//                                     /  \
//                                   ||    D
//                                  /  \
//                                &&    C
//                               /  \
//                              A    B
//
//   A ; B & C ; D || E ;                 ;
//                                      /
//                                     ;
//                                   /   \
//                                  &     ||
//                                 / \   /  \
//                                ;   C D    E
//                               / \
//                              A   B
//
//   (A ; B &) | (C || D) && E                 &&
//                                            /  \
//                                        PIPE    E
//                                       /    \
//                                    SUB      SUB
//                                   /        /
//                                  &       ||
//                                 /       /  \
//                                ;       C    D
//                               / \
//                              A   B
```
