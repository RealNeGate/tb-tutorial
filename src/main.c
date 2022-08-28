#include "tb.h"
#include "ast.h"
#include "parser.h"
#include <stdio.h>

void* ParseAlloc(void* (*mallocProc)(size_t));
void Parse(void* parser, int kind, double value, Context* ctx);
void ParseFree(void* parser, void (*freeProc)(size_t));

static void foo(void) {
    printf("Hello, World!\n");
}

////////////////////////////////
// Lexer
////////////////////////////////
enum { LEXER_FINAL = 16, LEXER_STATE_COUNT = 32 };
typedef struct {
    const char* str;
} LexerPattern;

typedef struct {
    uint64_t final_bitset;
    uint8_t dfa[128][LEXER_STATE_COUNT];
} LexerRecipe;

static bool lex__isspace(char ch) {
    return ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t';
}

static LexerRecipe lex__register(int pattern_count, const char* patterns[]) {
    LexerRecipe recipe = { 0 };

    int used = 1;
    recipe.final_bitset |= 1;

    for (int i = 0; i < pattern_count; i++) {
        const char* str = patterns[i];
        int last = 0;

        while (*str == '[') {
            str += 1;

            int old = used++;

            // decide if the next state is itself
            char end_rule = 0;
            {
                const char* s = str;
                while (*s && *s != ']') s += 1;

                s += 1; // skip past the ]
                if (*s == '+' || *s == '*') end_rule = *s;
            }

            int new = (end_rule == '*' ? old : used++);
            while (*str && *str != ']') {
                char min = *str++;
                if (*str == '-') {
                    str += 1;
                    char max = *str++;

                    // write out range in the DFA in some empty space
                    printf("dfa[%c - %c][%d] = %d\n", min, max, old, new);
                    for (int i = min; i < max; i++) {
                        recipe.dfa[i][old] = new;
                    }
                } else {
                    // single char
                    printf("dfa[%c][%d] = %d\n", min, old, new);
                    recipe.dfa[min][old] = new;
                }
            }

            if (end_rule == '+') {
                // copy the old state into a new one except this time it's self-referencing
                for (int i = 0; i < 128; i++) {
                    if (recipe.dfa[i][old] == new) {
                        printf("  dfa[%c][%d] = %d\n", i, old, new);
                        recipe.dfa[i][new] = new;
                    }
                }
            }

            if (*str != ']') {
                fprintf(stderr, "error: expected closing bracket for group: %s\n", str);
                abort();
            }

            last = new;

            str += 1;
            str += (*str == '+' || *str == '*');
        }

        if (last == 0) {
            fprintf(stderr, "error: no states made in pattern: %s\n", patterns[i]);
            abort();
        }

        // mark the last state as final
        recipe.final_bitset |= (1u << last);
    }

    return recipe;
}

static void lex__test(LexerRecipe* restrict recipe, const char* str) {
    for (;;) {
        // Skip any whitespace and comments
        // branchless space skip
        str += (*str == ' ');
        if (*str == '\0') {
            // quit, we're done
            return;
        }

        const char* token_start = str;
        ptrdiff_t state = 0;
        for (;;) {
            uint64_t next = recipe->dfa[*str][state];
            if (recipe->final_bitset & (1u << next)) break;

            state = next;
            str += 1;
        }

        printf("'%.*s' %llu\n", (int)(str - token_start), token_start, state);
    }
}

////////////////////////////////
// Parser
////////////////////////////////
static void ast__parse(Context* ctx, const char* str) {
    #if 0
    // Run parser
    void* parser = ParseAlloc(malloc);

    for (;;) {
        // skip garbage
        while (lex__isspace(*str)) str++;

        // EOF
        if (*str == 0) break;

        // Run DFA
        Parse(parser, hTokenId, sToken, ctx);
    }

    Parse(parser, 0, sToken);
    ParseFree(parser, free);
    #endif
}

int main(void) {
    // Initialize lexer
    static const char* RULES[] = {
        "[A-Za-z_][A-Za-z0-9_]*",
        "[0-9]+",
    };
    LexerRecipe recipe = lex__register(2, RULES);
    lex__test(&recipe, "hello = world(a, b);");

    #if 0
    TB_FeatureSet features = { 0 };
    TB_Module* mod = tb_module_create_for_host(TB_DEBUGFMT_NONE, &features);

    // declare the function prototype
    TB_FunctionPrototype* proto = tb_prototype_create(mod, TB_CDECL, TB_TYPE_VOID, 1, false);
    tb_prototype_add_param(proto, TB_TYPE_PTR);

    // fill up the IR
    TB_Function* func = tb_function_create(mod, "entry", TB_LINKAGE_PUBLIC);
    tb_function_set_prototype(func, proto);
    tb_inst_vcall(func, TB_TYPE_VOID, tb_inst_param(func, 0), 0, NULL);
    tb_inst_ret(func, TB_NULL_REG);

    // finish up
    tb_module_compile_function(mod, func, TB_ISEL_FAST);
    tb_module_export_jit(mod, TB_ISEL_FAST);


    // run crap
    void(*jit)(void*) = tb_function_get_jit_pos(func);
    jit(foo);

    tb_module_destroy(mod);
    #endif
    return 0;
}
