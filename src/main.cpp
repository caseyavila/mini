#include "MiniLexer.h"
#include "MiniParser.h"

#include "error_listener.h"
#include "type_checker.h"

using namespace antlr4;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file>\n";
        return 1;
    }

	std::ifstream MiniFile(argv[1]);

	if (!MiniFile.is_open()) {
        std::perror("Failed to open input file");
        return 1;
    }

    ANTLRInputStream input(MiniFile);
    MiniLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    MiniParser parser(&tokens);

    ErrorListener errorListener;
    parser.addErrorListener(&errorListener);

    TypeChecker types_check;
	types_check.check_program(parser.program());

    MiniFile.close();
}
