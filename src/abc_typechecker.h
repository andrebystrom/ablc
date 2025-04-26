/**
 * basic typechecker.
 */

#ifndef ABC_TYPECHECKER_H
#define ABC_TYPECHECKER_H

#include <stdbool.h>

#include "abc_parser.h"
#include "data/abc_arr.h"
#include "data/abc_pool.h"

// Typecheck the parse tree, returning true on success.
bool abc_typechecker_typecheck(struct abc_program *program);

#endif // ABC_TYPECHECKER_H
