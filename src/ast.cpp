#include "ast.h"
#include "MiniParser.h"
#include <memory>
#include <variant>

/*  converts ANTLR parse tree to my AST format */

struct TypeEqualityVisitor {
    bool operator()(const Struct &, const Null &) const {
        return true;
    }

    bool operator()(const Null &, const Struct &) const {
        return true;
    }

    bool operator()(const Struct &s1, const Struct &s2) const {
        return s1.id == s2.id;
    }

    template <typename T>
    bool operator()(const T &, const T &) const {
        return true;
    }

    template <typename T1, typename T2>
    bool operator()(const T1 &, const T2 &) const {
        return false;
    }
};

bool operator==(const Type &lhs, const Type &rhs) {
    return std::visit(TypeEqualityVisitor {}, lhs, rhs);
}

std::ostream &operator<<(std::ostream &os, const Type &type) {
    std::string type_string;
    if (auto s = std::get_if<Struct>(&type)) {
        os << "'struct " << s->id << "'";
        return os;
    }
    else if (std::holds_alternative<Int>(type)) type_string = "'int'";
    else if (std::holds_alternative<Bool>(type)) type_string = "'bool'";
    else if (std::holds_alternative<Array>(type)) type_string = "'array'";
    else if (std::holds_alternative<Null>(type)) type_string = "'null'";
    else if (std::holds_alternative<Void>(type)) type_string = "'void'";

    os << type_string;
    return os;
}

template <typename T>
concept TypeContext = std::same_as<T, MiniParser::TypeContext> ||
                      std::same_as<T, MiniParser::ReturnTypeContext>;

Type type_from_ctx(TypeContext auto *ctx) {
    if (auto ret = dynamic_cast<MiniParser::ReturnTypeContext *>(ctx)) {
        if (auto real = dynamic_cast<MiniParser::ReturnTypeRealContext *>(ret)) {
            return type_from_ctx(real->type());
        } else if (auto real = dynamic_cast<MiniParser::ReturnTypeVoidContext *>(ret)) {
            return Void {};
        }
    }

    if (dynamic_cast<MiniParser::ArrayTypeContext *>(ctx)) {
        return Array {};
    } else if (dynamic_cast<MiniParser::BoolTypeContext *>(ctx)) {
        return Bool {};
    } else if (auto s_ctx = dynamic_cast<MiniParser::StructTypeContext *>(ctx)) {
        return Struct { s_ctx->ID()->getText() };
    } else if (dynamic_cast<MiniParser::IntTypeContext *>(ctx)) {
        return Int {};
    }

    std::cerr << "Unhandled AST type, qutting...\n";
    std::exit(1);
}

template <typename T>
std::vector<Declaration> declarations(T *ctx) {
    std::vector<Declaration> decls;
    decls.reserve(ctx->decl().size());

    for (auto *decl_p : ctx->decl()) {
        std::string decl_id = decl_p->ID()->getText();
        Type decl_t = type_from_ctx(decl_p->type());
        decls.emplace_back(decl_t, decl_id);
    }

    return decls;
}

std::vector<Declaration> multi_declarations(MiniParser::DeclarationsContext *ctx) {
    std::vector<Declaration> decls;

    for (auto *decl_p : ctx->declaration()) {
        for (auto *id_p : decl_p->ID()) {
            std::string decl_id = id_p->getText();
            Type decl_t = type_from_ctx(decl_p->type());
            decls.emplace_back(decl_t, decl_id);
        }
    }

    return decls;
}

TypeDeclarations type_declarations(MiniParser::TypesContext *ctx) {
    TypeDeclarations type_decls;

    for (auto *type_decl_p : ctx->typeDeclaration()) {
        std::string type_id = type_decl_p->ID()->getText();
        std::vector<Declaration> decls = declarations(type_decl_p->nestedDecl());
        if (!type_decls.emplace(type_id, decls).second) {
            std::cerr << "Duplicate type declaration: '" << type_id << "'\n";
        }
    }

    return type_decls;
}

/* expressions */

Expression expression(MiniParser::ExpressionContext *ctx);

UnaryOp unary_op(const std::string &op) {
    if (op == "-") return Negative {};
    else if (op == "!") return Not {};

    std::cerr << "Unhandled AST unary op, qutting...\n";
    std::exit(1);
}

Unary unary(MiniParser::UnaryExprContext *ctx) {
    UnaryOp op = unary_op(ctx->op->getText());
    Expression expr = expression(ctx->expression());
    return Unary {
        std::move(op),
        std::make_unique<Expression>(std::move(expr))
    };
}

BinaryOp binary_op(const std::string &op) {
    if (op == "*") return Mul {};
    else if (op == "/") return Div {};
    else if (op == "+") return Add {};
    else if (op == "-") return Sub {};
    else if (op == "==") return Eq {};
    else if (op == "!=") return Neq {};
    else if (op == ">") return Grt {};
    else if (op == ">=") return Geq {};
    else if (op == "<") return Lst {};
    else if (op == "<=") return Leq {};
    else if (op == "&&") return And {};
    else if (op == "||") return Or {};

    std::cerr << "Unhandled AST binary op, qutting...\n";
    std::exit(1);
}

Binary binary(MiniParser::BinaryExprContext *ctx) {
    BinaryOp op = binary_op(ctx->op->getText());
    Expression left = expression(ctx->lft);
    Expression right = expression(ctx->rht);
    return Binary {
        std::move(op),
        std::make_unique<Expression>(std::move(left)),
        std::make_unique<Expression>(std::move(right))
    };
}

Index index(MiniParser::IndexExprContext *ctx) {
    Expression left = expression(ctx->lft);
    Expression index = expression(ctx->idx);
    return Index {
        std::make_unique<Expression>(std::move(left)),
        std::make_unique<Expression>(std::move(index))
    };
}

Dot dot(MiniParser::DotExprContext *ctx) {
    Expression expr = expression(ctx->expression());
    std::string id = ctx->ID()->getText();
    return Dot {
        std::make_unique<Expression>(std::move(expr)),
        std::move(id)
    };
}

std::vector<Expression> expressions(const std::vector<MiniParser::ExpressionContext *> &exprs_p) {
    std::vector<Expression> exprs;
    exprs.reserve(exprs_p.size());

    for (auto &expr_p : exprs_p) {
        exprs.emplace_back(expression(expr_p));
    }

    return exprs;
}

template <typename T>
Invocation invocation(T *ctx) {
    std::string id = ctx->ID()->getText();
    std::vector<Expression> args = expressions(ctx->arguments()->expression());

    return Invocation {
        std::move(id),
        std::move(args)
    };
}

Expression expression(MiniParser::ExpressionContext *ctx) {
    if (dynamic_cast<MiniParser::TrueExprContext *>(ctx)) {
        return True {};
    } else if (dynamic_cast<MiniParser::FalseExprContext *>(ctx)) {
        return False {};
    } else if (dynamic_cast<MiniParser::NullExprContext *>(ctx)) {
        return Null {};
    } else if (auto new_p = dynamic_cast<MiniParser::NewExprContext *>(ctx)) {
        return NewStruct {new_p->ID()->getText()};
    } else if (auto newa_p = dynamic_cast<MiniParser::NewArrayExprContext *>(ctx)) {
        return NewArray {std::stoi(newa_p->INTEGER()->getText())};
    } else if (auto int_p = dynamic_cast<MiniParser::IntegerExprContext *>(ctx)) {
        return std::stoi(int_p->INTEGER()->getText());
    } else if (auto id_p = dynamic_cast<MiniParser::IdentifierExprContext *>(ctx)) {
        return id_p->ID()->getText();
    } else if (auto unary_p = dynamic_cast<MiniParser::UnaryExprContext *>(ctx)) {
        return unary(unary_p);
    } else if (auto binary_p = dynamic_cast<MiniParser::BinaryExprContext *>(ctx)) {
        return binary(binary_p);
    } else if (auto index_p = dynamic_cast<MiniParser::IndexExprContext *>(ctx)) {
        return index(index_p);
    } else if (auto dot_p = dynamic_cast<MiniParser::DotExprContext *>(ctx)) {
        return dot(dot_p);
    } else if (auto nest_p = dynamic_cast<MiniParser::NestedExprContext *>(ctx)) {
        return expression(nest_p->expression());
    } else if (auto inv_p = dynamic_cast<MiniParser::InvocationExprContext *>(ctx)) {
        return invocation(inv_p);
    }

    std::cerr << "Unhandled AST expression, qutting...\n";
    std::exit(1);
}

/* statements */

Statement statement(MiniParser::StatementContext *ctx);

Block block(const std::vector<MiniParser::StatementContext *> &stmts_p) {
    Block blk;
    blk.reserve(stmts_p.size());

    for (auto &stmt_p : stmts_p) {
        blk.emplace_back(statement(stmt_p));
    }

    return blk;
}

void flatten_block(Block &acc, Block &&source) {
    for (auto it = source.begin(); it != source.end(); it = source.erase(it)) {
        Statement& current_stmt = *it;

        if (Conditional *cond = std::get_if<Conditional>(&current_stmt)) {
            Block flat_then;
            flatten_block(flat_then, std::move(cond->then));

            std::optional<Block> flat_els_opt = std::nullopt;
            if (cond->els.has_value()) {
                Block flat_els;
                flatten_block(flat_els, std::move(cond->els.value()));
                flat_els_opt = std::move(flat_els);
            }

            acc.emplace_back(Conditional {
                std::move(cond->guard),
                std::move(flat_then),
                std::move(flat_els_opt)
            });
        } else if (Loop *loop = std::get_if<Loop>(&current_stmt)) {
            Block flat_body;

            flatten_block(flat_body, std::move(loop->body));

            acc.emplace_back(Loop {
                std::move(loop->guard),
                std::move(flat_body)
            });
        } else if (Block *block = std::get_if<Block>(&current_stmt)) {
            flatten_block(acc, std::move(*block));
        } else {
            acc.emplace_back(std::move(current_stmt));
        }
    }
}

Loop loop(MiniParser::WhileContext *ctx) {
    Expression guard = expression(ctx->expression());
    Block blk = block(ctx->block()->statementList()->statement());

    return Loop {
        std::move(guard),
        std::move(blk)
    };
}

Conditional conditional(MiniParser::ConditionalContext *ctx) {
    Expression guard = expression(ctx->expression());
    Block then = block(ctx->thenBlock->statementList()->statement());
    if (ctx->elseBlock == nullptr) {
        return Conditional {
            std::move(guard),
            std::move(then),
            std::nullopt
        };
    }

    Block els = block(ctx->elseBlock->statementList()->statement());
    return Conditional {
        std::move(guard),
        std::move(then),
        std::move(els)
    };
}

Return retvrn(MiniParser::ReturnContext *ctx) {
    if (ctx->expression() == nullptr) {
        return Return { std::nullopt };
    }
    return Return { expression(ctx->expression()) };
}

LValue lvalue(MiniParser::LvalueContext *ctx) {
    if (auto id_p = dynamic_cast<MiniParser::LvalueIdContext *>(ctx)) {
        return id_p->ID()->getText();
    } else if (auto dot_p = dynamic_cast<MiniParser::LvalueDotContext *>(ctx)) {
        LValue lval = lvalue(dot_p->lvalue());
        return LValueDot {
            std::make_unique<LValue>(std::move(lval)),
            dot_p->ID()->getText()
        };
    } else if (auto idx_p = dynamic_cast<MiniParser::LvalueIndexContext *>(ctx)) {
        LValue lval = lvalue(idx_p->lvalue());
        return LValueIndex {
            std::make_unique<LValue>(std::move(lval)),
            expression(idx_p->expression())
        };
    }

    std::cerr << "Unhandled AST lvalue, qutting...\n";
    std::exit(1);
}

Assignment assignment(MiniParser::AssignmentContext *ctx) {
    LValue lval = lvalue(ctx->lvalue());
    if (ctx->expression() == nullptr) {
        return Assignment { std::move(lval), Read {} };
    }

    return Assignment { std::move(lval), expression(ctx->expression())};
}

Statement statement(MiniParser::StatementContext *ctx) {
    if (auto del_p = dynamic_cast<MiniParser::DeleteContext *>(ctx)) {
        return Delete { expression(del_p->expression()) };
    } else if (auto pri_p = dynamic_cast<MiniParser::PrintContext *>(ctx)) {
        return Print { expression(pri_p->expression()) };
    } else if (auto pln_p = dynamic_cast<MiniParser::PrintLnContext *>(ctx)) {
        return PrintLn { expression(pln_p->expression()) };
    } else if (auto ret_p = dynamic_cast<MiniParser::ReturnContext *>(ctx)) {
        return retvrn(ret_p);
    } else if (auto inv_p = dynamic_cast<MiniParser::InvocationContext *>(ctx)) {
        return invocation(inv_p);
    } else if (auto nest_p = dynamic_cast<MiniParser::NestedBlockContext *>(ctx)) {
        return block(nest_p->block()->statementList()->statement());
    } else if (auto loop_p = dynamic_cast<MiniParser::WhileContext *>(ctx)) {
        return loop(loop_p);
    } else if (auto cond_p = dynamic_cast<MiniParser::ConditionalContext *>(ctx)) {
        return conditional(cond_p);
    } else if (auto ass_p = dynamic_cast<MiniParser::AssignmentContext *>(ctx)) {
        return assignment(ass_p);
    }

    std::cerr << "Unhandled AST statement, qutting...\n";
    std::exit(1);
}

Function function(MiniParser::FunctionContext *ctx) {
    std::string func_id = ctx->ID()->getText();
    std::vector<Declaration> params = declarations(ctx->parameters());
    Type return_t = type_from_ctx(ctx->returnType());
    std::vector<Declaration> decls = multi_declarations(ctx->declarations());
    Block flat_blk;
    flatten_block(flat_blk, block(ctx->statementList()->statement()));

    return Function {
        std::move(func_id),
        std::move(params),
        std::move(return_t),
        std::move(decls),
        std::move(flat_blk)
    };
}

Functions functions(MiniParser::FunctionsContext *ctx) {
    Functions funcs;

    for (auto *func_p : ctx->function()) {
        std::string func_id = func_p->ID()->getText();
        if (!funcs.emplace(func_id, function(func_p)).second) {
            std::cerr << "Duplicate function definition: '" << func_id << "'\n";
            std::exit(1);
        }
    }

    return funcs;
}

Program parse_program(MiniParser::ProgramContext *ctx) {
    TypeDeclarations type_decls = type_declarations(ctx->types());
    std::vector<Declaration> decls = multi_declarations(ctx->declarations());
    Functions funcs = functions(ctx->functions());

    return {
        std::move(type_decls),
        std::move(decls),
        std::move(funcs)
    };
}
