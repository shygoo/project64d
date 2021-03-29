#include <stdafx.h>
#include "../ScriptAPI.h"

void ScriptAPI::Define_Number_prototype_hex(duk_context *ctx)
{
    duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1, "Number");
    duk_get_prop_string(ctx, -1, "prototype");
    duk_push_c_function(ctx, js_Number_prototype_hex, 1);
    duk_put_prop_string(ctx, -2, "hex");
    duk_pop_n(ctx, 3);
}

duk_ret_t ScriptAPI::js_Number_prototype_hex(duk_context *ctx)
{
    duk_uint_t value;
    duk_uint_t length = 8;
    char hexString[64];

    if(!duk_is_undefined(ctx, -1))
    {
        length = duk_to_uint(ctx, -1);
        duk_pop(ctx);
    }

    duk_push_this(ctx);
    value = duk_to_uint(ctx, -1);
    duk_pop(ctx);

    snprintf(hexString, sizeof(hexString), "%0*X", length, value);

    duk_push_string(ctx, hexString);
    return 1;
}
