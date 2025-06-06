#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <variant>
#include <memory>

struct Int {};
struct Bool {};
struct Struct { std::string id; };
struct Array {};
struct Null {};
struct Void {};

using Type = std::variant<Int, Bool, Struct, Array, Void>;

struct Declaration {
    Type type;
    std::string id;
};

using TypeDeclarations = std::unordered_map<std::string, std::vector<Declaration>>;

/* Expressions */

struct Id { std::string id; };
struct True {};
struct False {};
struct NewStruct { std::string id; };
struct NewArray { int size; };

struct Invocation;
struct Dot;
struct Index;
struct Unary;
struct Binary;

using Expression = std::variant<Invocation, Dot, Index, Unary, Binary, Id, int,
                                True, False, NewStruct, NewArray, Null>;

struct Invocation {
    std::string id;
    std::vector<Expression> arguments;
};

struct Dot {
    std::unique_ptr<Expression> expr;
    std::string id;
};

struct Index {
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> index;
};

struct Unary {
    enum class Op { Negative, Not };

    Op op;
    std::unique_ptr<Expression> expr;
};

struct Binary {
    enum class Op {
		Mul, Div, Add, Sub, Eq, NotEq, Greater,
        GreaterEq, Less, LessEq, And, Or
    };

    Op op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

/* Statements */

struct Print { Expression expr; };
struct PrintLn { Expression expr; };
struct Delete { Expression expr; };
struct Return { std::optional<Expression> expr; };

struct LValueDot;
struct LValueIndex;

using LValue = std::variant<std::string, LValueDot, LValueIndex>;

struct LValueDot {
    std::unique_ptr<LValue> lvalue;
    std::string id;
};
struct LValueIndex {
    std::unique_ptr<LValue> lvalue;
    Expression index;
};

struct Read {};
struct Assignment {
    std::variant<Expression, Read> source;
    LValue lvalue;
};

struct Conditional;
struct Loop;

using Statement = std::variant<Invocation, Assignment, Conditional, Loop,
                               Print, PrintLn, Delete, Return>;

struct Conditional {
	Expression guard;
	std::vector<Statement> then;
	std::optional<std::vector<Statement>> els;
};

struct Loop {
	Expression guard;
	std::vector<Statement> body;
};

/* Program */

struct Function {
	std::string id;
    std::vector<Declaration> parameters;
    Type return_type;
    std::vector<Declaration> declarations;
    std::vector<Statement> body;
};

struct Program {
    TypeDeclarations types;
    std::vector<Declaration> declarations;
    std::vector<Function> functions;
};
