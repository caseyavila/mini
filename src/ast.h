#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <variant>
#include <memory>
#include <iostream>

#include "MiniParser.h"

struct Int {};
struct Bool {};
struct Struct { std::string id; };
struct Array {};
struct Null {};
struct Void {};

using Type = std::variant<Int, Bool, Struct, Array, Void, Null>;
bool operator==(const Type& lhs, const Type& rhs);
std::ostream& operator<<(std::ostream& os, const Type& type);

using Environment = std::unordered_map<std::string, Type>;

struct Declaration {
    Type type;
    std::string id;
};

using TypeDeclarations = std::unordered_map<std::string, std::vector<Declaration>>;

/* Expressions */

struct True {};
struct False {};
struct NewStruct { std::string id; };
struct NewArray { int size; };

struct Invocation;
struct Dot;
struct Index;
struct Unary;
struct Binary;

using Expression = std::variant<Invocation, Dot, Index, Unary, Binary, std::string,
                                int, True, False, NewStruct, NewArray, Null>;

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

struct Negative {};
struct Not {};

using UnaryOp = std::variant<Negative, Not>;

struct Unary {
    UnaryOp op;
    std::unique_ptr<Expression> expr;
};

struct Mul {};
struct Div {};
struct Add {};
struct Sub {};
struct Eq {};
struct Neq {};
struct Grt {};
struct Geq {};
struct Lst {};
struct Leq {};
struct And {};
struct Or {};

using BinaryOp = std::variant<Mul, Div, Add, Sub, Eq, Neq,
                              Grt, Geq, Lst, Leq, And, Or>;

struct Binary {
    BinaryOp op;
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
    LValue lvalue;
    std::variant<Expression, Read> source;
};

struct Conditional;
struct Loop;

/* you're really gonna make me do this... */
template<class T>
using Smt = std::variant<Invocation, Assignment, Conditional, Loop,
                         Print, PrintLn, Delete, Return, std::vector<T>>;

template <template<class> class K>
struct Fix : K<Fix<K>>
{
   using K<Fix>::K;
};

using Statement = Fix<Smt>;
using Block = std::vector<Statement>;

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

template <typename T>
struct GenericFunction {
	std::string id;
    std::vector<Declaration> parameters;
    Type return_type;
    std::vector<Declaration> declarations;
    T body;
    Environment local_env;
};

template <typename T>
struct GenericProgram {
    TypeDeclarations types;
    std::vector<Declaration> declarations;
    T functions;
    Environment top_env;
};

using Function = GenericFunction<Block>;
using Functions = std::unordered_map<std::string, Function>;
using Program = GenericProgram<Functions>;

Program parse_program(MiniParser::ProgramContext *ctx);
