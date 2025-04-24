#ifndef ABC_TYPECHECKER_H
#define ABC_TYPECHECKER_H

#include <stdbool.h>

#include "abc_parser.h"

bool abc_typechecker_typecheck(struct abc_program *program);

#endif //ABC_TYPECHECKER_H
