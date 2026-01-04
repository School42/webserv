#pragma once
#include <string>
#include <map>
#include <sstream>

class Response {
public:
	// Constructor
	Response();
	
	// Destructor
	~Response();
	
	// Reset for reuse
	void reset();
	
	// Setters
	void setStatusCode(int code);
	void setStatusText(const std::string& text);
	void setContentType(const std::string& type);
	void setBody(const std::string& body);
	void setKeepAlive(bool keepAlive);
	
	// Add/set headers
	void setHeader(const std::string& name, const std::string& value);
	void addHeader(const std::string& name, const std::string& value);
	
	// Getters
	int getStatusCode() const;
	const std::string& getStatusText() const;
	const std::string& getContentType() const;
	const std::string& getBody() const;
	bool isKeepAlive() const;
	std::string getHeader(const std::string& name) const;
	const std::map<std::string, std::string>& getHeaders() const;
	
	// Build the complete HTTP response string
	std::string build() const;
	
	// Static factory methods for common responses
	static Response ok(const std::string& body, const std::string& contentType = "text/html");
	static Response created(const std::string& body, const std::string& contentType = "text/html");
	static Response redirect(int code, const std::string& location);
	static Response error(int code, const std::string& message);
	
	// Static helper to get status text from code
	static std::string getStatusTextForCode(int code);
	
	// Static helper to build error page HTML
	static std::string buildErrorPageHtml(int code, const std::string& statusText, 
	                                       const std::string& message);

private:
	int _statusCode;
	std::string _statusText;
	std::string _contentType;
	std::string _body;
	bool _keepAlive;
	std::map<std::string, std::string> _headers;
};