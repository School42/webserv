#include "Lexer.hpp"
#include <cctype>

Lexer::Lexer(const std::string& input)
	: _input(input), _pos(0), _line(1), _col(1) {}

Lexer::~Lexer() {}

char Lexer::currentChar() const {
	if (_pos >= _input.size())
		return '\0';
	return _input[_pos];
}

char Lexer::advance() {
	char c = currentChar();
	_pos++;
	
	if (c == '\n') {
		_line++;
		_col = 1;
	} else {
		_col++;
	}
	return c;
}

void Lexer::skipWhitespace() {
	while (std::isspace(static_cast<unsigned char>(currentChar()))) {
		advance();
	}
}

void Lexer::skipComments() {
	if (currentChar() == '#') {
		advance();
		while (currentChar() != '\n' && currentChar() != '\0') {
			advance();
		}
	}
}

Token Lexer::makeToken(TokenType type, const std::string& value) {
	Token tok;
	tok.type = type;
	tok.value = value;
	tok.line = _line;
	tok.col = static_cast<int>(_col - value.length());
	return tok;
}

Token Lexer::lexIdentifier() {
	size_t start = _pos;
	
	while (std::isalnum(static_cast<unsigned char>(currentChar())) ||
	       currentChar() == '_' ||
	       currentChar() == '/' ||
	       currentChar() == '.' ||
	       currentChar() == '-' ||
	       currentChar() == ':') {
		advance();
	}
	
	return makeToken(TOK_IDENT, _input.substr(start, _pos - start));
}

Token Lexer::nextToken() {
	while (true) {
		skipWhitespace();
		if (currentChar() == '#') {
			skipComments();
			continue;
		}
		break;
	}
	
	char c = currentChar();
	
	if (c == '\0')
		return makeToken(TOK_EOF, "");
	
	if (std::isalnum(static_cast<unsigned char>(c)) || 
	    c == '/' || c == '.' || c == '-' || c == '_' || c == ':')
		return lexIdentifier();
	
	advance();
	
	switch (c) {
		case '{': return makeToken(TOK_LBRACE, "{");
		case '}': return makeToken(TOK_RBRACE, "}");
		case ';': return makeToken(TOK_SEMICOLON, ";");
		default:
			return makeToken(TOK_ERROR, std::string(1, c));
	}
}

Token Lexer::peekToken() {
	size_t savedPos = _pos;
	int savedLine = _line;
	int savedCol = _col;
	
	Token tok = nextToken();
	
	_pos = savedPos;
	_line = savedLine;
	_col = savedCol;
	
	return tok;
}