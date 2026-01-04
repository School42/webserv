#include "ConfigError.hpp"

// Constructor with explicit line/col
ConfigError::ConfigError(const std::string& message, int line, int col)
	: std::runtime_error(buildWhatString()),
	  _message(message),
	  _line(line),
	  _col(col),
	  _has_location(true) {}

// Constructor with Token
ConfigError::ConfigError(const std::string& message, const Token& token)
	: std::runtime_error(buildWhatString()),
	  _message(message),
	  _line(token.line),
	  _col(token.col),
	  _has_location(true) {}

// Constructor for general errors (no location)
ConfigError::ConfigError(const std::string& message)
	: std::runtime_error(message),
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
	std::stringstream ss;
	
	if (_has_location) {
		ss << "Config error at line " << _line << ", column " << _col << ": " << _message;
	} else {
		ss << "Config error: " << _message;
	}
	
	return ss.str();
}

// Helper to build what() string
std::string ConfigError::buildWhatString() const {
	return formatMessage();
}