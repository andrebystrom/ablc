#include "abc_typechecker.h"

#include <assert.h>
#include <string.h>

struct abc_typechecker {
    struct abc_pool *pool;
    enum abc_type curr_fun_type;
    // TODO: these should be a map for better efficiency
    struct abc_arr types;
    struct abc_arr formals;
};

enum typecheck_type { TYPE_VOID, TYPE_INT, TYPE_BOOL };

struct type {
    char *name;
    enum typecheck_type type;
    bool marker;
};

// function argument types
struct formals {
    char *name;
    struct abc_arr types;
    enum typecheck_type ret_type;
    bool marker;
};

struct typecheck_result {
    bool err;
    enum typecheck_type type;
};

void abc_typechecker_init(struct abc_typechecker *typechecker) {
    typechecker->pool = abc_pool_create();
    abc_arr_init(&typechecker->types, sizeof(struct type), typechecker->pool);
    abc_arr_init(&typechecker->types, sizeof(struct formals), typechecker->pool);
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

static struct typecheck_result typecheck_fun(struct abc_typechecker *tc, struct abc_fun_decl *fun);
static struct typecheck_result typecheck_decl(struct abc_typechecker *tc, struct abc_decl *decl);
static struct typecheck_result typecheck_stmt(struct abc_typechecker *tc, struct abc_stmt *stmt);
static struct typecheck_result typecheck_block_stmt(struct abc_typechecker *tc, struct abc_block_stmt *block);
static struct typecheck_result typecheck_expr(struct abc_typechecker *tc, struct abc_expr *expr);

bool abc_typechecker_typecheck(struct abc_typechecker *typechecker, struct abc_program *program) {
    bool ok = true;
    for (size_t i = 0; i < program->fun_decls.len; i++) {
        struct abc_fun_decl fun_decl = ((struct abc_fun_decl *) program->fun_decls.data)[i];
        if (typecheck_fun(typechecker, &fun_decl).err) {
            ok = false;
        }
    }
    return ok;
}

static struct typecheck_result typecheck_fun(struct abc_typechecker *tc, struct abc_fun_decl *fun) {
    struct formals formals = {.marker = false, .name = fun->name.lexeme, .ret_type = fun->type};
    abc_arr_init(&formals.types, sizeof(enum typecheck_type), tc->pool);
    for (size_t i = 0; i < fun->params.len; i++) {
        struct abc_param param = ((struct abc_param *) fun->params.data)[i];
        enum typecheck_type type = param.type;
        abc_arr_push(&formals.types, &type);
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
    switch (decl->tag) {
        case ABC_DECL_VAR:
            // TODO check against redeclaration
            struct type type = {.name = decl->val.var.name.lexeme, .type = decl->val.var.type};
            if (decl->val.var.has_init) {
                struct typecheck_result result = typecheck_expr(tc, decl->val.var.init);
                if (result.err || result.type != decl->val.var.type) {
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
            return (struct typecheck_result) {0};
    }
}

static struct typecheck_result typecheck_stmt(struct abc_typechecker *tc, struct abc_stmt *stmt) {
    switch (stmt->tag) {
        case ABC_STMT_EXPR:
            return (struct typecheck_result) {0};
        case ABC_STMT_IF:
            return (struct typecheck_result) {0};
        case ABC_STMT_WHILE:
            return (struct typecheck_result) {0};
        case ABC_STMT_BLOCK:
            return (struct typecheck_result) {0};
        case ABC_STMT_PRINT:
            return (struct typecheck_result) {0};
        case ABC_STMT_RETURN:
            return (struct typecheck_result) {0};
    }
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

static struct typecheck_result typecheck_expr(struct abc_typechecker *tc, struct abc_expr *expr) {
    return (struct typecheck_result) {0};
}
