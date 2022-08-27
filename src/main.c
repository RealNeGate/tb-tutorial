#include "tb.h"

static void foo(void) {
    printf("Hello, World!\n");
}

int main(void) {
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
    return 0;
}
