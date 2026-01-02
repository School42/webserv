#pragma once
#include "Lexer.hpp"
#include <vector>
#include <map>
#include "ServerConfig.hpp"
#include "LocationConfig.hpp"
#include <cstdlib>
#include <cerrno>
#include <climits>

// to distinguish the tokens into server and location scope
enum DirectiveScope {
	SCOPE_SERVER,
	SCOPE_LOCATION
};

// is the directive allowed for multiple value or not?
enum DirectiveArity {
	SINGLE_VALUE,
	MULTI_VALUE
};

// for multiple values, duplicate values are allowed or forbidden
enum DuplicatePolicy {
	DUP_FORBIDDEN,
	DUP_ALLOWED,
	DUP_UNIQUE_KEY
};

// spec for each directive
struct DirectiveSpec {
	std::string name;
	DirectiveScope scope;
	DirectiveArity arity;
	DuplicatePolicy dupPolicy;
};

// directive rule table (single source of truth)
static const DirectiveSpec g_directives[] = {

	// SERVER
	{"listen", SCOPE_SERVER, MULTI_VALUE, DUP_UNIQUE_KEY},
	{"server_name", SCOPE_SERVER, MULTI_VALUE, DUP_ALLOWED},
	{"error_page", SCOPE_SERVER, MULTI_VALUE, DUP_UNIQUE_KEY},
	{"host", SCOPE_SERVER, SINGLE_VALUE, DUP_FORBIDDEN},
	{"client_max_body_size", SCOPE_SERVER, SINGLE_VALUE, DUP_FORBIDDEN},

	// LOCATION
	{"root", SCOPE_LOCATION, SINGLE_VALUE, DUP_FORBIDDEN},
	{"index", SCOPE_LOCATION, SINGLE_VALUE, DUP_FORBIDDEN},
	{"autoindex", SCOPE_LOCATION, SINGLE_VALUE, DUP_FORBIDDEN},
	{"upload_store", SCOPE_LOCATION, SINGLE_VALUE, DUP_FORBIDDEN},
	{"return", SCOPE_LOCATION, SINGLE_VALUE, DUP_FORBIDDEN},
	{"client_max_body_size", SCOPE_LOCATION, SINGLE_VALUE, DUP_FORBIDDEN},

	{"allowed_methods", SCOPE_LOCATION, MULTI_VALUE, DUP_ALLOWED},
	{"cgi_extension", SCOPE_LOCATION, MULTI_VALUE, DUP_ALLOWED},
	{"cgi_pass", SCOPE_LOCATION, MULTI_VALUE, DUP_ALLOWED},
};

class Parser {
	public:
		Parser(Lexer& lexer);
		std::vector<ServerConfig> parse();

	private:
		Lexer& _lexer;
		Token _current;

		// token control;
		void advance();
		bool match(TokenType type);
		Token expect(TokenType type, const std::string& error);

		// helper
		std::vector<Token> collectValuesUntilSemicolon(void);
		typedef std::set<std::string> SeenSet;

		// grammar
		void parseServerBlock(std::vector<ServerConfig>& servers);
		void parseServerDirective(ServerConfig& server, std::set<std::string>& seen);
		void parseLocationBlock(ServerConfig& server);
		void parseLocationDirective(LocationConfig& location, std::set<std::string>& seen);

		// parsing directives
		void applyServerDirective(ServerConfig& server, const Token& name, const std::vector<Token>& values);
		void applyLocationDirective(LocationConfig& location, const Token& name, const std::vector<Token>& values);

		// directive rules
		const DirectiveSpec* findDirective(const std::string& name, DirectiveScope scope);

		void enforceDuplication(const DirectiveSpec& spec, const Token& directive, bool alreadySeen);

		// print
		void debugPrintServers(const std::vector<ServerConfig>& servers) const;
		void debugPrintServer(const ServerConfig& server) const;
		void debugPrintLocation(const LocationConfig& location) const;
};