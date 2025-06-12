#include "MiniLexer.h"
#include "MiniParser.h"

#include "aasm.h"
#include "print_aasm.h"
#include "ast.h"
#include "cfg.h"
#include "type_checker.h"
#include "error_listener.h"

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

    antlr4::ANTLRInputStream input(MiniFile);
    MiniLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    MiniParser parser(&tokens);

    ErrorListener errorListener;
    parser.addErrorListener(&errorListener);

	Program prog = parse_program(parser.program());

	/* typecheck */
	check_program(prog);

	cfg::Program cfg_prog = cfg_program(std::move(prog));
	/* cfg_enumerate(cfg_prog, true); */

	aasm_program(cfg_prog);
	print_aasm_program(cfg_prog);

    MiniFile.close();

    return 0;
}
