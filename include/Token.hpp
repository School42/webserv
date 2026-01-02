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
	TokenType type; // identify the token
	std::string value; // store the value
	int line; // track the line
	int col; // track the colon
};