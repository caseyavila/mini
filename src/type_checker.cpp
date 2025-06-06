#include "type_checker.h"
#include "MiniParser.h"
#include "ast.h"
#include <unordered_map>
#include <unordered_set>
#include <variant>

using Environment = std::unordered_map<std::string, Type>;

Type type_from_ctx(MiniParser::TypeContext *ctx) {
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

/* validates and extracts types */
TypeDeclarations types(MiniParser::TypesContext *ctx) {
    TypeDeclarations decls;
    std::unordered_set<std::string> type_ids;

    /* need to store type names first in case type member isn't declared yet */
    for (auto *type_decl_p : ctx->typeDeclaration()) {
        std::string id = type_decl_p->ID()->getText();
        if (type_ids.contains(id)) {
            std::cerr << "Duplicate type declaration: '" << id << "'\n";
            std::exit(1);
        }
        type_ids.insert(id);
    }

    for (auto *type_decl_p : ctx->typeDeclaration()) {
        std::string type_id = type_decl_p->ID()->getText();
        std::unordered_set<std::string> member_list;

        for (auto *decl_p : type_decl_p->nestedDecl()->decl()) {
            Declaration member;

            member.id = decl_p->ID()->getText();
            if (member_list.contains(member.id)) {
                std::cerr << "Duplicate member '" << member.id
                          << "' in type '" << type_id << "'\n";
                std::exit(1);
            }
            member_list.insert(member.id);

            member.type = type_from_ctx(decl_p->type());
            if (auto s = std::get_if<Struct>(&member.type)) {
                if (!type_ids.contains(s->id)) {
                    std::cerr << "Member '" << member.id
                              << "' has invalid type: '" << s->id << "'\n";
                    std::exit(1);
                }
            }

            decls[type_id].push_back(member);
        }
    }

    return decls;
}

Environment top_environment(MiniParser::DeclarationsContext *ctx, TypeDeclarations &decls) {
    Environment env;

    for (auto *decl_p : ctx->declaration()) {
        for (auto *id_p : decl_p->ID()) {
            std::string decl_id = id_p->getText();

            if (env.contains(decl_id)) {
                std::cerr << "Duplicate declaration: '" << decl_id << "'\n";
                std::exit(1);
            }


            Type decl_type = type_from_ctx(decl_p->type());
            if (auto s = std::get_if<Struct>(&decl_type)) {
                if (!decls.contains(s->id)) {
                    std::cerr << "Declaration' " << decl_id
                              << "' has invalid type: '" << s->id << "'\n";
                    std::exit(1);
                }
            }

            env[decl_id] = decl_type;
        }
    }

    return env;
}

bool TypeChecker::check_program(MiniParser::ProgramContext *ctx) {
    TypeDeclarations type_decls = types(ctx->types());
    Environment top_env = top_environment(ctx->declarations(), type_decls);

    return false;
}
