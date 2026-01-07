#include "HttpRequest.hpp"
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <iostream>

// Constructor
HttpRequest::HttpRequest()
	: _method(""),
	  _uri(""),
	  _path(""),
	  _queryString(""),
	  _httpVersion(""),
	  _body(""),
	  _contentLength(0),
	  _chunked(false),
	  _currentChunkSize(0),
	  _currentChunkRead(0),
	  _state(PARSE_REQUEST_LINE),
	  _errorMessage(""),
	  _maxBodySize(104857600) {}  // 100MB default

// Destructor
HttpRequest::~HttpRequest() {}

// Reset for new request
void HttpRequest::reset() {
	_method.clear();
	_uri.clear();
	_path.clear();
	_queryString.clear();
	_httpVersion.clear();
	_headers.clear();
	_body.clear();
	_contentLength = 0;
	_chunked = false;
	_currentChunkSize = 0;
	_currentChunkRead = 0;
	_state = PARSE_REQUEST_LINE;
	_errorMessage.clear();
}

// Helper: convert string to lowercase
std::string HttpRequest::toLower(const std::string& str) {
	std::string result = str;
	for (size_t i = 0; i < result.size(); ++i) {
		result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
	}
	return result;
}

// Main parsing method
HttpParseResult HttpRequest::parse(const std::string& data, size_t& bytesConsumed) {
	bytesConsumed = 0;
	size_t pos = 0;
	
	while (pos < data.size() && _state != PARSE_COMPLETE && _state != PARSE_ERROR) {
		switch (_state) {
			case PARSE_REQUEST_LINE: {
				// Look for \r\n
				size_t lineEnd = data.find("\r\n", pos);
				if (lineEnd == std::string::npos) {
					// Check if line is too long
					if (data.size() - pos > MAX_REQUEST_LINE) {
						_state = PARSE_ERROR;
						_errorMessage = "Request line too long";
						return PARSE_FAILED;
					}
					bytesConsumed = pos;
					return PARSE_INCOMPLETE;
				}
				
				std::string line = data.substr(pos, lineEnd - pos);
				if (!parseRequestLine(line)) {
					return PARSE_FAILED;
				}
				
				pos = lineEnd + 2;  // Skip \r\n
				_state = PARSE_HEADERS;
				break;
			}
			
			case PARSE_HEADERS: {
				// Look for \r\n
				size_t lineEnd = data.find("\r\n", pos);
				if (lineEnd == std::string::npos) {
					if (data.size() - pos > MAX_HEADER_SIZE) {
						_state = PARSE_ERROR;
						_errorMessage = "Header line too long";
						return PARSE_FAILED;
					}
					bytesConsumed = pos;
					return PARSE_INCOMPLETE;
				}
				
				std::string line = data.substr(pos, lineEnd - pos);
				pos = lineEnd + 2;  // Skip \r\n
				
				// Empty line = end of headers
				if (line.empty()) {
					// Determine if there's a body
					std::string transferEncoding = getHeader("transfer-encoding");
					if (toLower(transferEncoding) == "chunked") {
						_chunked = true;
						_state = PARSE_CHUNKED_SIZE;
					} else if (_contentLength > 0) {
						if (_contentLength > _maxBodySize) {
							_state = PARSE_ERROR;
							_errorMessage = "Content-Length exceeds maximum body size";
							return PARSE_FAILED;
						}
						_state = PARSE_BODY;
					} else {
						_state = PARSE_COMPLETE;
					}
				} else {
					if (!parseHeader(line)) {
						return PARSE_FAILED;
					}
					
					// Check header count limit
					if (_headers.size() > MAX_HEADERS_COUNT) {
						_state = PARSE_ERROR;
						_errorMessage = "Too many headers";
						return PARSE_FAILED;
					}
				}
				break;
			}
			
			case PARSE_BODY: {
				size_t remaining = _contentLength - _body.size();
				size_t available = data.size() - pos;
				size_t toRead = (available < remaining) ? available : remaining;
				
				_body.append(data.substr(pos, toRead));
				pos += toRead;
				
				if (_body.size() >= _contentLength) {
					_state = PARSE_COMPLETE;
				} else {
					bytesConsumed = pos;
					return PARSE_INCOMPLETE;
				}
				break;
			}
			
			case PARSE_CHUNKED_SIZE: {
				// Look for \r\n
				size_t lineEnd = data.find("\r\n", pos);
				if (lineEnd == std::string::npos) {
					bytesConsumed = pos;
					return PARSE_INCOMPLETE;
				}
				
				std::string line = data.substr(pos, lineEnd - pos);
				pos = lineEnd + 2;
				
				if (!parseChunkedSize(line)) {
					return PARSE_FAILED;
				}
				
				if (_currentChunkSize == 0) {
					_state = PARSE_CHUNKED_TRAILER;
				} else {
					_currentChunkRead = 0;
					_state = PARSE_CHUNKED_DATA;
				}
				break;
			}
			
			case PARSE_CHUNKED_DATA: {
				size_t remaining = _currentChunkSize - _currentChunkRead;
				size_t available = data.size() - pos;
				size_t toRead = (available < remaining) ? available : remaining;
				
				_body.append(data.substr(pos, toRead));
				_currentChunkRead += toRead;
				pos += toRead;
				
				// Check body size limit
				if (_body.size() > _maxBodySize) {
					_state = PARSE_ERROR;
					_errorMessage = "Body exceeds maximum size";
					return PARSE_FAILED;
				}
				
				if (_currentChunkRead >= _currentChunkSize) {
					// Need to read trailing \r\n after chunk data
					if (pos + 2 > data.size()) {
						bytesConsumed = pos;
						return PARSE_INCOMPLETE;
					}
					if (data.substr(pos, 2) != "\r\n") {
						_state = PARSE_ERROR;
						_errorMessage = "Invalid chunk terminator";
						return PARSE_FAILED;
					}
					pos += 2;
					_state = PARSE_CHUNKED_SIZE;
				} else {
					bytesConsumed = pos;
					return PARSE_INCOMPLETE;
				}
				break;
			}
			
			case PARSE_CHUNKED_TRAILER: {
				// Look for empty line (end of trailers)
				size_t lineEnd = data.find("\r\n", pos);
				if (lineEnd == std::string::npos) {
					bytesConsumed = pos;
					return PARSE_INCOMPLETE;
				}
				
				std::string line = data.substr(pos, lineEnd - pos);
				pos = lineEnd + 2;
				
				if (line.empty()) {
					_state = PARSE_COMPLETE;
				}
				// Ignore trailer headers for now
				break;
			}
			
			case PARSE_COMPLETE:
			case PARSE_ERROR:
				break;
		}
	}
	
	bytesConsumed = pos;
	
	if (_state == PARSE_COMPLETE) {
		return PARSE_SUCCESS;
	} else if (_state == PARSE_ERROR) {
		return PARSE_FAILED;
	}
	return PARSE_INCOMPLETE;
}

// Parse request line: METHOD URI HTTP/VERSION
bool HttpRequest::parseRequestLine(const std::string& line) {
	// Find first space (after method)
	size_t firstSpace = line.find(' ');
	if (firstSpace == std::string::npos) {
		_state = PARSE_ERROR;
		_errorMessage = "Invalid request line: missing method";
		return false;
	}
	
	_method = line.substr(0, firstSpace);
	
	// Validate method
	if (_method != "GET" && _method != "POST" && _method != "DELETE") {
		_state = PARSE_ERROR;
		_errorMessage = "Invalid HTTP method: " + _method;
		return false;
	}
	
	// Find second space (after URI)
	size_t secondSpace = line.find(' ', firstSpace + 1);
	if (secondSpace == std::string::npos) {
		_state = PARSE_ERROR;
		_errorMessage = "Invalid request line: missing HTTP version";
		return false;
	}
	
	_uri = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	if (_uri.empty()) {
		_state = PARSE_ERROR;
		_errorMessage = "Invalid request line: empty URI";
		return false;
	}
	
	// Parse URI into path and query string
	parseUri();
	
	_httpVersion = line.substr(secondSpace + 1);
	
	// Validate HTTP version
	if (_httpVersion != "HTTP/1.0" && _httpVersion != "HTTP/1.1") {
		_state = PARSE_ERROR;
		_errorMessage = "Unsupported HTTP version: " + _httpVersion;
		return false;
	}
	
	return true;
}

// Parse header line: Name: Value
bool HttpRequest::parseHeader(const std::string& line) {
	size_t colonPos = line.find(':');
	if (colonPos == std::string::npos) {
		_state = PARSE_ERROR;
		_errorMessage = "Invalid header: missing colon";
		return false;
	}
	
	std::string name = line.substr(0, colonPos);
	std::string value = line.substr(colonPos + 1);
	
	// Trim leading/trailing whitespace from value
	size_t valueStart = value.find_first_not_of(" \t");
	if (valueStart != std::string::npos) {
		value = value.substr(valueStart);
	}
	size_t valueEnd = value.find_last_not_of(" \t");
	if (valueEnd != std::string::npos) {
		value = value.substr(0, valueEnd + 1);
	}
	
	// Store with lowercase name for case-insensitive lookup
	std::string lowerName = toLower(name);
	_headers[lowerName] = value;
	
	// Handle Content-Length specially
	if (lowerName == "content-length") {
		char* end = NULL;
		long len = std::strtol(value.c_str(), &end, 10);
		if (end == value.c_str() || *end != '\0' || len < 0) {
			_state = PARSE_ERROR;
			_errorMessage = "Invalid Content-Length value";
			return false;
		}
		_contentLength = static_cast<size_t>(len);
	}
	
	return true;
}

// Parse chunked size line
bool HttpRequest::parseChunkedSize(const std::string& line) {
	// Size is hex, optionally followed by extension (;...)
	std::string sizeStr = line;
	size_t semiPos = line.find(';');
	if (semiPos != std::string::npos) {
		sizeStr = line.substr(0, semiPos);
	}
	
	// Trim whitespace
	size_t start = sizeStr.find_first_not_of(" \t");
	if (start != std::string::npos) {
		sizeStr = sizeStr.substr(start);
	}
	size_t end = sizeStr.find_last_not_of(" \t");
	if (end != std::string::npos) {
		sizeStr = sizeStr.substr(0, end + 1);
	}
	
	char* endPtr = NULL;
	unsigned long size = std::strtoul(sizeStr.c_str(), &endPtr, 16);
	
	if (endPtr == sizeStr.c_str() || (*endPtr != '\0' && *endPtr != ' ' && *endPtr != '\t')) {
		_state = PARSE_ERROR;
		_errorMessage = "Invalid chunk size";
		return false;
	}
	
	_currentChunkSize = static_cast<size_t>(size);
	return true;
}

// Parse URI into path and query string
void HttpRequest::parseUri() {
	size_t queryPos = _uri.find('?');
	if (queryPos != std::string::npos) {
		_path = _uri.substr(0, queryPos);
		_queryString = _uri.substr(queryPos + 1);
	} else {
		_path = _uri;
		_queryString = "";
	}
	
	// Normalize path (remove .., etc) - basic implementation
	// TODO: More robust path normalization
}

// Getters - Request line
const std::string& HttpRequest::getMethod() const {
	return _method;
}

const std::string& HttpRequest::getUri() const {
	return _uri;
}

const std::string& HttpRequest::getPath() const {
	return _path;
}

const std::string& HttpRequest::getQueryString() const {
	return _queryString;
}

const std::string& HttpRequest::getHttpVersion() const {
	return _httpVersion;
}

// Getters - Headers
std::string HttpRequest::getHeader(const std::string& name) const {
	std::string lowerName = toLower(name);
	std::map<std::string, std::string>::const_iterator it = _headers.find(lowerName);
	if (it != _headers.end()) {
		return it->second;
	}
	return "";
}

const std::map<std::string, std::string>& HttpRequest::getHeaders() const {
	return _headers;
}

bool HttpRequest::hasHeader(const std::string& name) const {
	std::string lowerName = toLower(name);
	return _headers.find(lowerName) != _headers.end();
}

// Getters - Body
const std::string& HttpRequest::getBody() const {
	return _body;
}

size_t HttpRequest::getContentLength() const {
	return _contentLength;
}

bool HttpRequest::isChunked() const {
	return _chunked;
}

// Getters - State
HttpParseState HttpRequest::getState() const {
	return _state;
}

const std::string& HttpRequest::getErrorMessage() const {
	return _errorMessage;
}

// Getters - Derived info
std::string HttpRequest::getHost() const {
	std::string host = getHeader("host");
	// Remove port if present
	size_t colonPos = host.find(':');
	if (colonPos != std::string::npos) {
		return host.substr(0, colonPos);
	}
	return host;
}

int HttpRequest::getPort() const {
	std::string host = getHeader("host");
	size_t colonPos = host.find(':');
	if (colonPos != std::string::npos) {
		std::string portStr = host.substr(colonPos + 1);
		char* end = NULL;
		long port = std::strtol(portStr.c_str(), &end, 10);
		if (end != portStr.c_str() && *end == '\0' && port > 0 && port <= 65535) {
			return static_cast<int>(port);
		}
	}
	return 80;  // Default HTTP port
}

bool HttpRequest::isKeepAlive() const {
	std::string connection = toLower(getHeader("connection"));
	
	if (_httpVersion == "HTTP/1.1") {
		// HTTP/1.1 defaults to keep-alive
		return connection != "close";
	} else {
		// HTTP/1.0 defaults to close
		return connection == "keep-alive";
	}
}

// Set max body size
void HttpRequest::setMaxBodySize(size_t maxSize) {
	_maxBodySize = maxSize;
}