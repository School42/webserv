#pragma once
#include <string>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include "Token.hpp"

class ConfigError : public std::runtime_error {
public:
	// Constructor with explicit line/col
	ConfigError(const std::string& message, int line, int col);
	
	// Constructor with Token (extracts line/col automatically)
	ConfigError(const std::string& message, const Token& token);
	
	// Constructor for general errors (no location)
	explicit ConfigError(const std::string& message);
	
	// Destructor
	~ConfigError() throw();
	
	// Getters
	int getLine() const;
	int getColumn() const;
	bool hasLocation() const;
	
	// Format complete error message
	std::string formatMessage() const;

private:
	std::string _message;
	int _line;
	int _col;
	bool _has_location;
	
	// Helper to build what() string
	std::string buildWhatString() const;
};