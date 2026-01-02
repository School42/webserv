#include <fstream>
#include <sstream>
#include <iostream>
#include "Lexer.hpp"
#include "Parser.hpp"

int main(int argc, char** argv) {
	try {
		if (argc != 2)
			return 1;
		
		std::ifstream file(argv[1]);
		if (!file)
			return 1;
		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string config = buffer.str();

		Lexer lexer(config);
		Parser parser(lexer);
		std::vector<ServerConfig> servers = parser.parse();
	} catch(const std::runtime_error& e){
		std::cerr << e.what() << std::endl;
		return 1;
	}
}