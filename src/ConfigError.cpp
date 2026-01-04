#include "ConfigError.hpp"

// Static helper to build what() string
std::string ConfigError::buildWhatString(const std::string& message, int line, int col, bool has_location) {
	std::stringstream ss;
	
	if (has_location) {
		ss << "Config error at line " << line << ", column " << col << ": " << message;
	} else {
		ss << "Config error: " << message;
	}
	
	return ss.str();
}

// Constructor with explicit line/col
ConfigError::ConfigError(const std::string& message, int line, int col)
	: std::runtime_error(buildWhatString(message, line, col, true)),
	  _message(message),
	  _line(line),
	  _col(col),
	  _has_location(true) {}

// Constructor with Token
ConfigError::ConfigError(const std::string& message, const Token& token)
	: std::runtime_error(buildWhatString(message, token.line, token.col, true)),
	  _message(message),
	  _line(token.line),
	  _col(token.col),
	  _has_location(true) {}

// Constructor for general errors (no location)
ConfigError::ConfigError(const std::string& message)
	: std::runtime_error(buildWhatString(message, 0, 0, false)),
	  _message(message),
	  _line(0),
	  _col(0),
	  _has_location(false) {}

// Destructor
ConfigError::~ConfigError() throw() {}

// Getters
int ConfigError::getLine() const { return _line; }
int ConfigError::getColumn() const { return _col; }
bool ConfigError::hasLocation() const { return _has_location; }

// Format complete error message
std::string ConfigError::formatMessage() const {
	return buildWhatString(_message, _line, _col, _has_location);
}