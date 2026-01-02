#pragma once
#include "Token.hpp"
#include <string>
#include <iostream>

class Lexer {
	public:
		Lexer(const std::string& input);

		Token nextToken();
		Token peekToken();
	
	private:
		const std::string& _input;
		size_t _pos;
		int _line;
		int _col;

		void skipWhitespace();
		void skipComments();
		char currentChar() const;
		char advance();

		Token makeToken(TokenType type, const std::string& value);
		Token lexIdentifier();
};