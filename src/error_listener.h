#pragma once

#include "antlr4-runtime.h"

using namespace antlr4;

class ErrorListener : public BaseErrorListener {
  public:
	virtual void syntaxError(Recognizer *recognizer, Token *offendingSymbol,
            size_t line, size_t charPositionInLine, const std::string &msg,
            std::exception_ptr e) override {

        std::exit(1);
    }
};
