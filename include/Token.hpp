#pragma once
#include <string>

enum TokenType {
	TOK_IDENT,
	TOK_LBRACE,
	TOK_RBRACE,
	TOK_SEMICOLON,
	TOK_EOF,
	TOK_ERROR
};

struct Token {
	TokenType type;
	std::string value;
	int line;
	int col;
	
	// Constructor for convenience
	Token() : type(TOK_EOF), value(""), line(0), col(0) {}
};