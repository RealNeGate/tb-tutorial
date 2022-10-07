#include <tb.h>
#include <stdio.h>

#define NL_LEXER_IMPL
#include "lexer.h"

////////////////////////////////
// Runtime support
////////////////////////////////
static void foo(int param) {
    printf("Hello, World! %d\n", param);
}

////////////////////////////////
// Parser
////////////////////////////////
typedef struct Type Type;
typedef struct Param Param;

// it's just easier to do a linked list ok
struct Param {
    Param* next;
    char* name;

    // due to our type inference we don't specify the type of a parameter
    // on creation and instead just do it by matching against the first
    // invocation
    Type* type;
};

struct Type {
    enum {
        TYPE_HOLE, // unresolved at the moment
        TYPE_INT,  // 32bit integer
        TYPE_MAP,  // just a table
        TYPE_FUNC, // expression with runtime inputs
    } tag;
    union {
        struct {
            Type* key;
            Type* val;
        } map;
        struct {
            Type* ret;
            size_t param_count;
            Param* params;
        } func;
    };
};

typedef struct Sym {
    char* name;
    TB_Reg value;
} Sym;

enum { MAX_SYMBOLS = 1024 };
int sym_count;
Sym syms[MAX_SYMBOLS];

#define CTABLE_LEN(a) (sizeof(a) / sizeof(a[0]))
#define CTABLE_GET(a, i) (i < CTABLE_LEN(a) ? a[i] : 0)

static int preds[] = {
    ['*'] = 10, ['/'] = 10,
    ['+'] = 9,  ['-'] = 9,
};

static TB_Reg unary(TB_Function* func, NL_Lexer* restrict l) {
    switch (l->token_type) {
        case NL_TOKEN_IDENT: {
            size_t len = l->token_end - l->token_start;
            const char* str = l->token_start;
            NL_READ_TOKEN(l);

            for (size_t i = sym_count; i--;) {
                if (strncmp(str, syms[i].name, len) == 0) {
                    return tb_inst_load(func, TB_TYPE_I32, syms[i].value, 4);
                }
            }

            fprintf(stderr, "unknown variable '%.*s'\n", (int) len, str);
            abort();
        }
        case NL_TOKEN_NUMBER: {
            unsigned long long n = nl_parse_int(l);
            NL_READ_TOKEN(l);
            return tb_inst_sint(func, TB_TYPE_I32, n);
        }
        default: abort();
    }
}

static TB_Reg binop(TB_Function* func, NL_Lexer* restrict l, int min_prec) {
    TB_Reg lhs = unary(func, l);

    int prec;
    NL_TokenType binop_type;
    while (binop_type = l->token_type, prec = CTABLE_GET(preds, binop_type), prec != 0 && prec >= min_prec) {
        NL_READ_TOKEN(l);

        TB_Reg rhs = binop(func, l, prec + 1);
        printf("r%d %c r%d =>", lhs, (char) binop_type, rhs);
        switch (binop_type) {
            case '+': lhs = tb_inst_add(func, lhs, rhs, 0); break;
            case '-': lhs = tb_inst_sub(func, lhs, rhs, 0); break;
            case '*': lhs = tb_inst_mul(func, lhs, rhs, 0); break;
            case '/': lhs = tb_inst_div(func, lhs, rhs, true); break;
            default: abort();
        }
        printf(" r%d\n", lhs);
    }

    return lhs;
}

static TB_Reg expr(TB_Function* func, NL_Lexer* restrict l) {
    if (NL_TRY_EAT_STRING(l, "var")) {
        // 'var' ident '=' expr
        char* name = nl_token_as_cstr(l);
        NL_READ_TOKEN(l);

        if (!NL_TRY_EAT(l, '=')) {
            abort();
        }

        TB_Reg storage = tb_inst_local(func, 4, 4);
        TB_Reg initial = expr(func, l);
        tb_inst_store(func, TB_TYPE_I32, storage, initial, 4);

        assert(sym_count < MAX_SYMBOLS);
        syms[sym_count++] = (Sym){ .name = name, .value = storage };
        return initial;
    } else if (NL_TRY_EAT_STRING(l, "if")) {
        TB_Label true_lbl = tb_basic_block_create(func);
        TB_Label false_lbl = tb_basic_block_create(func);

        // 'if' expr 'then' ('else' expr)?
        TB_Reg cond = expr(func, l);
        tb_inst_if(func, cond, true_lbl, false_lbl);
        if (!NL_TRY_EAT_STRING(l, "then")) {
            abort();
        }

        tb_inst_set_label(func, true_lbl);
        TB_Reg hit = expr(func, l);
        TB_Label hit_lbl = tb_inst_get_label(func);

        if (NL_TRY_EAT_STRING(l, "else")) {
            TB_Label skip_lbl = tb_basic_block_create(func);
            tb_inst_goto(func, skip_lbl);
            tb_inst_set_label(func, false_lbl);

            TB_Reg miss = expr(func, l);
            TB_Label miss_lbl = tb_inst_get_label(func);

            tb_inst_goto(func, skip_lbl);
            tb_inst_set_label(func, skip_lbl);
            return tb_inst_phi2(func, hit_lbl, hit, miss_lbl, miss);
        } else {
            tb_inst_goto(func, false_lbl);
            tb_inst_set_label(func, false_lbl);
            return TB_NULL_REG;
        }
    } else if (l->token_type == '(') {
        NL_READ_TOKEN(l);

        TB_Reg tail = TB_NULL_REG;
        while (l->token_type != ')') {
            tail = expr(func, l);

            if (l->token_type == ',') {
                NL_READ_TOKEN(l);
            }
        }

        NL_READ_TOKEN(l);
        return tail;
    } else {
        return binop(func, l, 0);
    }
}

static TB_Reg parse(TB_Function* func, const char* text) {
    NL_Lexer l = { text };

    NL_READ_TOKEN(&l);
    return expr(func, &l);
}

int main(void) {
    TB_FeatureSet features = { 0 };
    TB_Module* mod = tb_module_create_for_host(&features, true);

    // declare the function prototype
    TB_FunctionPrototype* proto = tb_prototype_create(mod, TB_CDECL, TB_TYPE_VOID, 1, false);
    tb_prototype_add_param(proto, TB_TYPE_PTR);

    // fill up the IR
    TB_Function* func;
    {
        func = tb_function_create(mod, "entry", TB_LINKAGE_PUBLIC);
        tb_function_set_prototype(func, proto);

        TB_Reg result = parse(func, "(var apple = 16 + 4*4, if 16 + 16 then apple else 0)");
        tb_inst_vcall(func, TB_TYPE_VOID, tb_inst_param(func, 0), 1, &result);
        tb_inst_ret(func, TB_NULL_REG);

        tb_function_print(func, tb_default_print_callback, stdout, false);
    }

    // finish up
    tb_module_compile_function(mod, func, TB_ISEL_FAST);
    tb_module_export_jit(mod);

    // run crap
    void(*jit)(void*) = tb_function_get_jit_pos(func);
    jit(foo);

    tb_module_destroy(mod);
    return 0;
}
