#include <fstream>
#include <sstream>
#include <iostream>
#include "Lexer.hpp"
#include "Parser.hpp"
#include "ConfigError.hpp"

int main(int argc, char** argv) {
	try {
		if (argc != 2) {
			std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
			return 1;
		}
		
		// Read file
		std::ifstream file(argv[1]);
		if (!file) {
			std::cerr << "Error: Cannot open file: " << argv[1] << std::endl;
			return 1;
		}
		
		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string config = buffer.str();
		file.close();
		
		// Parse
		Lexer lexer(config);
		Parser parser(lexer);
		std::vector<ServerConfig> servers = parser.parse();
		
		std::cout << "\n✓ Configuration parsed successfully!" << std::endl;
		std::cout << "Total servers: " << servers.size() << std::endl;
		
		return 0;
		
	} catch (const ConfigError& e) {
		// ConfigError with detailed location info
		std::cerr << "\n✗ " << e.formatMessage() << std::endl;
		return 1;
	} catch (const std::runtime_error& e) {
		// Fallback for any other runtime errors
		std::cerr << "\n✗ Runtime error: " << e.what() << std::endl;
		return 1;
	} catch (const std::exception& e) {
		// Catch-all for unexpected errors
		std::cerr << "\n✗ Unexpected error: " << e.what() << std::endl;
		return 1;
	}
}