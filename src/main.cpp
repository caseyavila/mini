#include "MiniLexer.h"
#include "MiniParser.h"

#include "aasm.h"
#include "print_aasm.h"
#include "ast.h"
#include "cfg.h"
#include "type_checker.h"
#include "error_listener.h"
#include <cstdlib>

constexpr std::string_view extension = ".mini";

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file>\n";
        return 1;
    }

    std::string mini_name = argv[1];
    if (!mini_name.ends_with(extension)) {
        std::cerr << "Can only compile files ending with '" << extension << "'\n";
        return 1;
    }

	std::ifstream mini_file(mini_name);

	if (!mini_file.is_open()) {
        std::perror("Failed to open input file");
        return 1;
    }

    antlr4::ANTLRInputStream input(mini_file);
    MiniLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    MiniParser parser(&tokens);

    ErrorListener errorListener;
    parser.addErrorListener(&errorListener);

    /* generate AST */
	Program prog = parse_program(parser.program());
    mini_file.close();

	/* typecheck */
	check_program(prog);

	/* generate CFG */
	cfg::Program cfg_prog = cfg_program(std::move(prog));
	/* cfg_enumerate(cfg_prog, true); */

	/* gerate AASM */
	aasm_program(cfg_prog);

	std::string exec_name = mini_name.substr(0, mini_name.size() - extension.size());
	std::string ll_name = exec_name + ".ll";
	std::ofstream ll_file(ll_name);

	/* save stdout and redirect to .ll */
	std::streambuf *stdout = std::cout.rdbuf();
	std::cout.rdbuf(ll_file.rdbuf());

	print_aasm_program(cfg_prog);

	std::cout.rdbuf(stdout);
	ll_file.close();

	std::system(("clang util.c " + ll_name).c_str());
	//std::remove(ll_name.c_str());

    return 0;
}
