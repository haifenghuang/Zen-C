#include "move_check.h"
#include "../diagnostics/diagnostics.h"
#include <stdio.h>
#include <string.h>

int is_type_copy(ParserContext *ctx, Type *t)
{
    if (!t)
    {
        return 1; // Default to Copy for unknown types to avoid annoyance
    }

    switch (t->kind)
    {
    case TYPE_INT:
    case TYPE_I8:
    case TYPE_I16:
    case TYPE_I32:
    case TYPE_I64:
    case TYPE_U8:
    case TYPE_U16:
    case TYPE_U32:
    case TYPE_U64:
    case TYPE_F32:
    case TYPE_F64:
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_VOID:
    case TYPE_POINTER: // Pointers are Copy
    case TYPE_FUNCTION:
    case TYPE_ENUM: // Enums are integers
    case TYPE_BITINT:
    case TYPE_UBITINT:
        return 1;

    case TYPE_STRUCT:
        // Structs are MOVE by default unless they implement Copy
        if (check_impl(ctx, "Copy", t->name))
        {
            return 1;
        }

        // If the struct is NOT defined (opaque/C type) and does NOT implement Drop,
        // treat it as Copy (C behavior).
        if (!find_struct_def(ctx, t->name) && !check_impl(ctx, "Drop", t->name))
        {
            return 1;
        }

        return 0;

    case TYPE_ARRAY:
        // Fixed-size arrays of Copy types are themselves Copy
        // This allows reusing stack buffers like char[32] without move errors
        return is_type_copy(ctx, t->inner);

    case TYPE_ALIAS:
        if (t->alias.is_opaque_alias)
        {
            return 1;
        }
        return is_type_copy(ctx, t->inner);

    default:
        return 1;
    }
}

static void tc_error_with_hints(TypeChecker *tc, Token t, const char *msg, const char *const *hints)
{
    if (tc)
    {
        zerror_with_hints(t, msg, hints);
        tc->error_count++;
    }
    else
    {
        // Parser mode: fail immediately to emulate original zpanic behavior
        zpanic_with_hints(t, msg, hints);
    }
}

void check_use_validity(TypeChecker *tc, ASTNode *var_node, ZenSymbol *sym)
{
    if (!sym || !var_node)
    {
        return;
    }

    if (sym->is_moved)
    {
        char msg[256];
        snprintf(msg, 255, "Use of moved value '%s'", sym->name);

        const char *hints[] = {"This type owns resources and cannot be implicitly copied",
                               "Consider using a reference ('&') to borrow the value instead",
                               NULL};
        tc_error_with_hints(tc, var_node->token, msg, hints);
    }
}

void mark_symbol_moved(ParserContext *ctx, ZenSymbol *sym, ASTNode *context_node)
{
    (void)context_node;
    if (!sym)
    {
        return;
    }

    // Only move if type is NOT Copy
    Type *t = sym->type_info;

    int copy = is_type_copy(ctx, t);
    if (t && ctx && !copy)
    {
        sym->is_moved = 1;
    }
}

void mark_symbol_valid(ZenSymbol *sym)
{
    if (sym)
    {
        sym->is_moved = 0;
    }
}
