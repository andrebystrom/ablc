#include "abc_typechecker.h"

#include <assert.h>
#include <string.h>

struct abc_typechecker {
    struct abc_pool *pool;
    enum abc_parser_type curr_fun_type;
    // TODO: these should be a map for better efficiency
    struct abc_arr types;
    struct abc_arr formals;
};

struct type {
    char *name;
    enum abc_type type;
    bool marker;
};

// function argument types
struct formals {
    char *name;
    struct abc_arr types;
    enum abc_type ret_type;
    bool marker;
};

struct typecheck_result {
    bool err;
    enum abc_type type;
};

void abc_typechecker_init(struct abc_typechecker *typechecker) {
    typechecker->pool = abc_pool_create();
    abc_arr_init(&typechecker->types, sizeof(struct type), typechecker->pool);
    abc_arr_init(&typechecker->formals, sizeof(struct formals), typechecker->pool);
}

void abc_typechecker_destroy(struct abc_typechecker *typechecker) { abc_pool_destroy(typechecker->pool); }

static void push_formals_scope(struct abc_typechecker *tc) {
    struct formals formals = {.marker = true};
    abc_arr_push(&tc->formals, &formals);
}

static void pop_formals_scope(struct abc_typechecker *tc) {
    for (size_t i = 0; i < tc->formals.len; i++) {
        struct formals formals = ((struct formals *) tc->formals.data)[tc->formals.len - (i + 1)];
        if (formals.marker) {
            tc->formals.len = tc->formals.len - (i + 1);
            break;
        }
    }
}

static bool push_formals(struct abc_typechecker *tc, struct formals *formals) {
    int found_depth = -1;
    int depth = 0;
    for (size_t i = 0; i < tc->formals.len; i++) {
        struct formals f = ((struct formals *) tc->formals.data)[i];
        if (f.marker) {
            depth++;
            continue;
        }
        if (strcmp(f.name, formals->name) == 0) {
            found_depth = (int) depth;
        }
    }
    if (found_depth == depth) {
        return false;
    }
    abc_arr_push(&tc->formals, formals);
    return true;
}

static bool lookup_formals(struct abc_typechecker *tc, const char *name, struct formals *formals) {
    bool found = false;
    for (size_t i = 0; i < tc->formals.len; i++) {
        struct formals f = ((struct formals *) tc->formals.data)[i];
        if (f.marker) {
            continue;
        }
        if (strcmp(f.name, name) == 0) {
            found = true;
            *formals = f;
        }
    }
    return found;
}

static void push_type_scope(struct abc_typechecker *tc) {
    struct type type = {.marker = true};
    abc_arr_push(&tc->types, &type);
}

static void pop_type_scope(struct abc_typechecker *tc) {
    for (size_t i = 0; i < tc->types.len; i++) {
        struct type type = ((struct type *) tc->types.data)[tc->types.len - (i + 1)];
        if (type.marker) {
            tc->types.len = tc->types.len - (i + 1);
            break;
        }
    }
}

static bool push_type(struct abc_typechecker *tc, struct type *type) {
    int depth = 0;
    int found_depth = -1;
    for (size_t i = 0; i < tc->types.len; i++) {
        struct type t = ((struct type *) tc->types.data)[i];
        if (t.marker) {
            depth++;
            continue;
        }
        if (strcmp(t.name, type->name) == 0) {
            found_depth = (int) depth;
        }
    }
    if (found_depth == depth) {
        return false;
    }
    abc_arr_push(&tc->types, type);
    return true;
}

static bool lookup_type(struct abc_typechecker *tc, const char *name, struct type *type) {
    bool found = false;
    for (size_t i = 0; i < tc->types.len; i++) {
        int offset = tc->types.len - (i + 1);
        struct type *t = (struct type *) tc->types.data + offset;
        if (t->marker) {
            continue;
        }
        if (strcmp(t->name, name) == 0) {
            found = true;
            *type = *t;
        }
    }

    return found;
}

static struct typecheck_result typecheck_fun(struct abc_typechecker *tc, struct abc_fun_decl *fun);
static struct typecheck_result typecheck_decl(struct abc_typechecker *tc, struct abc_decl *decl);
static struct typecheck_result typecheck_stmt(struct abc_typechecker *tc, struct abc_stmt *stmt);
static struct typecheck_result typecheck_block_stmt(struct abc_typechecker *tc, struct abc_block_stmt *block);
static struct typecheck_result typecheck_expr(struct abc_typechecker *tc, struct abc_expr *expr);

bool abc_typechecker_typecheck(struct abc_program *program) {
    bool ok = true;
    struct abc_typechecker tc;
    abc_typechecker_init(&tc);
    for (size_t i = 0; i < program->fun_decls.len; i++) {
        struct abc_fun_decl fun_decl = ((struct abc_fun_decl *) program->fun_decls.data)[i];
        if (typecheck_fun(&tc, &fun_decl).err) {
            ok = false;
        }
    }

    // check for valid main
    bool found = false;
    for (size_t i = 0; i < program->fun_decls.len; i++) {
        struct abc_fun_decl fun_decl = ((struct abc_fun_decl *) program->fun_decls.data)[i];
        if (strcmp(fun_decl.name.lexeme, "main") == 0) {
            found = true;
            if (fun_decl.type != PARSER_TYPE_VOID) {
                fprintf(stderr, "main function must be of type void\n");
                ok = false;
            }
            if (fun_decl.params.len != 0) {
                fprintf(stderr, "main function must have no parameters\n");
                ok = false;
            }
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "no main function defined\n");
        ok = false;
    }

    abc_typechecker_destroy(&tc);
    return ok;
}

static struct typecheck_result typecheck_fun(struct abc_typechecker *tc, struct abc_fun_decl *fun) {
    struct formals formals = {.marker = false, .name = fun->name.lexeme, .ret_type = (enum abc_type) fun->type};
    abc_arr_init(&formals.types, sizeof(struct type), tc->pool);
    for (size_t i = 0; i < fun->params.len; i++) {
        struct abc_param param = ((struct abc_param *) fun->params.data)[i];
        if (param.type == PARSER_TYPE_VOID) {
            fprintf(stderr, "void cannot be used as a function parameter\n");
            return (struct typecheck_result) {.err = true};
        }
        enum abc_type type = (enum abc_type) param.type;
        struct type t = {.marker = false, .name = param.token.lexeme, .type = type};
        abc_arr_push(&formals.types, &t);
        push_type(tc, &t);
    }
    if (!push_formals(tc, &formals)) {
        fprintf(stderr, "function %s redefined, skipping typecheck\n", fun->name.lexeme);
        return (struct typecheck_result) {.err = true};
    }

    tc->curr_fun_type = fun->type;
    struct typecheck_result result = typecheck_block_stmt(tc, &fun->body);
    return result;
}

static struct typecheck_result typecheck_decl(struct abc_typechecker *tc, struct abc_decl *decl) {
    struct type type = {0};
    switch (decl->tag) {
        case ABC_DECL_VAR:
            type.name = decl->val.var.name.lexeme;
            type.type = (enum abc_type) decl->val.var.type;
            if (type.type == ABC_TYPE_VOID) {
                fprintf(stderr, "cannot declare variable of type void\n");
                return (struct typecheck_result) {.err = true};
            }
            if (decl->val.var.has_init) {
                struct typecheck_result result = typecheck_expr(tc, decl->val.var.init);
                if (result.err || result.type != (enum abc_type) decl->val.var.type) {
                    if (!result.err) {
                        fprintf(stderr, "type mismatch for decl %s\n", decl->val.var.name.lexeme);
                    }
                    return (struct typecheck_result) {.err = true};
                }
            }
            if (!push_type(tc, &type)) {
                fprintf(stderr, "%s defined multiple times\n", decl->val.var.name.lexeme);
                return (struct typecheck_result) {.err = true};
            }
            return (struct typecheck_result) {.err = false};
        case ABC_DECL_STMT:
            return typecheck_stmt(tc, &decl->val.stmt.stmt);
    }
    assert(0);
}

static struct typecheck_result typecheck_stmt(struct abc_typechecker *tc, struct abc_stmt *stmt) {
    struct typecheck_result res;
    switch (stmt->tag) {
        case ABC_STMT_EXPR:
            return typecheck_expr(tc, stmt->val.expr_stmt.expr);
        case ABC_STMT_IF:
            res = typecheck_expr(tc, stmt->val.if_stmt.cond);
            if (res.err) {
                return (struct typecheck_result) {.err = true};
            }
            if (res.type != ABC_TYPE_BOOL) {
                fprintf(stderr, "expect bool in if condition\n");
                return (struct typecheck_result) {.err = true};
            }
            res = typecheck_stmt(tc, stmt->val.if_stmt.then_stmt);
            if (res.err) {
                return (struct typecheck_result) {.err = true};
            }
            if (stmt->val.if_stmt.has_else) {
                res = typecheck_stmt(tc, stmt->val.if_stmt.else_stmt);
                if (res.err) {
                    return (struct typecheck_result) {.err = true};
                }
            }
            return (struct typecheck_result) {.err = false};
        case ABC_STMT_WHILE:
            res = typecheck_expr(tc, stmt->val.while_stmt.cond);
            if (res.err) {
                return (struct typecheck_result) {.err = true};
            }
            if (res.type != ABC_TYPE_BOOL) {
                fprintf(stderr, "expect bool in while condition\n");
                return (struct typecheck_result) {.err = true};
            }
            return typecheck_stmt(tc, stmt->val.while_stmt.body);
        case ABC_STMT_BLOCK:
            return typecheck_block_stmt(tc, &stmt->val.block_stmt);
        case ABC_STMT_PRINT:
            res = typecheck_expr(tc, stmt->val.print_stmt.expr);
            if (res.type == ABC_TYPE_VOID) {
                fprintf(stderr, "expect non-void expr in print statement\n");
                return (struct typecheck_result) {.err = true};
            }
            return (struct typecheck_result) {.err = false};
        case ABC_STMT_RETURN:
            if (stmt->val.return_stmt.has_expr) {
                res = typecheck_expr(tc, stmt->val.return_stmt.expr);
                if (res.err) {
                    return (struct typecheck_result) {.err = true};
                }
                if (res.type != (enum abc_type) tc->curr_fun_type) {
                    fprintf(stderr, "type mismatch for return statement\n");
                    return (struct typecheck_result) {.err = true};
                }
                return (struct typecheck_result) {.err = false, .type = res.type};
            } else {
                if (tc->curr_fun_type != PARSER_TYPE_VOID) {
                    fprintf(stderr, "type mismatch for return statement\n");
                    return (struct typecheck_result) {.err = true};
                }
                return (struct typecheck_result) {.err = false};
            }
    }
    assert(0);
}

static struct typecheck_result typecheck_block_stmt(struct abc_typechecker *tc, struct abc_block_stmt *block) {
    push_type_scope(tc);
    bool ok = true;
    for (size_t i = 0; i < block->decls.len; i++) {
        struct abc_decl decl = ((struct abc_decl *) block->decls.data)[i];
        struct typecheck_result result = typecheck_decl(tc, &decl);
        if (result.err) {
            ok = false;
        }
    }
    pop_type_scope(tc);
    return (struct typecheck_result) {.err = !ok};
}

static struct typecheck_result typecheck_bin_expr(struct abc_typechecker *tc, struct abc_bin_expr *expr);
static struct typecheck_result typecheck_unary_expr(struct abc_typechecker *tc, struct abc_unary_expr *expr);
static struct typecheck_result typecheck_call_expr(struct abc_typechecker *tc, struct abc_call_expr *expr);
static struct typecheck_result typecheck_lit_expr(struct abc_typechecker *tc, struct abc_lit_expr *expr);
static struct typecheck_result typecheck_assign_expr(struct abc_typechecker *tc, struct abc_assign_expr *expr);
static struct typecheck_result typecheck_expr(struct abc_typechecker *tc, struct abc_expr *expr) {
    struct typecheck_result result;
    switch (expr->tag) {
        case ABC_EXPR_BINARY:
            result = typecheck_bin_expr(tc, &expr->val.bin_expr);
            break;
        case ABC_EXPR_UNARY:
            result = typecheck_unary_expr(tc, &expr->val.unary_expr);
            break;
        case ABC_EXPR_CALL:
            result = typecheck_call_expr(tc, &expr->val.call_expr);
            break;
        case ABC_EXPR_LITERAL:
            result = typecheck_lit_expr(tc, &expr->val.lit_expr);
            break;
        case ABC_EXPR_ASSIGN:
            result = typecheck_assign_expr(tc, &expr->val.assign_expr);
            break;
        case ABC_EXPR_GROUPING:
            result = typecheck_expr(tc, expr->val.grouping_expr.expr);
            break;
        default:
            assert(0);
    }
    if (!result.err) {
        expr->type = result.type;
    }
    return result;
}

static struct typecheck_result typecheck_bin_expr(struct abc_typechecker *tc, struct abc_bin_expr *expr) {
    struct typecheck_result left = typecheck_expr(tc, expr->left);
    struct typecheck_result right = typecheck_expr(tc, expr->right);

    if (left.err || right.err) {
        return (struct typecheck_result) {.err = true};
    }

    if (expr->op.type == TOKEN_OR || expr->op.type == TOKEN_AND) {
        if (left.type != ABC_TYPE_BOOL || right.type != ABC_TYPE_BOOL) {
            fprintf(stderr, "expect bool as lhs and rhs in logical expression\n");
            return (struct typecheck_result) {.err = true};
        }
        return (struct typecheck_result) {.err = false, .type = ABC_TYPE_BOOL};
    }
    if (left.type != ABC_TYPE_INT || right.type != ABC_TYPE_INT) {
        fprintf(stderr, "expect int as lhs and rhs in numerical/relational expression\n");
        return (struct typecheck_result) {.err = true};
    }
    enum abc_type type = ABC_TYPE_BOOL;
    if (expr->op.type == TOKEN_PLUS || expr->op.type == TOKEN_MINUS || expr->op.type == TOKEN_STAR ||
        expr->op.type == TOKEN_SLASH) {
        type = ABC_TYPE_INT;
    }
    return (struct typecheck_result) {.err = false, .type = type};
}

static struct typecheck_result typecheck_unary_expr(struct abc_typechecker *tc, struct abc_unary_expr *expr) {
    struct typecheck_result rhs = typecheck_expr(tc, expr->expr);
    if (rhs.err) {
        return (struct typecheck_result) {.err = true};
    }
    if (expr->op.type == TOKEN_BANG) {
        if (rhs.type != ABC_TYPE_BOOL) {
            fprintf(stderr, "expect bool as rhs of negation\n");
            return (struct typecheck_result) {.err = true};
        }
        return (struct typecheck_result) {.err = false, .type = ABC_TYPE_BOOL};
    }
    if (rhs.type != ABC_TYPE_INT) {
        fprintf(stderr, "expect int as rhs of unary '-'\n");
        return (struct typecheck_result) {.err = true};
    }
    return (struct typecheck_result) {.err = false, .type = ABC_TYPE_INT};
}

static struct typecheck_result typecheck_call_expr(struct abc_typechecker *tc, struct abc_call_expr *expr) {
    struct formals formals;
    if (!lookup_formals(tc, expr->callee.val.identifier.lexeme, &formals)) {
        fprintf(stderr, "unknown function %s\n", expr->callee.val.identifier.lexeme);
        return (struct typecheck_result) {.err = true};
    }
    if (formals.types.len != expr->args.len) {
        fprintf(stderr, "incorrect parameter count for %s\n", expr->callee.val.identifier.lexeme);
        return (struct typecheck_result) {.err = true};
    }
    for (size_t i = 0; i < formals.types.len; i++) {
        struct type type = ((struct type *) formals.types.data)[i];
        struct abc_expr *arg = ((struct abc_expr **) expr->args.data)[i];
        struct typecheck_result result = typecheck_expr(tc, arg);
        if (result.err) {
            return (struct typecheck_result) {.err = true};
        }
        if (type.type != result.type) {
            fprintf(stderr, "type mismatch for parameter %lu in call to %s\n", i + 1,
                    expr->callee.val.identifier.lexeme);
            return (struct typecheck_result) {.err = true};
        }
    }
    return (struct typecheck_result) {.err = false, .type = formals.ret_type};
}

static struct typecheck_result typecheck_lit_expr(struct abc_typechecker *tc, struct abc_lit_expr *expr) {
    if (expr->lit.tag == ABC_LITERAL_INT) {
        return (struct typecheck_result) {.err = false, .type = ABC_TYPE_INT};
    }
    struct type t;
    if (!lookup_type(tc, expr->lit.val.identifier.lexeme, &t)) {
        fprintf(stderr, "reference to unknown identifier %s\n", expr->lit.val.identifier.lexeme);
        return (struct typecheck_result) {.err = true};
    }
    return (struct typecheck_result) {.err = false, .type = t.type};
}

static struct typecheck_result typecheck_assign_expr(struct abc_typechecker *tc, struct abc_assign_expr *expr) {
    struct type t;
    if (!lookup_type(tc, expr->lit.val.identifier.lexeme, &t)) {
        fprintf(stderr, "trying to assign to unknown identifier %s\n", expr->lit.val.identifier.lexeme);
        return (struct typecheck_result) {.err = true};
    }
    struct typecheck_result result = typecheck_expr(tc, expr->expr);
    if (result.err) {
        return (struct typecheck_result) {.err = true};
    }
    if (result.type != t.type) {
        fprintf(stderr, "attempt to assign value to %s of different type\n", expr->lit.val.identifier.lexeme);
        return (struct typecheck_result) {.err = true};
    }
    return (struct typecheck_result) {.err = false, .type = t.type};
}
