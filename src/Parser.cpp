#include "Parser.hpp"
#include <iostream>

// Constructor
Parser::Parser(Lexer& lexer) : _lexer(lexer) {
	advance();
}

// Destructor
Parser::~Parser() {}

// Token control
void Parser::advance() {
	_current = _lexer.nextToken();
}

bool Parser::match(TokenType type) {
	if (_current.type == type) {
		advance();
		return true;
	}
	return false;
}

Token Parser::expect(TokenType type, const std::string& error) {
	if (_current.type != type)
		throw ConfigError(error, _current);
	Token t = _current;
	advance();
	return t;
}

// Collect all tokens until semicolon
std::vector<Token> Parser::collectValuesUntilSemicolon() {
	std::vector<Token> values;
	
	while (_current.type != TOK_SEMICOLON) {
		if (_current.type == TOK_EOF)
			throw ConfigError("Unexpected end of file, expected ';'", _current);
		
		values.push_back(_current);
		advance();
	}
	
	expect(TOK_SEMICOLON, "Expected ';'");
	return values;
}

// Directive lookup - for server context
const DirectiveSpec* Parser::findDirectiveInServer(const std::string& name) const {
	for (size_t i = 0; i < sizeof(g_directives) / sizeof(g_directives[0]); ++i) {
		if (g_directives[i].name == name &&
		    (g_directives[i].scope == SCOPE_SERVER_ONLY || g_directives[i].scope == SCOPE_BOTH)) {
			return &g_directives[i];
		}
	}
	return NULL;
}

// Directive lookup - for location context
const DirectiveSpec* Parser::findDirectiveInLocation(const std::string& name) const {
	for (size_t i = 0; i < sizeof(g_directives) / sizeof(g_directives[0]); ++i) {
		if (g_directives[i].name == name &&
		    (g_directives[i].scope == SCOPE_LOCATION_ONLY || g_directives[i].scope == SCOPE_BOTH)) {
			return &g_directives[i];
		}
	}
	return NULL;
}

// Parse listen address (support both "port" and "interface:port")
ListenAddress Parser::parseListenAddress(const Token& token) const {
	const std::string& value = token.value;
	size_t colon_pos = value.find(':');
	
	// Case 1: Just port number (e.g., "8080")
	if (colon_pos == std::string::npos) {
		char* end = NULL;
		errno = 0;
		long port = std::strtol(value.c_str(), &end, 10);
		
		if (errno != 0 || end == value.c_str() || *end != '\0')
			throw ConfigError("Invalid port number in listen directive", token);
		
		if (port <= 0 || port > 65535)
			throw ConfigError("Port number must be between 1 and 65535", token);
		
		return ListenAddress("", static_cast<int>(port));
	}
	
	// Case 2: interface:port (e.g., "127.0.0.1:8080")
	std::string interface = value.substr(0, colon_pos);
	std::string port_str = value.substr(colon_pos + 1);
	
	if (interface.empty())
		throw ConfigError("Empty interface in listen directive", token);
	
	if (port_str.empty())
		throw ConfigError("Empty port in listen directive", token);
	
	char* end = NULL;
	errno = 0;
	long port = std::strtol(port_str.c_str(), &end, 10);
	
	if (errno != 0 || end == port_str.c_str() || *end != '\0')
		throw ConfigError("Invalid port number in listen directive", token);
	
	if (port <= 0 || port > 65535)
		throw ConfigError("Port number must be between 1 and 65535", token);
	
	return ListenAddress(interface, static_cast<int>(port));
}

// Helper functions for conversion
static int toInt(const Token& t) {
	char* end = NULL;
	errno = 0;
	
	long val = std::strtol(t.value.c_str(), &end, 10);
	
	if (errno != 0 || end == t.value.c_str() || *end != '\0')
		throw ConfigError("Invalid integer value", t);
	
	if (val < INT_MIN || val > INT_MAX)
		throw ConfigError("Integer out of range", t);
	
	return static_cast<int>(val);
}

static size_t parseSize(const Token& t) {
	const std::string& s = t.value;
	char* end = NULL;
	errno = 0;
	
	unsigned long base = std::strtoul(s.c_str(), &end, 10);
	
	if (errno != 0 || end == s.c_str())
		throw ConfigError("Invalid size value", t);
	
	size_t multiplier = 1;
	
	if (*end != '\0') {
		if (*(end + 1) != '\0')
			throw ConfigError("Invalid size unit (use K, M, or G)", t);
		
		switch (*end) {
			case 'K': case 'k': multiplier = 1024; break;
			case 'M': case 'm': multiplier = 1024 * 1024; break;
			case 'G': case 'g': multiplier = 1024 * 1024 * 1024; break;
			default:
				throw ConfigError("Invalid size unit (use K, M, or G)", t);
		}
	}
	
	return static_cast<size_t>(base) * multiplier;
}

// Main parse entry point
std::vector<ServerConfig> Parser::parse() {
	std::vector<ServerConfig> servers;
	
	while (_current.type != TOK_EOF) {
		if (_current.type == TOK_IDENT && _current.value == "server") {
			parseServerBlock(servers);
		} else {
			throw ConfigError("Expected 'server' block at top level", _current);
		}
	}
	
	if (servers.empty())
		throw ConfigError("Configuration must contain at least one server block");
	
	// Validate and apply defaults for each server
	for (size_t i = 0; i < servers.size(); ++i) {
		validateAndApplyDefaults(servers[i]);
	}
	
	debugPrintServers(servers);
	return servers;
}

// Parse server block
void Parser::parseServerBlock(std::vector<ServerConfig>& servers) {
	expect(TOK_IDENT, "Expected 'server'");
	expect(TOK_LBRACE, "Expected '{' after 'server'");
	
	ServerConfig server;
	std::set<std::string> seenDirectives;
	
	while (_current.type != TOK_RBRACE) {
		if (_current.type == TOK_EOF)
			throw ConfigError("Unexpected end of file in server block", _current);
		
		if (_current.type == TOK_IDENT && _current.value == "location") {
			parseLocationBlock(server);
		} else {
			parseServerDirective(server, seenDirectives);
		}
	}
	
	expect(TOK_RBRACE, "Expected '}' after server block");
	
	// Resolve inheritance for all locations in this server
	server.resolveLocationInheritance();
	
	servers.push_back(server);
}

// Parse server directive
void Parser::parseServerDirective(ServerConfig& server, std::set<std::string>& seen) {
	Token name = expect(TOK_IDENT, "Expected server directive name");
	
	const DirectiveSpec* spec = findDirectiveInServer(name.value);
	if (!spec)
		throw ConfigError("Invalid directive in server context: '" + name.value + "'", name);
	
	// Check for duplicates (single-value directives can't appear twice)
	if (spec->arity == SINGLE_VALUE && seen.count(name.value))
		throw ConfigError("Duplicate directive: '" + name.value + "'", name);
	
	seen.insert(name.value);
	
	// Collect values
	std::vector<Token> values = collectValuesUntilSemicolon();
	
	// Apply to server config
	applyServerDirective(server, name, values);
}

// Parse location block
void Parser::parseLocationBlock(ServerConfig& server) {
	expect(TOK_IDENT, "Expected 'location'");
	
	Token path = expect(TOK_IDENT, "Expected location path");
	expect(TOK_LBRACE, "Expected '{' after location path");
	
	LocationConfig location(path.value);
	std::set<std::string> seenDirectives;
	
	while (_current.type != TOK_RBRACE) {
		if (_current.type == TOK_EOF)
			throw ConfigError("Unexpected end of file in location block", _current);
		
		parseLocationDirective(location, seenDirectives);
	}
	
	expect(TOK_RBRACE, "Expected '}' after location block");
	
	server.addLocation(location);
}

// Parse location directive
void Parser::parseLocationDirective(LocationConfig& location, std::set<std::string>& seen) {
	Token name = expect(TOK_IDENT, "Expected location directive name");
	
	const DirectiveSpec* spec = findDirectiveInLocation(name.value);
	if (!spec)
		throw ConfigError("Invalid directive in location context: '" + name.value + "'", name);
	
	// Check for duplicates
	if (spec->arity == SINGLE_VALUE && seen.count(name.value))
		throw ConfigError("Duplicate directive: '" + name.value + "'", name);
	
	seen.insert(name.value);
	
	// Collect values
	std::vector<Token> values = collectValuesUntilSemicolon();
	
	// Apply to location config
	applyLocationDirective(location, name, values);
}

// Apply server directives
void Parser::applyServerDirective(ServerConfig& server, const Token& name, const std::vector<Token>& values) {
	const std::string& dir = name.value;
	
	// listen (now supports interface:port)
	if (dir == "listen") {
		if (values.size() != 1)
			throw ConfigError("'listen' expects exactly one argument (port or interface:port)", name);
		
		ListenAddress addr = parseListenAddress(values[0]);
		server.addListen(addr);
		return;
	}
	
	// server_name
	if (dir == "server_name") {
		if (values.empty())
			throw ConfigError("'server_name' expects at least one argument", name);
		
		for (size_t i = 0; i < values.size(); ++i)
			server.addServerName(values[i].value);
		return;
	}
	
	// error_page
	if (dir == "error_page") {
		if (values.size() != 2)
			throw ConfigError("'error_page' expects exactly 2 arguments: error_code and URI", name);
		
		int code = toInt(values[0]);
		if (code < 400 || code > 599) {
			std::cout << "Testing\n"; 
			std::cout << "line : " << values[0].line << " col : " << values[0].col << std::endl;
			throw ConfigError("Error code must be between 400 and 599", values[0]);
		}
		server.addErrorPage(code, values[1].value);
		return;
	}
	
	// root (inheritable)
	if (dir == "root") {
		if (values.size() != 1)
			throw ConfigError("'root' expects exactly one argument", name);
		
		if (values[0].value.empty())
			throw ConfigError("'root' path cannot be empty", values[0]);
		
		server.setRoot(values[0].value);
		return;
	}
	
	// index (inheritable)
	if (dir == "index") {
		if (values.empty())
			throw ConfigError("'index' expects at least one argument", name);
		
		for (size_t i = 0; i < values.size(); ++i) {
			if (values[i].value.empty())
				throw ConfigError("Index filename cannot be empty", values[i]);
			server.addIndex(values[i].value);
		}
		return;
	}
	
	// autoindex (inheritable)
	if (dir == "autoindex") {
		if (values.size() != 1)
			throw ConfigError("'autoindex' expects exactly one argument (on or off)", name);
		
		if (values[0].value == "on")
			server.setAutoIndex(true);
		else if (values[0].value == "off")
			server.setAutoIndex(false);
		else
			throw ConfigError("'autoindex' must be 'on' or 'off'", values[0]);
		return;
	}
	
	// client_max_body_size (inheritable)
	if (dir == "client_max_body_size") {
		if (values.size() != 1)
			throw ConfigError("'client_max_body_size' expects exactly one argument", name);
		
		size_t size = parseSize(values[0]);
		server.setClientMaxBodySize(size);
		return;
	}
	
	throw ConfigError("Unhandled server directive: '" + dir + "'", name);
}

// Apply location directives
void Parser::applyLocationDirective(LocationConfig& location, const Token& name, const std::vector<Token>& values) {
	const std::string& dir = name.value;
	
	// return
	if (dir == "return") {
		if (values.size() != 2)
			throw ConfigError("'return' expects exactly 2 arguments: status_code and URL", name);
		
		location.setReturn(values[0].value, values[1].value);
		return;
	}
	
	// cgi_pass
	if (dir == "cgi_pass") {
		if (values.size() != 1)
			throw ConfigError("'cgi_pass' expects exactly one argument", name);
		
		if (values[0].value.empty())
			throw ConfigError("'cgi_pass' path cannot be empty", values[0]);
		
		location.addCgiPass(values[0].value);
		return;
	}
	
	// cgi_extension
	if (dir == "cgi_extension") {
		if (values.empty())
			throw ConfigError("'cgi_extension' expects at least one argument", name);
		
		for (size_t i = 0; i < values.size(); ++i) {
			if (values[i].value.empty())
				throw ConfigError("CGI extension cannot be empty", values[i]);
			location.addCgiExtension(values[i].value);
		}
		return;
	}
	
	// upload_store
	if (dir == "upload_store") {
		if (values.size() != 1)
			throw ConfigError("'upload_store' expects exactly one argument", name);
		
		if (values[0].value.empty())
			throw ConfigError("'upload_store' path cannot be empty", values[0]);
		
		location.setUploadStore(values[0].value);
		return;
	}
	
	// allowed_methods
	if (dir == "allowed_methods") {
		if (values.empty())
			throw ConfigError("'allowed_methods' expects at least one argument", name);
		
		for (size_t i = 0; i < values.size(); ++i) {
			const std::string& method = values[i].value;
			
			// Validate HTTP method (optional but good practice)
			if (method != "GET" && method != "POST" && method != "DELETE" && 
			    method != "PUT" && method != "HEAD" && method != "PATCH" &&
			    method != "OPTIONS" && method != "CONNECT" && method != "TRACE")
				throw ConfigError("Invalid HTTP method: '" + method + "'", values[i]);
			
			location.addAllowedMethod(method);
		}
		return;
	}
	
	// root (inheritable)
	if (dir == "root") {
		if (values.size() != 1)
			throw ConfigError("'root' expects exactly one argument", name);
		
		if (values[0].value.empty())
			throw ConfigError("'root' path cannot be empty", values[0]);
		
		location.setRoot(values[0].value);
		return;
	}
	
	// index (inheritable)
	if (dir == "index") {
		if (values.empty())
			throw ConfigError("'index' expects at least one argument", name);
		
		for (size_t i = 0; i < values.size(); ++i) {
			if (values[i].value.empty())
				throw ConfigError("Index filename cannot be empty", values[i]);
			location.addIndex(values[i].value);
		}
		return;
	}
	
	// autoindex (inheritable)
	if (dir == "autoindex") {
		if (values.size() != 1)
			throw ConfigError("'autoindex' expects exactly one argument (on or off)", name);
		
		if (values[0].value == "on")
			location.setAutoIndex(true);
		else if (values[0].value == "off")
			location.setAutoIndex(false);
		else
			throw ConfigError("'autoindex' must be 'on' or 'off'", values[0]);
		return;
	}
	
	// client_max_body_size (inheritable)
	if (dir == "client_max_body_size") {
		if (values.size() != 1)
			throw ConfigError("'client_max_body_size' expects exactly one argument", name);
		
		size_t size = parseSize(values[0]);
		location.setClientMaxBodySize(size);
		return;
	}
	
	throw ConfigError("Unhandled location directive: '" + dir + "'", name);
}

// Validation and defaults application
void Parser::validateAndApplyDefaults(ServerConfig& server) {
	// Must have at least one listen directive
	if (server.getListenAddresses().empty())
		throw ConfigError("Server block must have at least one 'listen' directive");
	
	// Apply server-level defaults
	applyServerDefaults(server);
	
	// Apply defaults to each location after inheritance
	std::vector<LocationConfig>& locations = server.getLocations();
	for (size_t i = 0; i < locations.size(); ++i) {
		applyLocationDefaults(locations[i]);
		
		// Validate that location has root (either inherited or set)
		if (!locations[i].hasRoot())
			throw ConfigError("Location '" + locations[i].getPath() + 
			                  "' must have 'root' directive (set in server or location block)");
	}
}

void Parser::applyServerDefaults(ServerConfig& server) {
	// Default: client_max_body_size = 1M
	if (!server.hasClientMaxBodySize())
		server.setClientMaxBodySize(1048576); // 1MB
	
	// Default: autoindex = off
	if (!server.hasAutoIndex())
		server.setAutoIndex(false);
	
	// Default: index = index.html
	if (!server.hasIndex())
		server.addIndex("index.html");
}

void Parser::applyLocationDefaults(LocationConfig& location) {
	// Default: client_max_body_size = 1M (if not inherited)
	if (!location.hasClientMaxBodySize())
		location.setClientMaxBodySize(1048576);
	
	// Default: autoindex = off (if not inherited)
	if (!location.hasAutoIndex())
		location.setAutoIndex(false);
	
	// Default: index = index.html (if not inherited)
	if (!location.hasIndex())
		location.addIndex("index.html");
	
	// Default: allowed_methods = GET HEAD POST
	if (location.getAllowedMethods().empty()) {
		location.addAllowedMethod("GET");
		location.addAllowedMethod("HEAD");
		location.addAllowedMethod("POST");
	}
}

// Debug printing
void Parser::debugPrintServers(const std::vector<ServerConfig>& servers) const {
	std::cout << "\n=== Parsed Configuration ===\n";
	for (size_t i = 0; i < servers.size(); ++i) {
		std::cout << "\n[Server " << i << "]\n";
		debugPrintServer(servers[i]);
	}
	std::cout << "\n=== End of Configuration ===\n" << std::endl;
}

void Parser::debugPrintServer(const ServerConfig& s) const {
	// listen addresses
	std::cout << "  listen:\n";
	const std::vector<ListenAddress>& addrs = s.getListenAddresses();
	for (size_t i = 0; i < addrs.size(); ++i) {
		std::cout << "    ";
		if (addrs[i].interface.empty())
			std::cout << "0.0.0.0:" << addrs[i].port;
		else
			std::cout << addrs[i].interface << ":" << addrs[i].port;
		std::cout << "\n";
	}
	
	// server_name
	std::cout << "  server_name: ";
	const std::vector<std::string>& names = s.getServerNames();
	if (names.empty()) {
		std::cout << "(catch-all)";
	} else {
		for (size_t i = 0; i < names.size(); ++i) {
			std::cout << names[i];
			if (i < names.size() - 1) std::cout << ", ";
		}
	}
	std::cout << "\n";
	
	// root
	std::cout << "  root: ";
	if (s.hasRoot())
		std::cout << s.getRoot();
	else
		std::cout << "(not set)";
	std::cout << "\n";
	
	// index
	std::cout << "  index: ";
	const std::vector<std::string>& idx = s.getIndex();
	for (size_t i = 0; i < idx.size(); ++i) {
		std::cout << idx[i];
		if (i < idx.size() - 1) std::cout << ", ";
	}
	std::cout << "\n";
	
	// autoindex
	std::cout << "  autoindex: " << (s.getAutoIndex() ? "on" : "off") << "\n";
	
	// client_max_body_size
	std::cout << "  client_max_body_size: " << s.getClientMaxBodySize() << " bytes";
	if (s.getClientMaxBodySize() >= 1073741824)
		std::cout << " (" << (s.getClientMaxBodySize() / 1073741824.0) << " GB)";
	else if (s.getClientMaxBodySize() >= 1048576)
		std::cout << " (" << (s.getClientMaxBodySize() / 1048576.0) << " MB)";
	else if (s.getClientMaxBodySize() >= 1024)
		std::cout << " (" << (s.getClientMaxBodySize() / 1024.0) << " KB)";
	std::cout << "\n";
	
	// error_page
	const std::map<int, std::string>& errors = s.getErrorPages();
	if (!errors.empty()) {
		std::cout << "  error_page:\n";
		for (std::map<int, std::string>::const_iterator it = errors.begin(); 
		     it != errors.end(); ++it) {
			std::cout << "    " << it->first << " -> " << it->second << "\n";
		}
	}
	
	// locations
	const std::vector<LocationConfig>& locs = s.getLocations();
	if (!locs.empty()) {
		std::cout << "\n  Locations:\n";
		for (size_t i = 0; i < locs.size(); ++i) {
			std::cout << "  [Location " << i << "]\n";
			debugPrintLocation(locs[i]);
		}
	}
}

void Parser::debugPrintLocation(const LocationConfig& l) const {
	std::cout << "    path: " << l.getPath() << "\n";
	
	// root
	std::cout << "    root: ";
	if (l.hasRoot())
		std::cout << l.getRoot();
	else
		std::cout << "(not set)";
	std::cout << "\n";
	
	// index
	std::cout << "    index: ";
	const std::vector<std::string>& idx = l.getIndex();
	if (idx.empty()) {
		std::cout << "(not set)";
	} else {
		for (size_t i = 0; i < idx.size(); ++i) {
			std::cout << idx[i];
			if (i < idx.size() - 1) std::cout << ", ";
		}
	}
	std::cout << "\n";
	
	// autoindex
	std::cout << "    autoindex: ";
	if (l.hasAutoIndex())
		std::cout << (l.getAutoIndex() ? "on" : "off");
	else
		std::cout << "(not set)";
	std::cout << "\n";
	
	// client_max_body_size
	std::cout << "    client_max_body_size: ";
	if (l.hasClientMaxBodySize()) {
		std::cout << l.getClientMaxBodySize() << " bytes";
		if (l.getClientMaxBodySize() >= 1073741824)
			std::cout << " (" << (l.getClientMaxBodySize() / 1073741824.0) << " GB)";
		else if (l.getClientMaxBodySize() >= 1048576)
			std::cout << " (" << (l.getClientMaxBodySize() / 1048576.0) << " MB)";
		else if (l.getClientMaxBodySize() >= 1024)
			std::cout << " (" << (l.getClientMaxBodySize() / 1024.0) << " KB)";
	} else {
		std::cout << "(not set)";
	}
	std::cout << "\n";
	
	// allowed_methods
	std::cout << "    allowed_methods: ";
	const std::vector<std::string>& methods = l.getAllowedMethods();
	if (methods.empty()) {
		std::cout << "(not set)";
	} else {
		for (size_t i = 0; i < methods.size(); ++i) {
			std::cout << methods[i];
			if (i < methods.size() - 1) std::cout << ", ";
		}
	}
	std::cout << "\n";
	
	// return
	if (!l.getReturnCode().empty()) {
		std::cout << "    return: " << l.getReturnCode() 
		          << " -> " << l.getReturnValue() << "\n";
	}
	
	// cgi_pass
	const std::vector<std::string>& cgi = l.getCgiPass();
	if (!cgi.empty()) {
		std::cout << "    cgi_pass: ";
		for (size_t i = 0; i < cgi.size(); ++i) {
			std::cout << cgi[i];
			if (i < cgi.size() - 1) std::cout << ", ";
		}
		std::cout << "\n";
	}
	
	// cgi_extension
	const std::vector<std::string>& ext = l.getCgiExtension();
	if (!ext.empty()) {
		std::cout << "    cgi_extension: ";
		for (size_t i = 0; i < ext.size(); ++i) {
			std::cout << ext[i];
			if (i < ext.size() - 1) std::cout << ", ";
		}
		std::cout << "\n";
	}
	
	// upload_store
	if (!l.getUploadStore().empty()) {
		std::cout << "    upload_store: " << l.getUploadStore() << "\n";
	}
}