#include "MiniParser.h"
#include "type_checker.h"
#include "ast.h"
#include <concepts>
#include <unordered_map>
#include <unordered_set>
#include <variant>

using Environment = std::unordered_map<std::string, Type>;

template <typename T>
concept TypeContext = std::same_as<T, MiniParser::TypeContext> ||
                      std::same_as<T, MiniParser::ReturnTypeContext>;

Type type_from_ctx(TypeContext auto *ctx) {
    if (auto ret = dynamic_cast<MiniParser::ReturnTypeContext *>(ctx)) {
        if (auto real = dynamic_cast<MiniParser::ReturnTypeRealContext *>(ret)) {
            return type_from_ctx(real->type());
        } else {
            return Void {};
        }
    }

    if (dynamic_cast<MiniParser::ArrayTypeContext *>(ctx)) {
        return Array {};
    } else if (dynamic_cast<MiniParser::BoolTypeContext *>(ctx)) {
        return Bool {};
    } else if (auto s_ctx = dynamic_cast<MiniParser::StructTypeContext *>(ctx)) {
        return Struct { s_ctx->ID()->getText() };
    } else {
        return Int {};
    }
}

template <typename T, typename K>
concept HasContains = requires(T t, K k) {
    { t.contains(k) } -> std::convertible_to<bool>;
};

bool check_type(Type &t, HasContains<std::string> auto &types) {
    if (auto s = std::get_if<Struct>(&t)) {
        if (!types.contains(s->id)) {
            std::cerr << "Invalid type: '" << s->id << "'\n";
            return false;
        }
    }

    return true;
}

bool check_declarations(std::vector<Declaration> &decls, HasContains<std::string> auto &types) {
    std::unordered_set<std::string> ids;

    for (auto &decl : decls) {
        if (ids.contains(decl.id)) {
            std::cerr << "Duplicate declaration: '" << decl.id << "'\n";
            return false;
        }

        if (!check_type(decl.type, types)) {
            std::cerr << "in declaration: '" << decl.id << "'\n";
            return false;
        }

        ids.insert(decl.id);
    }

    return true;
}

template <typename T>
std::vector<Declaration> declarations(T *ctx) {
    std::vector<Declaration> decls;

    for (auto *decl_p : ctx->decl()) {
        std::string decl_id = decl_p->ID()->getText();
        Type decl_t = type_from_ctx(decl_p->type());
        decls.push_back({ decl_t, decl_id });
    }

    return decls;
}

std::vector<Declaration> multi_declarations(MiniParser::DeclarationsContext *ctx) {
    std::vector<Declaration> decls;

    for (auto *decl_p : ctx->declaration()) {
        for (auto *id_p : decl_p->ID()) {
            std::string decl_id = id_p->getText();
            Type decl_t = type_from_ctx(decl_p->type());
            decls.push_back({ decl_t, decl_id });
        }
    }

    return decls;
}

/* validates and extracts types */
TypeDeclarations type_declarations(MiniParser::TypesContext *ctx) {
    std::unordered_set<std::string> type_ids;

    /* need to store type names first in case type member isn't declared yet */
    for (auto *type_decl_p : ctx->typeDeclaration()) {
        std::string type_id = type_decl_p->ID()->getText();
        if (type_ids.contains(type_id)) {
            std::cerr << "Duplicate type declaration: '" << type_id << "'\n";
            std::exit(1);
        }
        type_ids.insert(type_id);
    }

    TypeDeclarations type_decls;
    for (auto *type_decl_p : ctx->typeDeclaration()) {
        std::string type_id = type_decl_p->ID()->getText();
        std::vector<Declaration> decls = declarations(type_decl_p->nestedDecl());

        if (!check_declarations(decls, type_ids)) {
            std::cerr << "in type declaration: '" << type_id << "'\n";
            std::exit(1);
        }

        type_decls[type_id] = decls;
    }

    return type_decls;
}

Environment environment(std::vector<Declaration> &decls) {
    Environment env;

    for (Declaration decl : decls) {
        env[decl.id] = decl.type;
    }

    return env;
}

Function function(MiniParser::FunctionContext *ctx, TypeDeclarations &type_decls, Environment &top_env) {
    std::string name = ctx->ID()->getText();

    /* check return type */
    Type return_t = type_from_ctx(ctx->returnType());
    if (!check_type(return_t, type_decls)) {
        std::cerr << "in function declaration: '" << name << "'\n";
        std::exit(1);
    }

    std::vector<Declaration> decls = multi_declarations(ctx->declarations());
    std::vector<Declaration> params = declarations(ctx->parameters());

    /* combine decls and params for environment */
    std::vector<Declaration> env_decls;
    env_decls.reserve(decls.size() + params.size());
    env_decls.insert(env_decls.end(), decls.cbegin(), decls.cend());
    env_decls.insert(env_decls.end(), params.cbegin(), params.cend());

    if (!check_declarations(env_decls, type_decls)) {
        std::cerr << "in function declaration: '" << name << "'\n";
        std::exit(1);
    }

    Environment local_env = environment(env_decls);

    return Function { name, params, return_t, decls };
}

std::vector<Function> functions(MiniParser::FunctionsContext *ctx,
        TypeDeclarations &type_decls, Environment &env) {
    std::vector<Function> funcs;

    for (auto *func_p : ctx->function()) {
        funcs.push_back(function(func_p, type_decls, env));
    }

    std::unordered_map<std::string, bool> is_int;
    for (auto &func : funcs) {
        if (is_int.contains(func.id)) {
            std::cerr << "Duplicate function declaration '" << func.id << "'\n";
            std::exit(1);
        }
        is_int[func.id] = std::holds_alternative<Int>(func.return_type);
    }

    if (!is_int["main"]) {
        std::cerr << "No function 'main' with return type 'int' detected\n";
        std::exit(1);
    }

    return funcs;
}

bool TypeChecker::check_program(MiniParser::ProgramContext *ctx) {
    TypeDeclarations type_decls = type_declarations(ctx->types());

    std::vector<Declaration> decls = multi_declarations(ctx->declarations());
    if (!check_declarations(decls, type_decls)) {
        std::cerr << "in top-level declaration\n";
        std::exit(1);
    }

    Environment top_env = environment(decls);

    std::vector<Function> funcs = functions(ctx->functions(), type_decls, top_env);

    return false;
}
