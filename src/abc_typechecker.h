#ifndef ABC_TYPECHECKER_H
#define ABC_TYPECHECKER_H

#include <stdbool.h>

#include "data/abc_pool.h"
#include "data/abc_arr.h"
#include "abc_parser.h"

struct abc_typechecker;

void abc_typechecker_init(struct abc_typechecker *typechecker);
void abc_typechecker_destroy(struct abc_typechecker *typechecker);
bool abc_typechecker_typecheck(struct abc_typechecker *typechecker, struct abc_program *program);

#endif // ABC_TYPECHECKER_H
