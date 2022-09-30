// This is supposed to be a simple lexer collection
//
// Namespaces:
//   nl_*
//   NL_*
//
// if you wish to use this library without collision don't use these
// identifiers
//
// Do this:
//   #define NL_LEXER_IMPL
// before you include this file into one C file
//
// Settings:
//   #define NL_LEXER_FORTH_PILLED this will make the nl_read_token map to the forth based
//   variant
//
//   Normally it's a C-like lexer
//
#ifndef NL_LEXER_H
#define NL_LEXER_H

#include <stdbool.h>
#include <stdio.h>

#ifdef NL_LEXER_STATIC
#define NL_API static
#else
#define NL_API extern
#endif

typedef enum {
    NL_TOKEN_NONE = 0,

    NL_TOKEN_PLUS  = '+',
    NL_TOKEN_MINUS = '-',
    NL_TOKEN_STAR  = '*',
    NL_TOKEN_SLASH = '/',

    // single character tokens are
    // mapped directly to the ASCII

    // special lexer constructs are
    // mapped to random ids after 128
    NL_TOKEN_IDENT = 128,
    NL_TOKEN_NUMBER,

    // this is used by the forth compiler
    // to represent any token that's not
    // a number
    NL_TOKEN_WORD
} NL_TokenType;

typedef struct {
    const char* current;

    NL_TokenType token_type;
    const char* token_start;
    const char* token_end;
} NL_Lexer;

// this is used if you wanna match a token against a constant string
#define NL_MATCH_STRING(l, str)                      \
((l->token_end - l->token_start) == sizeof(str)-1 && \
    memcmp(l->token_start, str, sizeof(str)-1) == 0) \

#define NL_MATCH_TYPE(l, type) ((l)->token_type == (type))

// it's kinda fucking wild that void ternary is even a thing
// this just eats a token if it can't match it'll throw an error
#define NL_EAT_STRING(l, str) \
(NL_MATCH_STRING(l, str) ? NL_READ_TOKEN(l) : (void) (printf("Expected '%s'\n", str), abort()))

#define NL_EAT(l, type) \
(l->token_type == type ? NL_READ_TOKEN(l) : (void) (printf("Expected '%c'\n", type), abort()))

// eats a token if it matches, it'll consume the token and return true
#define NL_TRY_EAT_STRING(l, str) \
(NL_MATCH_STRING(l, str) ? (NL_READ_TOKEN(l), true) : false)

#define NL_TRY_EAT(l, type) \
(l->token_type == type ? (NL_READ_TOKEN(l), true) : false)

#if defined(NL_LEXER_LISP_PILLED)
#define NL_READ_TOKEN nl_read_lisp_token
#elif defined(NL_LEXER_FORTH_PILLED)
#define NL_READ_TOKEN nl_read_forth_token
#else
#define NL_READ_TOKEN nl_read_c_token
#endif

// freed with free(ptr)
inline static char* nl_token_as_cstr(NL_Lexer* restrict l) {
    size_t len = l->token_end - l->token_start;
    char* str = malloc(len + 1);

    memcpy(str, l->token_start, len);
    str[len] = 0;

    return str;
}

NL_API void nl_read_lisp_token(NL_Lexer* restrict l);
NL_API void nl_read_forth_token(NL_Lexer* restrict l);
NL_API void nl_read_c_token(NL_Lexer* restrict l);
NL_API unsigned long long nl_parse_int(NL_Lexer* restrict l);

#endif /* NL_LEXER_H */

#ifdef NL_LEXER_IMPL

#define nl__set_token(type_, start_, end_) do { \
    l->token_type  = (type_);               \
    l->token_start = (start_);              \
    l->token_end   = (end_);                \
} while(0)

static bool nl__isspace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static bool nl__isident(char ch) {
    return (ch >= 'a' && ch <= 'z') ||
    (ch >= 'A' && ch <= 'Z') ||
    (ch >= '0' && ch <= '9') ||
        ch == '_';
}

NL_API void nl_read_c_token(NL_Lexer* restrict l) {
    l->current += (*l->current == ' ');

    // skip any garbage
    while (nl__isspace(*l->current)) l->current++;

    const char* start = l->current++;
    char peek = *start;

    if (peek == '\0') {
        l->current--;
        nl__set_token(NL_TOKEN_NONE, start, start);
    } else if ((peek >= 'a' && peek <= 'z') || (peek >= 'A' && peek <= 'Z') || (peek == '_')) {
        while (nl__isident(*l->current)) {
            l->current++;
        }

        nl__set_token(NL_TOKEN_IDENT, start, l->current);
    } else if (peek >= '0' && peek <= '9') {
        while (*l->current >= '0' && *l->current <= '9') {
            l->current++;
        }

        nl__set_token(NL_TOKEN_NUMBER, start, l->current);
    } else if (peek > 32) {
        nl__set_token(peek, start, l->current);
    } else {
        printf("Unknown character: %x", (int)peek);
        abort();
    }
}

NL_API void nl_read_forth_token(NL_Lexer* restrict l) {
    l->current += (*l->current == ' ');

    // skip any garbage
    while (nl__isspace(*l->current)) l->current++;

    // extract a word
    const char* start = l->current++;

    if (*start == '\0') {
        l->current--; // revert
        nl__set_token(NL_TOKEN_NONE, start, start);
    } else if (*start >= '0' && *start <= '9') {
        while (*l->current >= '0' && *l->current <= '9') l->current++;
        nl__set_token(NL_TOKEN_NUMBER, start, l->current);
    } else {
        while (!nl__isspace(*l->current)) l->current++;

        int type = NL_TOKEN_WORD;
        if ((l->current - start) == 1) {
            type = *start;
        }

        nl__set_token(type, start, l->current);
    }
}

NL_API void nl_read_lisp_token(NL_Lexer* restrict l) {
    l->current += (*l->current == ' ');

    // skip any garbage
    while (nl__isspace(*l->current)) l->current++;

    // extract a word
    const char* start = l->current++;

    if (*start == '\0') {
        l->current--; // revert
        nl__set_token(NL_TOKEN_NONE, start, start);
    } else if (*start == '(' || *start == ')') {
        nl__set_token(*start, start, l->current);
    } else if (*start >= '0' && *start <= '9') {
        while (*l->current >= '0' && *l->current <= '9') l->current++;
        nl__set_token(NL_TOKEN_NUMBER, start, l->current);
    } else {
        for (; *l->current; l->current++) {
            if (nl__isspace(*l->current)) break;
            if (*l->current >= '0' && *l->current <= '9') break;
            if (*l->current == '(' || *l->current == ')') break;
        }

        int type = NL_TOKEN_WORD;
        if ((l->current - start) == 1) {
            type = *start;
        }

        nl__set_token(type, start, l->current);
    }
}

NL_API unsigned long long nl_parse_int(NL_Lexer* restrict l) {
    unsigned long long value = 0;

    for (const char* str = l->token_start; str != l->token_end; str++) {
        value *= 10;
        value += *str - '0';
    }

    return value;
}

#undef nl__set_token
#endif /* NL_LEXER_IMPL */
