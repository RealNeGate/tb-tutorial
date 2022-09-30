#include <tb.h>
#include <stdio.h>

#define NL_LEXER_IMPL
#include "lexer.h"

static void foo(int param) {
    printf("Hello, World! %d\n", param);
}

#define EXPR(name, kid) \
static void parse_expr_ ## name (NL_Lexer* restrict l) { \
    while () { \
    } \
}

#define CTABLE_LEN(a) (sizeof(a) / sizeof(a[0]))
#define CTABLE_GET(a, i) (i < CTABLE_LEN(a) ? a[i] : 0)

static int preds[] = {
    ['*'] = 10,
    ['/'] = 10,

    ['+'] = 9,
    ['-'] = 9,
};

static TB_Reg unary(TB_Function* func, NL_Lexer* restrict l) {
    switch (l->token_type) {
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

static TB_Reg parse(TB_Function* func, const char* text) {
    NL_Lexer l = { text };

    NL_READ_TOKEN(&l);
    return binop(func, &l, 0);
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

        TB_Reg result = parse(func, "16 + 4");
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
