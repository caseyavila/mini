#pragma once

#include "MiniParser.h"

#include "ast.h"

class TypeChecker {
  public:
    bool check_program(MiniParser::ProgramContext *ctx);
};
