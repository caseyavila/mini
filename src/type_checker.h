#pragma once

#include "MiniBaseVisitor.h"
#include "antlr4-runtime.h"

#include "ast.h"

class TypeChecker {
  public:
    bool check_program(MiniParser::ProgramContext *ctx);
};
