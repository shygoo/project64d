#include <stdafx.h>
#include "../ScriptAPI.h"

void ScriptAPI::Define_script(duk_context *ctx)
{
    const duk_function_list_entry funcs[] = {
        { "timeout",   js_script_timeout,   1 },
        { "keepalive", js_script_keepalive, 1 },
        { nullptr, nullptr, 0 }
    };

    duk_push_global_object(ctx);
    duk_push_string(ctx, "script");
    duk_push_object(ctx);
    duk_put_function_list(ctx, -1, funcs);
    duk_freeze(ctx, -1);
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE);
    duk_pop(ctx);
}

duk_ret_t ScriptAPI::js_script_timeout(duk_context *ctx)
{
    CScriptInstance* inst = GetInstance(ctx);
    
    if(duk_get_top(ctx) != 1 || !duk_is_number(ctx, 0))
    {
        return ThrowInvalidArgsError(ctx);
    }

    inst->SetExecTimeout((uint64_t)duk_get_number(ctx, 0));
    return 0;
}

duk_ret_t ScriptAPI::js_script_keepalive(duk_context *ctx)
{
    CScriptInstance* inst = GetInstance(ctx);

    if(duk_get_top(ctx) != 1 || !duk_is_boolean(ctx, 0))
    {
        return ThrowInvalidArgsError(ctx);
    }

    duk_bool_t bKeepAlive = duk_get_boolean(ctx, 0);

    duk_push_global_object(ctx);
    duk_bool_t bHaveProp = duk_has_prop_string(ctx, -1, HSYM_KEEPALIVE);

    if(bKeepAlive && !bHaveProp)
    {
        duk_push_boolean(ctx, 1);
        duk_put_prop_string(ctx, -2, HSYM_KEEPALIVE);
        inst->IncRefCount();
    }
    else if(!bKeepAlive && bHaveProp)
    {
        duk_del_prop_string(ctx, -1, HSYM_KEEPALIVE);
        inst->DecRefCount();
    }

    duk_pop(ctx);

    return 0;
}