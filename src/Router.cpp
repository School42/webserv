#include "Router.hpp"
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <iostream>

// Constructor
Router::Router(const std::vector<ServerConfig>& servers)
	: _servers(servers) {}

// Destructor
Router::~Router() {}

// Main routing method
RouteResult Router::route(const HttpRequest& request, int listenPort) {
	RouteResult result;
	
	// Step 1: Find matching server
	std::string host = request.getHost();
	result.server = findServer(host, listenPort);
	
	if (!result.server) {
		result.matched = false;
		result.errorCode = 500;
		result.errorMessage = "No server configuration found";
		return result;
	}
	
	// Step 2: Decode and normalize the path
	std::string decodedPath = urlDecode(request.getPath());
	std::string normalizedPath = normalizePath(decodedPath);
	
	// Security check: path traversal attempt
	if (normalizedPath.find("..") != std::string::npos) {
		result.matched = false;
		result.errorCode = 403;
		result.errorMessage = "Forbidden: path traversal attempt";
		return result;
	}
	
	// Step 3: Find matching location
	result.location = findLocation(*result.server, normalizedPath);
	
	if (!result.location) {
		result.matched = false;
		result.errorCode = 404;
		result.errorMessage = "No matching location found";
		return result;
	}
	
	// Step 4: Check for redirect
	if (hasRedirect(*result.location)) {
		result.matched = true;
		// Caller will handle redirect
		return result;
	}
	
	// Step 5: Validate HTTP method
	if (!isMethodAllowed(*result.location, request.getMethod())) {
		result.matched = false;
		result.errorCode = 405;
		result.errorMessage = "Method Not Allowed";
		return result;
	}
	
	// Step 6: Resolve filesystem path
	result.resolvedPath = resolvePath(*result.location, normalizedPath);
	// Step 7: Check if CGI request
	// (pathInfo will be set by caller if needed)
	
	result.matched = true;
	return result;
}

// Find matching server by Host header and port
const ServerConfig* Router::findServer(const std::string& host, int port) const {
	const ServerConfig* defaultServer = NULL;
	
	for (size_t i = 0; i < _servers.size(); ++i) {
		const ServerConfig& server = _servers[i];
		const std::vector<ListenAddress>& addrs = server.getListenAddresses();
		
		// Check if server listens on this port
		bool listensOnPort = false;
		for (size_t j = 0; j < addrs.size(); ++j) {
			if (addrs[j].port == port) {
				listensOnPort = true;
				break;
			}
		}
		
		if (!listensOnPort) {
			continue;
		}
		
		// Remember first server on this port as default
		if (!defaultServer) {
			defaultServer = &server;
		}
		
		// Check if server_name matches
		if (matchServerName(server, host)) {
			return &server;
		}
	}
	
	// Return default server if no exact match
	return defaultServer;
}

// Check if host matches any server_name
bool Router::matchServerName(const ServerConfig& server, const std::string& host) const {
	const std::vector<std::string>& names = server.getServerNames();
	
	// Empty server_names means catch-all
	if (names.empty()) {
		return false;  // Don't match explicitly, use as fallback
	}
	
	// Case-insensitive comparison
	std::string lowerHost = host;
	for (size_t i = 0; i < lowerHost.size(); ++i) {
		lowerHost[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowerHost[i])));
	}
	
	for (size_t i = 0; i < names.size(); ++i) {
		std::string lowerName = names[i];
		for (size_t j = 0; j < lowerName.size(); ++j) {
			lowerName[j] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowerName[j])));
		}
		
		if (lowerHost == lowerName) {
			return true;
		}
		
		// Simple wildcard support: *.example.com
		if (lowerName.size() > 2 && lowerName[0] == '*' && lowerName[1] == '.') {
			std::string suffix = lowerName.substr(1);  // .example.com
			if (lowerHost.size() > suffix.size() &&
			    lowerHost.substr(lowerHost.size() - suffix.size()) == suffix) {
				return true;
			}
		}
	}
	
	return false;
}

// Find matching location (longest prefix match)
const LocationConfig* Router::findLocation(const ServerConfig& server, const std::string& path) const {
	const std::vector<LocationConfig>& locations = server.getLocations();
	const LocationConfig* bestMatch = NULL;
	size_t bestMatchLen = 0;
	
	for (size_t i = 0; i < locations.size(); ++i) {
		const std::string& locPath = locations[i].getPath();
		
		if (matchLocation(locPath, path)) {
			if (locPath.size() > bestMatchLen) {
				bestMatch = &locations[i];
				bestMatchLen = locPath.size();
			}
		}
	}
	
	return bestMatch;
}

// Check if request path matches location path (prefix match)
bool Router::matchLocation(const std::string& locationPath, const std::string& requestPath) const {
	// Exact match
	if (locationPath == requestPath) {
		return true;
	}
	
	// Prefix match: location must be prefix of request path
	// and either location ends with / or next char in request is /
	if (requestPath.size() > locationPath.size()) {
		if (requestPath.substr(0, locationPath.size()) == locationPath) {
			// Check boundary
			if (locationPath[locationPath.size() - 1] == '/') {
				return true;
			}
			if (requestPath[locationPath.size()] == '/') {
				return true;
			}
		}
	}
	
	// Special case: location "/" matches everything
	if (locationPath == "/") {
		return true;
	}
	
	return false;
}

// Validate method is allowed
bool Router::isMethodAllowed(const LocationConfig& location, const std::string& method) const {
	const std::vector<std::string>& allowed = location.getAllowedMethods();
	
	for (size_t i = 0; i < allowed.size(); ++i) {
		if (allowed[i] == method) {
			return true;
		}
	}
	
	return false;
}

// Resolve URI to filesystem path
std::string Router::resolvePath(const LocationConfig& location, const std::string& uri) const {
	std::string root = location.getRoot();
	std::string locPath = location.getPath();
	// Remove trailing slash from root if present
	if (!root.empty() && root[root.size() - 1] == '/') {
		root = root.substr(0, root.size() - 1);
	}
	
	// Get the part of URI after the location path
	std::string relativePath;
	if (uri.size() > locPath.size()) {
		relativePath = uri.substr(locPath.size());
	} else if (uri == locPath) {
		relativePath = "";
	} else {
		relativePath = uri;
	}
	
	// Ensure relativePath starts with /
	if (relativePath.empty() || relativePath[0] != '/') {
		relativePath = "/" + relativePath;
	}
	
	return root + relativePath;
}

// Check if location has redirect
bool Router::hasRedirect(const LocationConfig& location) const {
	return !location.getReturnCode().empty();
}

// Get redirect info
void Router::getRedirect(const LocationConfig& location, int& code, std::string& url) const {
	const std::string& codeStr = location.getReturnCode();
	code = std::atoi(codeStr.c_str());
	url = location.getReturnValue();
}

// Check if path matches CGI extension
bool Router::isCgiRequest(const LocationConfig& location, const std::string& path) const {
	const std::vector<std::string>& extensions = location.getCgiExtension();
	
	if (extensions.empty()) {
		return false;
	}
	
	for (size_t i = 0; i < extensions.size(); ++i) {
		const std::string& ext = extensions[i];
		
		if (path.size() >= ext.size()) {
			if (path.substr(path.size() - ext.size()) == ext) {
				return true;
			}
		}
	}
	
	return false;
}

// Normalize path (remove ., .., duplicate slashes)
std::string Router::normalizePath(const std::string& path) const {
	if (path.empty()) {
		return "/";
	}
	
	std::vector<std::string> segments;
	std::string segment;
	
	for (size_t i = 0; i < path.size(); ++i) {
		if (path[i] == '/') {
			if (!segment.empty()) {
				if (segment == "..") {
					if (!segments.empty()) {
						segments.pop_back();
					}
				} else if (segment != ".") {
					segments.push_back(segment);
				}
				segment.clear();
			}
		} else {
			segment += path[i];
		}
	}
	
	// Handle last segment
	if (!segment.empty()) {
		if (segment == "..") {
			if (!segments.empty()) {
				segments.pop_back();
			}
		} else if (segment != ".") {
			segments.push_back(segment);
		}
	}
	
	// Build result
	std::string result = "/";
	for (size_t i = 0; i < segments.size(); ++i) {
		result += segments[i];
		if (i < segments.size() - 1) {
			result += "/";
		}
	}
	
	// Preserve trailing slash if original had one
	if (path.size() > 1 && path[path.size() - 1] == '/' && result[result.size() - 1] != '/') {
		result += "/";
	}
	
	return result;
}

// URL decode
std::string Router::urlDecode(const std::string& str) const {
	std::string result;
	result.reserve(str.size());
	
	for (size_t i = 0; i < str.size(); ++i) {
		if (str[i] == '%' && i + 2 < str.size()) {
			// Hex decode
			char hex[3] = { str[i + 1], str[i + 2], '\0' };
			char* end = NULL;
			long value = std::strtol(hex, &end, 16);
			
			if (end == hex + 2 && value >= 0 && value <= 255) {
				result += static_cast<char>(value);
				i += 2;
			} else {
				result += str[i];
			}
		} else if (str[i] == '+') {
			result += ' ';
		} else {
			result += str[i];
		}
	}
	
	return result;
}