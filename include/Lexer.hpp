#pragma once
#include "Token.hpp"
#include <string>

class Lexer {
public:
	// Constructor
	explicit Lexer(const std::string& input);
	
	// Destructor
	~Lexer();
	
	// Public interface
	Token nextToken();
	Token peekToken();

private:
	// Non-copyable
	Lexer(const Lexer& other);
	Lexer& operator=(const Lexer& rhs);
	
	// Members
	const std::string& _input;
	size_t _pos;
	int _line;
	int _col;
	
	// Helper methods
	void skipWhitespace();
	void skipComments();
	char currentChar() const;
	char advance();
	
	Token makeToken(TokenType type, const std::string& value);
	Token lexIdentifier();
};