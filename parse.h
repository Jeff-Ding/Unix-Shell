// parse.h                                 Stan Eisenstat (11/03/15)
//
// Header file for command line parser used in Parse
//
// bash version based on left-associative expression parse tree

#ifndef PARSE_INCLUDED
#define PARSE_INCLUDED          // parse.h has been #include-d


// A token is
//
// (1) a maximal, contiguous, nonempty sequence of nonwhitespace characters
//     other than the metacharacters <, >, ;, &, |, (, and ) [a SIMPLE token];
//
// (2) a redirection symbol (<, >, >>, or |);
//
// (3) a command terminator (;, &, &&, or ||);
//
// (4) a left or right parenthesis (used to group commands).


// String containing all metacharacters that terminate SIMPLE tokens
#define METACHAR "<>;&|()"


// A token list is a headless linked list of typed tokens.  All storage is
// allocated by malloc() / realloc().  The token type is specified by the
// symbolic constants defined below.

typedef struct token {          // Struct for each token in linked list
  char *text;                   //   String containing token (if SIMPLE)
  int type;                     //   Corresponding type
  struct token *next;           //   Pointer to next token in linked list
} token;


// Break the string LINE into a headless linked list of typed tokens and
// return a pointer to the first token (or NULL if none were found or an
// error was detected).

token *tokenize (char *line);


// Print out the token list
void dumpList (token *list);


// Free list of tokens LIST
void freeList (token *list);


/////////////////////////////////////////////////////////////////////////////

// Token types used by tokenize() and parse()

enum {

   // Token types used by tokenize() et al.

      SIMPLE,           // Maximal contiguous sequence ... (as above)

      RED_IN,           // <

      RED_OUT,          // >
      RED_OUT_APP,      // >>

      RED_PIPE,         // |

      SEP_END,          // ;
      SEP_BG,           // &
      SEP_AND,          // &&
      SEP_OR,           // ||

      PAR_LEFT,         // (
      PAR_RIGHT,        // )

   // Token types used by parse() et al.

      NONE,             // Nontoken: Did not find a token
      ERROR,            // Nontoken: Encountered an error
      PIPE,             // Nontoken: CMD struct for pipeline
      SUBCMD            // Nontoken: CMD struct for subcommand
};


/////////////////////////////////////////////////////////////////////////////

// The syntax for a command is
//
//   <stage>    = <simple> / (<command>)
//   <pipeline> = <stage> / <pipeline> | <stage>
//   <and-or>   = <pipeline> / <and-or> && <pipeline> / <and-or> || <pipeline>
//   <sequence> = <and-or> / <sequence> ; <and-or> / <sequence> & <and-or>
//   <command>  = <sequence> / <sequence> ; / <sequence> &
//
// where a <simple> is a single command with local variables, arguments, and
// I/O redirection but no |, &, ;, &&, ||, (, or ).
//
// A command is represented by a tree of CMD structs corresponding to its
// simple commands and the "operators" PIPE, && (SEP_AND), || (SEP_OR),
// ; (SEP_END), & (SEP_BG), and SUBCMD.  The tree corresponds to how the
// command is parsed by a bottom-up parser.
//
// The tree for a <simple> is a single struct of type SIMPLE that specifies its
// local variables (nLocal, locVar[], locVal[]) and arguments (argc, argv[]);
// and whether and where to redirect its standard input (fromType, fromFile),
// its standard output (toType, toFile), and its standard error (errType,
// errFile).  The left and right children are NULL.
//
// The tree for a <stage> is either the tree for a <simple> or a struct
// of type SUBCMD (which may have redirection) whose left child is the tree
// representing the <command> and whose right child is NULL.
//
// The tree for a <pipeline> is either the tree for a <stage> or a struct
// of type PIPE whose left child is the tree representing the <pipeline> and
// whose right child is the tree representing the <stage>.
//
// The tree for an <and-or> is either the tree for a <pipeline> or a struct
// of type && (= SEP_AND) or || (= SEP_OR) whose left child is the tree
// representing the <and-or> and whose right child is the tree representing
// the <pipeline>.
//
// The tree for a <sequence> is either the tree for an <and-or> or a struct of
// type ; (= SEP_END) or & (= SEP_BG) whose left child is the tree representing
// the <sequence> and whose right child is the tree representing the <and-or>.
//
// The tree for a <command> is either the tree for a <sequence> or a struct of
// type ; (= SEP_END) or & (= SEP_BG) whose left child is the tree representing
// the <sequence> and whose right child is NULL.
//
// While the grammar above captures the syntax of bash commands, it does not
// reflect the semantics of &, which specify that only the preceding <and/or>
// should be executed in the background, not the entire preceding <sequence>.
//
//
// Examples (where A, B, C, D, and E are <simple>):                          //
//                                                                           //
//                              Expression Tree                              //
//                                                                           //
//   A                          A                                            //
//                                                                           //
//   < a A | B | C | D > d                     PIPE                          //
//                                            /    \                         //
//                                        PIPE      D >d                     //
//                                       /    \                              //
//                                   PIPE      C                             //
//                                  /    \                                   //
//                              <a A      B                                  //
//                                                                           //
//   A && B || C && D                   &&                                   //
//                                     /  \                                  //
//                                   ||    D                                 //
//                                  /  \                                     //
//                                &&    C                                    //
//                               /  \                                        //
//                              A    B                                       //
//                                                                           //
//   A ; B & C ; D || E ;                 ;                                  //
//                                      /                                    //
//                                     ;                                     //
//                                   /   \                                   //
//                                  &     ||                                 //
//                                 / \   /  \                                //
//                                ;   C D    E                               //
//                               / \                                         //
//                              A   B                                        //
//                                                                           //
//   (A ; B &) | (C || D) && E                 &&                            //
//                                            /  \                           //
//                                        PIPE    E                          //
//                                       /    \                              //
//                                    SUB      SUB                           //
//                                   /        /                              //
//                                  &       ||                               //
//                                 /       /  \                              //
//                                ;       C    D                             //
//                               / \                                         //
//                              A   B                                        //
//                                                                           //

typedef struct cmd {
  int type;             // Node type (SIMPLE, PIPE, SEP_AND, SEP_OR,
			//   SEP_END, SEP_BG, SUBCMD, or NONE)

  int nLocal;           // Number of local variable assignments
  char **locVar;        // Array of local variable names and the values to
  char **locVal;        //   assign to them when the command executes

  int argc;             // Number of command-line arguments
  char **argv;          // Null-terminated argument vector

  int fromType;         // Redirect stdin?
			//  (NONE (default), RED_IN)
  char *fromFile;       // File to redirect stdin, contents of here
			//   document, or NULL (default)

  int toType;           // Redirect stdout?
			//  (NONE (default), RED_OUT, RED_OUT_APP)
  char *toFile;         // File to redirect stdout or NULL (default)

  struct cmd *left;     // Left subtree or NULL (default)
  struct cmd *right;    // Right subtree or NULL (default)
} CMD;

									      
// Allocate, initialize, and return a pointer to an empty command structure
CMD *mallocCMD (void);


// Print out the command data structure CMD
void dumpCMD (CMD *exec, int level);


// Free the command structure CMD
void freeCMD (CMD *cmd);


// Print tree of CMD structs in in-order starting at LEVEL
void dumpTree (CMD *exec, int level);


// Parse a token list into a command structure and return a pointer to
// that structure (NULL if errors found).
CMD *parse (token *tok);

#endif
