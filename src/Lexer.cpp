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

Token Lexer::lexQuotedString() {
	char quote = currentChar(); // store the quote char
	int startLine = _line;
	int startCol = _col;

	advance(); // skip  opening quote

	std::string value;

	while (currentChar() != '\0' && currentChar() != quote) {
		// handle escape sequences
		if (currentChar() == '\\') {
			advance(); // skip backslash

			if (currentChar() == '\0') {
				Token errTok;
				errTok.type = TOK_ERROR;
				errTok.value = "Unterminated string - unexpected end of file after escape";
				errTok.line = startLine;
				errTok.col = startCol;
				return errTok;
			}

			// handle common escape sequences
			switch (currentChar()) {
				case 'n': value += '\n'; break;
				case 't': value += '\t'; break;
				case 'r': value += '\r'; break;
				case '\\': value += '\\'; break;
				case '"': value += '"'; break;
				case '\'': value += '\''; break;
				default: value += currentChar(); break; // unknown escape
			}
			advance();
		} else if (currentChar() == '\n') {
			Token errTok;
			errTok.type = TOK_ERROR;
			errTok.value = "Unterminated string - newline in string";
			errTok.line = startLine;
			errTok.col = startCol;
			return errTok;
		} else {
			value += currentChar();
			advance();
		}
	}

	if (currentChar() == '\0') {
		Token errTok;
		errTok.type = TOK_ERROR;
		errTok.value = "Unterminated string";
		errTok.line = startLine;
		errTok.col = startCol;
		return errTok;
	}

	advance(); // skip closing quote

	Token tok;
	tok.type = TOK_IDENT;
	tok.value = value;
	tok.line = startLine;
	tok.col = startCol;
	return tok;
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

	// handle quoted strings
	if (c == '"' || c == '\'')
		return lexQuotedString();
	
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