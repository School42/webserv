#pragma once
#include "Lexer.hpp"
#include "ServerConfig.hpp"
#include "LocationConfig.hpp"
#include "ConfigError.hpp"
#include <vector>
#include <set>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <set>

// Directive scope categories
enum DirectiveScope {
	SCOPE_SERVER_ONLY,   // listen, server_name, error_page
	SCOPE_LOCATION_ONLY, // return, cgi_pass, cgi_extension, upload_store, allowed_methods
	SCOPE_BOTH           // root, index, autoindex, client_max_body_size
};

// Directive arity
enum DirectiveArity {
	SINGLE_VALUE,  // Expects exactly one value
	MULTI_VALUE    // Can have multiple values
};

// Duplicate policy
enum DuplicatePolicy {
	DUP_FORBIDDEN,   // Directive can appear only once
	DUP_ALLOWED,     // Directive can appear multiple times (values are accumulated)
	DUP_UNIQUE_KEY   // Multiple appearances allowed, but keys must be unique (e.g., listen ports)
};

// Directive specification
struct DirectiveSpec {
	std::string name;
	DirectiveScope scope;
	DirectiveArity arity;
	DuplicatePolicy dupPolicy;
};

// Global directive table (NOTE: 'host' removed - use 'listen interface:port' instead)
static const DirectiveSpec g_directives[] = {
	// Server-only directives
	{"listen",               SCOPE_SERVER_ONLY,   MULTI_VALUE,  DUP_UNIQUE_KEY},
	{"server_name",          SCOPE_SERVER_ONLY,   MULTI_VALUE,  DUP_UNIQUE_KEY},
	{"error_page",           SCOPE_SERVER_ONLY,   MULTI_VALUE,  DUP_UNIQUE_KEY},
	
	// Location-only directives
	{"return",               SCOPE_LOCATION_ONLY, SINGLE_VALUE, DUP_FORBIDDEN},
	{"cgi_pass",             SCOPE_LOCATION_ONLY, MULTI_VALUE,  DUP_UNIQUE_KEY},
	{"cgi_extension",        SCOPE_LOCATION_ONLY, MULTI_VALUE,  DUP_UNIQUE_KEY},
	{"upload_store",         SCOPE_LOCATION_ONLY, SINGLE_VALUE, DUP_FORBIDDEN},
	{"allowed_methods",      SCOPE_LOCATION_ONLY, MULTI_VALUE,  DUP_UNIQUE_KEY},
	
	// Both server and location (inheritable)
	{"root",                 SCOPE_BOTH,          SINGLE_VALUE, DUP_FORBIDDEN},
	{"index",                SCOPE_BOTH,          MULTI_VALUE,  DUP_UNIQUE_KEY},
	{"autoindex",            SCOPE_BOTH,          SINGLE_VALUE, DUP_FORBIDDEN},
	{"client_max_body_size", SCOPE_BOTH,          SINGLE_VALUE, DUP_FORBIDDEN}
};

class Parser {
public:
	// Constructor
	explicit Parser(Lexer& lexer);
	
	// Destructor
	~Parser();
	
	// Main parsing entry point
	std::vector<ServerConfig> parse();

private:
	// Non-copyable
	Parser(const Parser& other);
	Parser& operator=(const Parser& rhs);
	
	// Members
	Lexer& _lexer;
	Token _current;
	
	// Token control
	void advance();
	bool match(TokenType type);
	Token expect(TokenType type, const std::string& error);
	
	// Helper
	std::vector<Token> collectValuesUntilSemicolon();
	
	// Grammar
	void parseServerBlock(std::vector<ServerConfig>& servers);
	void parseServerDirective(ServerConfig& server, std::set<std::string>& seen);
	void parseLocationBlock(ServerConfig& server);
	void parseLocationDirective(LocationConfig& location, std::set<std::string>& seen);
	
	// Directive application
	void applyServerDirective(ServerConfig& server, const Token& name, const std::vector<Token>& values);
	void applyLocationDirective(LocationConfig& location, const Token& name, const std::vector<Token>& values);
	
	// Directive lookup
	const DirectiveSpec* findDirectiveInServer(const std::string& name) const;
	const DirectiveSpec* findDirectiveInLocation(const std::string& name) const;
	
	// Listen parsing helper
	ListenAddress parseListenAddress(const Token& token) const;
	
	// Validation helpers
	bool isValidIPv4(const std::string& ip) const;
	bool isValidRedirectUrl(const std::string& url) const;
	
	// Validation & defaults
	void validateAndApplyDefaults(ServerConfig& server);
	void applyServerDefaults(ServerConfig& server);
	void applyLocationDefaults(LocationConfig& location, ServerConfig& server);
	
	// Debug printing
	void debugPrintServers(const std::vector<ServerConfig>& servers) const;
	void debugPrintServer(const ServerConfig& server) const;
	void debugPrintLocation(const LocationConfig& location) const;

	// duplicate check
	// std::set<std::string> _seen_server_names;
	std::set<ListenAddress> _seen_listen;
};