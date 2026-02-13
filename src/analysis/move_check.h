#ifndef MOVE_CHECK_H
#define MOVE_CHECK_H

#include "../parser/parser.h"
#include "typecheck.h"

/**
 * @brief Determines if a type is safe to copy (implements Copy or is a primitive).
 *
 * @param ctx Parser context for type lookups.
 * @param t The type to check.
 * @return 1 if the type is Copy, 0 if it is Move.
 */
int is_type_copy(ParserContext *ctx, Type *t);

/**
 * @brief Checks if a symbol uses is valid (not moved).
 *
 * Reports an error if the symbol has been moved.
 *
 * @param tc Type checker context for error reporting.
 * @param node The AST node where the use occurs (for location).
 * @param sym The symbol being used.
 */
void check_use_validity(TypeChecker *tc, ASTNode *node, ZenSymbol *sym);

/**
 * @brief Marks a symbol as moved if its type is not Copy.
 *
 * @param ctx Parser context (for checking Copy trait).
 * @param sym The symbol to mark.
 * @param context_node The AST node causing the move (assignment, call, etc.).
 */
void mark_symbol_moved(ParserContext *ctx, ZenSymbol *sym, ASTNode *context_node);

/**
 * @brief Marks a symbol as valid (initialized or re-assigned).
 *
 * @param sym The symbol to mark.
 */
void mark_symbol_valid(ZenSymbol *sym);

#endif // MOVE_CHECK_H
