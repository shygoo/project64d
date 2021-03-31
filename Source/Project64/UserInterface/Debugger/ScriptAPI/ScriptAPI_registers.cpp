#include "stdafx.h"
#include "../ScriptAPI.h"
#include <Project64-core/N64System/Mips/RegisterClass.h>

#pragma warning(disable: 4702)

static duk_ret_t ThrowRegInvalidError(duk_context* ctx);
static duk_ret_t ThrowRegContextUnavailableError(duk_context* ctx);
static duk_ret_t ThrowRegAssignmentTypeError(duk_context* ctx);

static int GPRIndex(const char *regName);
static duk_ret_t GPRGetImpl(duk_context* ctx, bool bUpper);
static duk_ret_t GPRSetImpl(duk_context* ctx, bool bUpper);

static int FPRIndex(const char *regName);
static duk_ret_t FPRGetImpl(duk_context* ctx, bool bDouble);
static duk_ret_t FPRSetImpl(duk_context* ctx, bool bDouble);

static uint32_t* COP0RegPtr(const char *regName);

void ScriptAPI::Define_registers(duk_context* ctx)
{
    #define REG_PROXY_FUNCTIONS(getter, setter) { \
        { "get", getter, 2 }, \
        { "set", setter, 3 }, \
        { NULL, NULL, 0 } \
    }

    const struct {
        const char *key;
        const duk_function_list_entry functions[3];
    } proxies[] = {
        { "gpr", REG_PROXY_FUNCTIONS(js_gpr_get, js_gpr_set) },
        { "ugpr", REG_PROXY_FUNCTIONS(js_ugpr_get, js_ugpr_set) },
        { "fpr", REG_PROXY_FUNCTIONS(js_fpr_get, js_fpr_set) },
        { "dfpr", REG_PROXY_FUNCTIONS(js_dfpr_get, js_dfpr_set) },
        { "cop0", REG_PROXY_FUNCTIONS(js_cop0_get, js_cop0_set) },
        { NULL, NULL }
    };

    duk_push_global_object(ctx);

    for (size_t i = 0; proxies[i].key != NULL; i++)
    {
        duk_push_string(ctx, proxies[i].key);
        duk_push_object(ctx);
        duk_push_object(ctx);
        duk_put_function_list(ctx, -1, proxies[i].functions);
        duk_push_proxy(ctx, 0);
        duk_freeze(ctx, -1);
        duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE);
    }

    duk_pop(ctx);
}

duk_ret_t ScriptAPI::js_gpr_get(duk_context* ctx)
{
    return GPRGetImpl(ctx, false);
}

duk_ret_t ScriptAPI::js_gpr_set(duk_context* ctx)
{
    return GPRSetImpl(ctx, false);
}

duk_ret_t ScriptAPI::js_ugpr_get(duk_context* ctx)
{
    return GPRGetImpl(ctx, true);
}

duk_ret_t ScriptAPI::js_ugpr_set(duk_context* ctx)
{
    return GPRSetImpl(ctx, true);
}

duk_ret_t ScriptAPI::js_fpr_get(duk_context* ctx)
{
    return FPRGetImpl(ctx, false);
}

duk_ret_t ScriptAPI::js_fpr_set(duk_context* ctx)
{
    return FPRSetImpl(ctx, false);
}

duk_ret_t ScriptAPI::js_dfpr_get(duk_context* ctx)
{
    return FPRGetImpl(ctx, true);
}

duk_ret_t ScriptAPI::js_dfpr_set(duk_context* ctx)
{
    return FPRSetImpl(ctx, true);
}

duk_ret_t ScriptAPI::js_cop0_get(duk_context* ctx)
{
    if (g_Reg == NULL)
    {
        return ThrowRegContextUnavailableError(ctx);
    }

    if (!duk_is_string(ctx, 1))
    {
        return ThrowRegInvalidError(ctx);
    }

    const char* name = duk_get_string(ctx, 1);
    uint32_t* reg = COP0RegPtr(name);

    // TODO: Remove this if cause/fakecause ever get consolidated
    if (strcmp(name, "cause") == 0)
    {
        duk_push_uint(ctx, g_Reg->FAKE_CAUSE_REGISTER | g_Reg->CAUSE_REGISTER);
        uint32_t value = duk_get_uint(ctx, 2);
        g_Reg->FAKE_CAUSE_REGISTER = value;
        g_Reg->CAUSE_REGISTER = value;
    }

    if (strcmp(name, "cause") == 0)
    {
        g_Reg->CheckInterrupts();
    }

    if (reg == NULL)
    {
        return ThrowRegInvalidError(ctx);
    }

    duk_push_uint(ctx, *reg);
    return 1;
}

duk_ret_t ScriptAPI::js_cop0_set(duk_context* ctx)
{
    if (g_Reg == NULL)
    {
        return ThrowRegContextUnavailableError(ctx);
    }

    if (!duk_is_string(ctx, 1))
    {
        return ThrowRegInvalidError(ctx);
    }

    const char* name = duk_get_string(ctx, 1);
    uint32_t* reg = COP0RegPtr(name);

    if (!duk_is_number(ctx, 2))
    {
        return ThrowRegAssignmentTypeError(ctx);
    }

    // TODO: Remove this if cause/fakecause ever get consolidated
    if (strcmp(name, "cause") == 0)
    {
        uint32_t value = duk_get_uint(ctx, 2);
        g_Reg->FAKE_CAUSE_REGISTER = value;
        g_Reg->CAUSE_REGISTER = value;
        return 0;
    }

    if (reg == NULL)
    {
        return ThrowRegInvalidError(ctx);
    }

    *reg = duk_get_uint(ctx, 2);
    return 0;
}

static duk_ret_t ThrowRegInvalidError(duk_context* ctx)
{
    duk_push_error_object(ctx, DUK_ERR_ERROR, "invalid register name or number");
    return duk_throw(ctx);
}

static duk_ret_t ThrowRegContextUnavailableError(duk_context* ctx)
{
    duk_push_error_object(ctx, DUK_ERR_ERROR, "CPU register context is unavailable");
    return duk_throw(ctx);
}

static duk_ret_t ThrowRegAssignmentTypeError(duk_context* ctx)
{
    duk_push_error_object(ctx, DUK_ERR_TYPE_ERROR, "invalid register value assignment");
    return duk_throw(ctx);
}

static int GPRIndex(const char *regName)
{
    const char* names[] = {
        "r0", "at", "v0", "v1", "a0", "a1", "a2", "a3",
        "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
        "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
        "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
    };

    for (int i = 0; i < 32; i++)
    {
        if (strcmp(names[i], regName) == 0)
        {
            return i;
        }
    }

    return -1;
}

static duk_ret_t GPRGetImpl(duk_context* ctx, bool bUpper)
{
    int regIndex = -1;

    if (g_Reg == NULL)
    {
        return ThrowRegContextUnavailableError(ctx);
    }

    if (duk_is_number(ctx, 1))
    {
        regIndex = duk_get_int(ctx, 1);
    }
    else if (duk_is_string(ctx, 1))
    {
        const char* key = duk_get_string(ctx, 1);

        if (strcmp(key, "hi") == 0)
        {
            duk_push_uint(ctx, g_Reg->m_HI.UW[bUpper ? 1 : 0]);
            return 1;
        }

        if (strcmp(key, "lo") == 0)
        {
            duk_push_uint(ctx, g_Reg->m_LO.UW[bUpper ? 1 : 0]);
            return 1;
        }

        regIndex = GPRIndex(key);
    }

    if (regIndex < 0 || regIndex > 31)
    {
        return ThrowRegInvalidError(ctx);
    }
    
    duk_push_uint(ctx, g_Reg->m_GPR[regIndex].UW[bUpper ? 1: 0]);
    return 1;
}

static duk_ret_t GPRSetImpl(duk_context* ctx, bool bUpper)
{
    int regIndex = -1;

    if (g_Reg == NULL)
    {
        return ThrowRegContextUnavailableError(ctx);
    }

    if (!duk_is_number(ctx, 2))
    {
        return ThrowRegAssignmentTypeError(ctx);
    }

    uint32_t value = duk_get_uint(ctx, 2);

    if (duk_is_number(ctx, 1))
    {
        regIndex = duk_get_int(ctx, 1);
    }
    else if (duk_is_string(ctx, 1))
    {
        const char* key = duk_get_string(ctx, 1);

        if (strcmp("hi", key) == 0)
        {
            g_Reg->m_HI.UW[bUpper ? 1 : 0] = value;
            return 0;
        }

        if (strcmp("lo", key) == 0)
        {
            g_Reg->m_LO.UW[bUpper ? 1 : 0] = value;
            return 0;
        }

        regIndex = GPRIndex(key);
    }

    if (regIndex == 0)
    {
        return 0;
    }

    if (regIndex < 0 || regIndex > 31)
    {
        return ThrowRegInvalidError(ctx);
    }

    g_Reg->m_GPR[regIndex].UW[bUpper ? 1 : 0] = value;
    return 0;
}

static int FPRIndex(const char *regName)
{
    const char* names[32] = {
        "f0", "f1", "f2", "f3", "f4","f5", "f6", "f7", "f8",
        "f9", "f10", "f11", "f12", "f13", "f14", "f15", "f16",
        "f17", "f18", "f19", "f20", "f21", "f22", "f23", "f24",
        "f25", "f26", "f27", "f28", "f29", "f30", "f31"
    };

    for (int i = 0; i < 32; i++)
    {
        if (strcmp(names[i], regName) == 0)
        {
            return i;
        }
    }

    return -1;
}

static duk_ret_t FPRGetImpl(duk_context* ctx, bool bDouble)
{
    int regIndex = -1;

    if (g_Reg == NULL)
    {
        return ThrowRegContextUnavailableError(ctx);
    }

    if (duk_is_number(ctx, 1))
    {
        regIndex = duk_get_int(ctx, 1);
    }
    else if (duk_is_string(ctx, 1))
    {
        const char* key = duk_get_string(ctx, 1);
        // todo status reg
        regIndex = FPRIndex(key);
    }

    if (regIndex < 0 || regIndex > 31)
    {
        return ThrowRegInvalidError(ctx);
    }

    if (bDouble)
    {
        duk_push_number(ctx, (duk_double_t)*g_Reg->m_FPR_S[regIndex]);
    }
    else
    {
        duk_push_number(ctx, (duk_double_t)*g_Reg->m_FPR_D[regIndex & 0x1E]);
    }

    return 1;
}

static duk_ret_t FPRSetImpl(duk_context* ctx, bool bDouble)
{
    int regIndex = -1;

    if (g_Reg == NULL)
    {
        return ThrowRegContextUnavailableError(ctx);
    }

    if (!duk_is_number(ctx, 2))
    {
        return ThrowRegAssignmentTypeError(ctx);
    }

    if (duk_is_number(ctx, 1))
    {
        regIndex = duk_get_int(ctx, 1);
    }
    else if (duk_is_string(ctx, 1))
    {
        const char* key = duk_get_string(ctx, 1);
        // todo status reg
        regIndex = FPRIndex(key);
    }

    if (regIndex < 0 || regIndex > 31)
    {
        return ThrowRegInvalidError(ctx);
    }

    duk_double_t value = duk_get_number(ctx, 2);

    if (bDouble)
    {
        *g_Reg->m_FPR_S[regIndex] = (float)value;
    }
    else
    {
        *g_Reg->m_FPR_D[regIndex & 0x1E] = value;
    }

    return 0;
}

static uint32_t* COP0RegPtr(const char *regName)
{
    if (g_Reg == NULL)
    {
        return NULL;
    }

    struct {
        const char* name;
        uint32_t *ptr;
    } names[] = {
        { "index", &g_Reg->INDEX_REGISTER },
        { "random", &g_Reg->RANDOM_REGISTER },
        { "entrylo0", &g_Reg->ENTRYLO0_REGISTER },
        { "entrylo1", &g_Reg->ENTRYLO1_REGISTER },
        { "context", &g_Reg->CONTEXT_REGISTER },
        { "pagemask", &g_Reg->PAGE_MASK_REGISTER },
        { "wired", &g_Reg->WIRED_REGISTER },
        { "badvaddr", &g_Reg->BAD_VADDR_REGISTER },
        { "count", &g_Reg->COUNT_REGISTER },
        { "entryhi", &g_Reg->ENTRYHI_REGISTER },
        { "compare", &g_Reg->COMPARE_REGISTER },
        { "status", &g_Reg->STATUS_REGISTER },
        // TODO: Uncomment if cause/fakecause ever get consolidated
        //{ "cause", &g_Reg->CAUSE_REGISTER },
        { "epc", &g_Reg->EPC_REGISTER },
        { "config", &g_Reg->CONFIG_REGISTER },
        { "taglo", &g_Reg->TAGLO_REGISTER },
        { "taghi", &g_Reg->TAGHI_REGISTER },
        { "errorepc", &g_Reg->ERROREPC_REGISTER },
        { NULL, NULL }
    };
    
    for (int i = 0; names[i].name != NULL; i++)
    {
        if (strcmp(regName, names[i].name) == 0)
        {
            return names[i].ptr;
        }
    }

    return NULL;
}
