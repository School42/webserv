#include "Response.hpp"
#include <iostream>

// Constructor
Response::Response()
	: _statusCode(200),
	  _statusText("OK"),
	  _contentType("text/html"),
	  _body(""),
	  _keepAlive(true) {}

// Destructor
Response::~Response() {}

// Reset for reuse
void Response::reset() {
	_statusCode = 200;
	_statusText = "OK";
	_contentType = "text/html";
	_body.clear();
	_keepAlive = true;
	_headers.clear();
}

// Setters
void Response::setStatusCode(int code) {
	_statusCode = code;
}

void Response::setStatusText(const std::string& text) {
	_statusText = text;
}

void Response::setContentType(const std::string& type) {
	_contentType = type;
}

void Response::setBody(const std::string& body) {
	_body = body;
}

void Response::setKeepAlive(bool keepAlive) {
	_keepAlive = keepAlive;
}

// Add/set headers
void Response::setHeader(const std::string& name, const std::string& value) {
	_headers[name] = value;
}

void Response::addHeader(const std::string& name, const std::string& value) {
	// If header exists, append with comma (per HTTP spec for most headers)
	std::map<std::string, std::string>::iterator it = _headers.find(name);
	if (it != _headers.end()) {
		it->second += ", " + value;
	} else {
		_headers[name] = value;
	}
}

// Getters
int Response::getStatusCode() const {
	return _statusCode;
}

const std::string& Response::getStatusText() const {
	return _statusText;
}

const std::string& Response::getContentType() const {
	return _contentType;
}

const std::string& Response::getBody() const {
	return _body;
}

bool Response::isKeepAlive() const {
	return _keepAlive;
}

std::string Response::getHeader(const std::string& name) const {
	std::map<std::string, std::string>::const_iterator it = _headers.find(name);
	if (it != _headers.end()) {
		return it->second;
	}
	return "";
}

const std::map<std::string, std::string>& Response::getHeaders() const {
	return _headers;
}

// Build the complete HTTP response string
std::string Response::build() const {
	std::stringstream response;
	
	// Status line
	response << "HTTP/1.1 " << _statusCode << " " << _statusText << "\r\n";
	
	// Content-Type header
	response << "Content-Type: " << _contentType << "\r\n";
	
	// Content-Length header
	response << "Content-Length: " << _body.size() << "\r\n";
	
	// Connection header
	if (_keepAlive) {
		response << "Connection: keep-alive\r\n";
	} else {
		response << "Connection: close\r\n";
	}
	
	// Additional headers
	for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
	     it != _headers.end(); ++it) {
		response << it->first << ": " << it->second << "\r\n";
	}
	
	// Empty line to end headers
	response << "\r\n";
	
	// Body
	response << _body;
	
	return response.str();
}

// Static: Get status text for code
std::string Response::getStatusTextForCode(int code) {
	switch (code) {
		// 1xx Informational
		case 100: return "Continue";
		case 101: return "Switching Protocols";
		
		// 2xx Success
		case 200: return "OK";
		case 201: return "Created";
		case 202: return "Accepted";
		case 204: return "No Content";
		case 206: return "Partial Content";
		
		// 3xx Redirection
		case 300: return "Multiple Choices";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 303: return "See Other";
		case 304: return "Not Modified";
		case 307: return "Temporary Redirect";
		case 308: return "Permanent Redirect";
		
		// 4xx Client Errors
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 406: return "Not Acceptable";
		case 408: return "Request Timeout";
		case 409: return "Conflict";
		case 410: return "Gone";
		case 411: return "Length Required";
		case 413: return "Payload Too Large";
		case 414: return "URI Too Long";
		case 415: return "Unsupported Media Type";
		case 418: return "I'm a teapot";
		case 429: return "Too Many Requests";
		
		// 5xx Server Errors
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 504: return "Gateway Timeout";
		case 505: return "HTTP Version Not Supported";
		
		default: return "Unknown";
	}
}

// Static: Build error page HTML
std::string Response::buildErrorPageHtml(int code, const std::string& statusText,
                                          const std::string& message) {
	std::stringstream html;
	
	html << "<!DOCTYPE html>\n"
	     << "<html>\n"
	     << "<head>\n"
	     << "  <meta charset=\"UTF-8\">\n"
	     << "  <title>" << code << " " << statusText << "</title>\n"
	     << "  <style>\n"
	     << "    body {\n"
	     << "      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\n"
	     << "      background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);\n"
	     << "      min-height: 100vh;\n"
	     << "      display: flex;\n"
	     << "      justify-content: center;\n"
	     << "      align-items: center;\n"
	     << "      margin: 0;\n"
	     << "      color: #fff;\n"
	     << "    }\n"
	     << "    .container {\n"
	     << "      text-align: center;\n"
	     << "      padding: 40px;\n"
	     << "    }\n"
	     << "    .error-code {\n"
	     << "      font-size: 8em;\n"
	     << "      font-weight: 700;\n"
	     << "      background: linear-gradient(90deg, #f44336, #e91e63);\n"
	     << "      -webkit-background-clip: text;\n"
	     << "      -webkit-text-fill-color: transparent;\n"
	     << "      background-clip: text;\n"
	     << "      line-height: 1;\n"
	     << "    }\n"
	     << "    h1 {\n"
	     << "      font-size: 1.8em;\n"
	     << "      margin: 20px 0;\n"
	     << "    }\n"
	     << "    p {\n"
	     << "      color: #888;\n"
	     << "      margin-bottom: 30px;\n"
	     << "    }\n"
	     << "    a {\n"
	     << "      display: inline-block;\n"
	     << "      padding: 12px 30px;\n"
	     << "      background: linear-gradient(90deg, #00d4ff, #7b2ff7);\n"
	     << "      color: white;\n"
	     << "      text-decoration: none;\n"
	     << "      border-radius: 8px;\n"
	     << "    }\n"
	     << "    a:hover { opacity: 0.9; }\n"
	     << "    .footer { margin-top: 40px; color: #555; font-size: 0.9em; }\n"
	     << "  </style>\n"
	     << "</head>\n"
	     << "<body>\n"
	     << "  <div class=\"container\">\n"
	     << "    <div class=\"error-code\">" << code << "</div>\n"
	     << "    <h1>" << statusText << "</h1>\n"
	     << "    <p>" << message << "</p>\n"
	     << "    <a href=\"/\">Go Home</a>\n"
	     << "    <div class=\"footer\">webserv</div>\n"
	     << "  </div>\n"
	     << "</body>\n"
	     << "</html>\n";
	
	return html.str();
}

// Static factory: OK response
Response Response::ok(const std::string& body, const std::string& contentType) {
	Response resp;
	resp.setStatusCode(200);
	resp.setStatusText("OK");
	resp.setContentType(contentType);
	resp.setBody(body);
	return resp;
}

// Static factory: Created response
Response Response::created(const std::string& body, const std::string& contentType) {
	Response resp;
	resp.setStatusCode(201);
	resp.setStatusText("Created");
	resp.setContentType(contentType);
	resp.setBody(body);
	return resp;
}

// Static factory: Redirect response
Response Response::redirect(int code, const std::string& location) {
	Response resp;
	resp.setStatusCode(code);
	resp.setStatusText(getStatusTextForCode(code));
	resp.setHeader("Location", location);
	resp.setKeepAlive(false);
	
	// Build redirect body
	std::stringstream body;
	body << "<!DOCTYPE html>\n"
	     << "<html>\n"
	     << "<head>\n"
	     << "  <title>" << code << " " << resp.getStatusText() << "</title>\n"
	     << "  <meta http-equiv=\"refresh\" content=\"0;url=" << location << "\">\n"
	     << "</head>\n"
	     << "<body>\n"
	     << "  <h1>" << code << " " << resp.getStatusText() << "</h1>\n"
	     << "  <p>Redirecting to <a href=\"" << location << "\">" << location << "</a></p>\n"
	     << "</body>\n"
	     << "</html>\n";
	
	resp.setContentType("text/html");
	resp.setBody(body.str());
	
	return resp;
}

// Static factory: Error response
Response Response::error(int code, const std::string& message) {
	Response resp;
	std::string statusText = getStatusTextForCode(code);
	
	resp.setStatusCode(code);
	resp.setStatusText(statusText);
	resp.setContentType("text/html");
	resp.setBody(buildErrorPageHtml(code, statusText, message));
	resp.setKeepAlive(false);
	return resp;
}