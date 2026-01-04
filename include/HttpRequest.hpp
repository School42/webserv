#pragma once
#include <string>
#include <map>
#include <vector>
#include <sys/types.h>

// HTTP parsing states
enum HttpParseState {
	PARSE_REQUEST_LINE,
	PARSE_HEADERS,
	PARSE_BODY,
	PARSE_CHUNKED_SIZE,
	PARSE_CHUNKED_DATA,
	PARSE_CHUNKED_TRAILER,
	PARSE_COMPLETE,
	PARSE_ERROR
};

// HTTP parsing result
enum HttpParseResult {
	PARSE_INCOMPLETE,    // Need more data
	PARSE_SUCCESS,       // Request fully parsed
	PARSE_FAILED         // Parse error
};

class HttpRequest {
public:
	// Constructor
	HttpRequest();
	
	// Destructor
	~HttpRequest();
	
	// Main parsing method - call repeatedly as data arrives
	// Returns PARSE_INCOMPLETE if more data needed
	// Returns PARSE_SUCCESS when request is complete
	// Returns PARSE_FAILED on error
	HttpParseResult parse(const std::string& data, size_t& bytesConsumed);
	
	// Reset for new request (keep-alive)
	void reset();
	
	// Getters - Request line
	const std::string& getMethod() const;
	const std::string& getUri() const;
	const std::string& getPath() const;          // URI without query string
	const std::string& getQueryString() const;   // Query string (after ?)
	const std::string& getHttpVersion() const;
	
	// Getters - Headers (case-insensitive lookup)
	std::string getHeader(const std::string& name) const;
	const std::map<std::string, std::string>& getHeaders() const;
	bool hasHeader(const std::string& name) const;
	
	// Getters - Body
	const std::string& getBody() const;
	size_t getContentLength() const;
	bool isChunked() const;
	
	// Getters - State
	HttpParseState getState() const;
	const std::string& getErrorMessage() const;
	
	// Getters - Derived info
	std::string getHost() const;
	int getPort() const;            // From Host header, or 80 by default
	bool isKeepAlive() const;       // Based on Connection header and HTTP version
	
	// Set max body size (for validation)
	void setMaxBodySize(size_t maxSize);

private:
	// Non-copyable
	HttpRequest(const HttpRequest& other);
	HttpRequest& operator=(const HttpRequest& rhs);
	
	// Parsing helpers
	bool parseRequestLine(const std::string& line);
	bool parseHeader(const std::string& line);
	bool parseChunkedSize(const std::string& line);
	void parseUri();
	
	// Case-insensitive string comparison
	static std::string toLower(const std::string& str);
	
	// Request line components
	std::string _method;
	std::string _uri;
	std::string _path;
	std::string _queryString;
	std::string _httpVersion;
	
	// Headers (stored lowercase for case-insensitive lookup)
	std::map<std::string, std::string> _headers;
	
	// Body
	std::string _body;
	size_t _contentLength;
	bool _chunked;
	size_t _currentChunkSize;
	size_t _currentChunkRead;
	
	// State
	HttpParseState _state;
	std::string _errorMessage;
	
	// Limits
	size_t _maxBodySize;
	static const size_t MAX_REQUEST_LINE = 8192;
	static const size_t MAX_HEADER_SIZE = 8192;
	static const size_t MAX_HEADERS_COUNT = 100;
};