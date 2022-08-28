%include {
    #include "ast.h"
}

%token_prefix TOKEN_

// operator precedence
%left ADD SUB.
%left MUL DIV.

// data associated with nodes
%token_type { double }

// passed around the parse functions
%extra_argument { Context * context }
%parse_failure { context->error = true; }

// Grammar
formula ::= expr(A).               { context->result = A; }
expr(A) ::= expr(B) ADD expr(C).   { A = B + C; }
expr(A) ::= expr(B) SUB expr(C).   { A = B - C; }
expr(A) ::= expr(B) MUL expr(C).   { A = B * C; }
expr(A) ::= expr(B) DIV expr(C).   { A = B / C; }
expr(A) ::= LPAREN expr(B) RPAREN. { A = B; }
expr(A) ::= LITERAL(B).            { A = B; }
