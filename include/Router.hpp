#pragma once
#include <vector>
#include <string>
#include "ServerConfig.hpp"
#include "LocationConfig.hpp"
#include "HttpRequest.hpp"

// Routing result structure
struct RouteResult {
	bool matched;
	const ServerConfig* server;
	const LocationConfig* location;
	std::string errorMessage;
	int errorCode;  // HTTP error code if not matched
	
	// Resolved path info
	std::string resolvedPath;    // Full filesystem path
	std::string pathInfo;        // Extra path info (for CGI)
	
	RouteResult()
		: matched(false),
		  server(NULL),
		  location(NULL),
		  errorMessage(""),
		  errorCode(0),
		  resolvedPath(""),
		  pathInfo("") {}
};

class Router {
public:
	// Constructor
	explicit Router(const std::vector<ServerConfig>& servers);
	
	// Destructor
	~Router();
	
	// Main routing method
	RouteResult route(const HttpRequest& request, int listenPort);
	
	// Find matching server by Host header and port
	const ServerConfig* findServer(const std::string& host, int port) const;
	
	// Find matching location within a server
	const LocationConfig* findLocation(const ServerConfig& server, const std::string& path) const;
	
	// Validate method is allowed for location
	bool isMethodAllowed(const LocationConfig& location, const std::string& method) const;
	
	// Resolve URI to filesystem path
	std::string resolvePath(const LocationConfig& location, const std::string& uri) const;
	
	// Check if location has redirect
	bool hasRedirect(const LocationConfig& location) const;
	
	// Get redirect info
	void getRedirect(const LocationConfig& location, int& code, std::string& url) const;
	
	// Check if path matches CGI extension
	bool isCgiRequest(const LocationConfig& location, const std::string& path) const;

private:
	// Non-copyable
	Router(const Router& other);
	Router& operator=(const Router& rhs);
	
	// Reference to server configurations
	const std::vector<ServerConfig>& _servers;
	
	// Helper: check if host matches server_name
	bool matchServerName(const ServerConfig& server, const std::string& host) const;
	
	// Helper: check if path matches location (prefix match)
	bool matchLocation(const std::string& locationPath, const std::string& requestPath) const;
	
	// Helper: normalize path (remove .., etc)
	std::string normalizePath(const std::string& path) const;
	
	// Helper: decode URL-encoded characters
	std::string urlDecode(const std::string& str) const;
};