#include "Parser.hpp"

Parser::Parser(Lexer& lexer) : _lexer(lexer) {
	advance();
}

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
		throw std::runtime_error(error);
	Token t = _current;
	advance();
	return t;
}

const DirectiveSpec* Parser::findDirective(const std::string& name, DirectiveScope scope) {
	for (size_t i = 0; i < sizeof(g_directives) / sizeof(g_directives[0]) ; ++i) {
		if (g_directives[i].name == name && g_directives[i].scope == scope)
			return &g_directives[i];
	}
	return NULL;
}

std::vector<Token> Parser::collectValuesUntilSemicolon() {
	std::vector<Token> values;

	while (_current.type != TOK_SEMICOLON) {
		if (_current.type == TOK_EOF)
			throw std::runtime_error("Unexpected EOF");
		
		values.push_back(_current);
		advance();
	}
	expect(TOK_SEMICOLON, "Expected ','");
	return values;
}

std::vector<ServerConfig> Parser::parse(void){
	std::vector<ServerConfig> servers;
	while (_current.type != TOK_EOF)
	{
		if (_current.type == TOK_EOF)
			break;
		if (_current.type == TOK_IDENT && _current.value == "server")
			parseServerBlock(servers);
		else
			throw std::runtime_error("Expected 'server' block");
	}
	if (servers.empty())
		throw std::runtime_error("config must contain at least one server block");
	debugPrintServers(servers);
	return servers;
}

void Parser::parseServerBlock(std::vector<ServerConfig>& servers)
{
	expect(TOK_IDENT, "Expected 'server'");
	expect(TOK_LBRACE, "Expected '{' after server");

	ServerConfig server;
	SeenSet seenDirectives;

	while (_current.type != TOK_RBRACE)
	{
		if (_current.type == TOK_EOF)
			throw std::runtime_error("Unexpected EOF in server block");
		// location block
		if (_current.type == TOK_IDENT && _current.value == "location")
			parseLocationBlock(server);
		else
			parseServerDirective(server, seenDirectives);
	}

	expect(TOK_RBRACE, "Expected '}' after server block");
	servers.push_back(server);
}

void Parser::parseServerDirective(ServerConfig& server, std::set<std::string>& seen) {
	Token name = expect(TOK_IDENT, "Expected server directive");

	const DirectiveSpec* spec = findDirective(name.value, SCOPE_SERVER);
	if (!spec)
		throw std::runtime_error("Invalid server directive: " + name.value);

	// single value enforcement
	if (spec->arity == SINGLE_VALUE && seen.count(name.value))
		throw std::runtime_error("Duplicate directive: " + name.value);
	
	seen.insert(name.value);

	// collect values
	std::vector<Token> values = collectValuesUntilSemicolon();

	// dispatch to severconfig
	applyServerDirective(server, name, values);
}

void Parser::parseLocationBlock(ServerConfig& server)
{
	expect(TOK_IDENT, "Expected 'location'");

	Token path = expect(TOK_IDENT, "Expected location path");
	expect(TOK_LBRACE, "Expected '{' after location path");

	LocationConfig location(path.value);
	SeenSet seenDirectives;

	while (_current.type != TOK_RBRACE)
	{
		if (_current.type == TOK_EOF)
			throw std::runtime_error("Unexpected EOF in location block");

		parseLocationDirective(location, seenDirectives);
	}

	expect(TOK_RBRACE, "Expected '}' after location block");
	server.addLocation(location);
}

void Parser::parseLocationDirective(LocationConfig& location, std::set<std::string>& seen)
{
	Token name = expect(TOK_IDENT, "Expected location directive");

	const DirectiveSpec* spec = findDirective(name.value, SCOPE_LOCATION);
	if (!spec)
		throw std::runtime_error("Invalid location directive: " + name.value);

	if (spec->arity == SINGLE_VALUE && seen.count(name.value))
		throw std::runtime_error("Duplicate directive: " + name.value);
	
	seen.insert(name.value);

	// values
	std::vector<Token> values = collectValuesUntilSemicolon();

	applyLocationDirective(location, name, values);	
}

void Parser::enforceDuplication(const DirectiveSpec& spec, const Token& directive, bool alreadySeen) {
	if (spec.arity == SINGLE_VALUE && alreadySeen)
		throw std::runtime_error("Duplicate directive: " + directive.value);
}

// helpers
static int toInt(const Token& t) {
	char* end = NULL;
	errno = 0;

	long val = std::strtol(t.value.c_str(), &end, 10);

	if (errno != 0 || end == t.value.c_str() || *end != '\0')
		throw std::runtime_error("Invalid integer: " + t.value);
	
	if (val < INT_MIN || val > INT_MAX)
		throw std::runtime_error("Integer out of range: " + t.value);
	
	return static_cast<int>(val);
}

// static size_t toSize(const Token& t) {
// 	char* end = NULL;
// 	errno = 0;

// 	unsigned long val = std::strtoul(t.value.c_str(), &end, 10);

// 	if (errno != 0 || end == t.value.c_str() || *end != '\0')
// 		throw std::runtime_error("Invalid size: " + t.value);
	
// 	return static_cast<size_t>(val);
// }

static size_t parseSize(const Token& t) {
	const std::string& s = t.value;

	char* end = NULL;
	errno = 0;

	unsigned long base = std::strtoul(s.c_str(), &end, 10);

	if (errno != 0 || end == s.c_str())
		throw std::runtime_error("Invalid size: " + s);

	size_t multiplier = 1;

	if (*end != '\0') {
		if (*(end + 1) != '\0')
			throw std::runtime_error("Invalid size unit: " + s);

		switch (*end) {
			case 'K': multiplier = 1024; break;
			case 'M': multiplier = 1024 * 1024; break;
			case 'G': multiplier = 1024 * 1024 * 1024; break;
			default:
				throw std::runtime_error("Invalid size unit: " + s);
		}
	}

	return static_cast<size_t>(base) * multiplier;
}

void Parser::applyServerDirective(ServerConfig& server, const Token& name, const std::vector<Token>& values) {
	const std::string& dir = name.value;

	// listen
	if (dir == "listen")
	{
		if (values.size() != 1)
			throw std::runtime_error("listen expects exactly one argument");
		
		int port = toInt(values[0]);
		if (port <= 0 || port > 65536)
			throw std::runtime_error("Invalid listen port");
		
		server.addListen(port);
		return ;
	}

	// server name
	if (dir == "server_name")
	{
		if (values.empty())
			throw std::runtime_error("server_name expects at least one value");
		
		for (size_t i = 0; i < values.size(); ++i)
			server.addServerName(values[i].value);
		return ;
	}

	// root
	if (dir == "root")
	{
		if (values.size() != 1)
			throw std::runtime_error("root expects exactly one argument");
		
		server.setRoot(values[0].value);
		return;
	}

	// host
	if (dir == "host")
	{
		if (values.size() != 1)
			throw std::runtime_error("host expects exactly one value");
		
		server.setHost(values[0].value);
		return;
	}

	// index
	if (dir == "index")
	{
		if (values.empty())
			throw std::runtime_error("index expects at least one value");
		
		for (size_t i = 0; i < values.size(); ++i)
			server.addIndex(values[i].value);
		return ;
	}

	// client_max_body_size
	if (dir == "client_max_body_size")
	{
		if (values.size() != 1)
			throw std::runtime_error("client_max_body_size expects one value");
		
		size_t size = parseSize(values[0]);
		server.setClientMaxBodySize(size);
		return;
	}

	// error page
	if (dir == "error_page")
	{
		if (values.size() != 2)
			throw std::runtime_error("error_page expects 2 arguemnts");
		
		int code = toInt(values[0]);
		const std::string& uri = values[1].value;
		
		server.addErrorPage(code, uri);
		return;
	}
	throw std::runtime_error("Unhandled server directive: " + dir);
}

void Parser::applyLocationDirective(LocationConfig& location, const Token& name, const std::vector<Token>& values)
{
	const std::string& dir = name.value;

	// root
	if (dir == "root") {
		if (values.size() != 1)
			throw std::runtime_error("root expects exactly one argument");

		location.setRoot(values[0].value);
		return ;
	}

	// autoindex
	if (dir == "autoindex") {
		if (values.size() != 1)
			throw std::runtime_error("autoindex expects exactly one argument");
		
		if (values[0].value == "on")
			location.setAutoIndex(true);
		else if (values[0].value == "off")
			location.setAutoIndex(false);
		else
			throw std::runtime_error("autoindex must be on or off");
		return;
	}

	// client_max_body_size
	if (dir == "client_max_body_size") {
		if (values.size() != 1)
			throw std::runtime_error("client_max_body_size expects exactly one value");
		size_t size = parseSize(values[0]);
		location.setClientMaxBodySize(size);
		return ;
	}

	// cgi_pass
	if (dir == "cgi_pass") {
		if (values.size() != 1)
			throw std::runtime_error("cgi_pass expects exactly one argument");
		location.setCgiPass(values[0].value);
		return ;
	}

	// return
	if (dir == "return") {
		if (values.size() != 2)
			throw std::runtime_error("return expects return code and redirect link");
		location.setReturn(values[1].value, values[0].value);
		return ;
	}

	// index
	if (dir == "index"){
		if (values.empty())
			throw std::runtime_error("index expects at least one value");

		for (size_t i = 0; i < values.size(); ++i)
			location.addIndex(values[i].value);
		return;
	}

	// upload store directory
	if (dir == "upload_store"){
		if (values.size() != 1)
			throw std::runtime_error("upload_store expects exactly one value");
		location.setUploadStore(values[0].value);
		return;
	}

	// allow_methods
	if (dir == "allowed_methods") {
		if (values.empty())
			throw std::runtime_error("allow_methods expects at least one value");

		for (size_t i = 0; i < values.size(); ++i)
			location.addAllowedMethod(values[i].value);
		return;
	}

	throw std::runtime_error("Unknown location directive: " + dir);
}

// printing
void Parser::debugPrintServers(const std::vector<ServerConfig>& servers) const {
	std::cout << "=== Parsed Servers ===\n";
	for (size_t i = 0; i < servers.size(); ++i) {
		std::cout << "\n[Server " << i << "]\n";
		debugPrintServer(servers[i]);
	}
}

void Parser::debugPrintServer(const ServerConfig& s) const {
	// listen
	std::cout << "listen: ";
	for (size_t i = 0; i < s.getListenPorts().size(); ++i)
		std::cout << s.getListenPorts()[i] << " ";
	std::cout << "\n";

	// server_name
	std::cout << "server_name: ";
	for (size_t i = 0; i < s.getServerNames().size(); ++i)
		std::cout << s.getServerNames()[i] << " ";
	std::cout << "\n";

	// root
	std::cout << "root: " << s.getRoot() << "\n";

	// index
	std::cout << "index: ";
	for (size_t i = 0; i < s.getIndex().size(); ++i)
		std::cout << s.getIndex()[i] << " ";
	std::cout << "\n";

	// client_max_body_size
	std::cout << "client_max_body_size: " << s.getClientMaxBodySize() << "\n";

	// error_page
	std::cout << "error_page:\n";
	const std::map<int, std::string>& ep = s.getErrorPages();
	for (std::map<int, std::string>::const_iterator it = ep.begin(); it != ep.end(); ++it)
		std::cout << "  " << it->first << " -> " << it->second << "\n";

	// locations
	const std::vector<LocationConfig>& locs = s.getLocations();
	for (size_t i = 0; i < locs.size(); ++i) {
		std::cout << "\n  [Location " << i << "]\n";
		debugPrintLocation(locs[i]);
	}
}

void Parser::debugPrintLocation(const LocationConfig& l) const {
	std::cout << "  path: " << l.getPath() << "\n";
	std::cout << "  root: " << l.getRoot() << "\n";

	std::cout << "  index: ";
	const std::vector<std::string>& idx = l.getIndex();
	for (size_t i = 0; i < idx.size(); ++i)
		std::cout << idx[i] << " ";
	std::cout << "\n";

	std::cout << "  autoindex: " << (l.getAutoIndex() ? "on" : "off") << "\n";

	std::cout << "  allowed_methods: ";
	const std::vector<std::string>& m = l.getAllowedMethods();
	for (size_t i = 0; i < m.size(); ++i)
		std::cout << m[i] << " ";
	std::cout << "\n";
	std::cout << "  upload_store: " << l.getUploadStore() << std::endl;
	std::cout << "  return: " << l.getReturnCode() << " -> " << l.getReturnValue() << "\n";
	std::cout << "  cgi_pass: ";
	const std::vector<std::string>& cgi = l.getCgiPass();
	for (size_t i = 0; i < cgi.size(); ++i)
		std::cout << cgi[i] << " ";
	std::cout << "\n";
	std::cout << "  client_max_body_size: " << l.getClientMaxBodySize() << "\n";
}
