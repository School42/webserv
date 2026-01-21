#pragma once
#include <string>
#include <map>
#include <vector>
#include "ServerConfig.hpp"
#include "LocationConfig.hpp"
#include "HttpRequest.hpp"
#include "Router.hpp"

// File serving result
struct FileResult {
	bool success;
	int statusCode;
	std::string statusText;
	std::string contentType;
	std::string body;
	std::string errorMessage;
	bool isDirectory;
	std::string redirectPath;  // For directory without trailing slash
	
	FileResult()
		: success(false),
		  statusCode(500),
		  statusText("Internal Server Error"),
		  contentType("text/html"),
		  body(""),
		  errorMessage(""),
		  isDirectory(false),
		  redirectPath("") {}
};

class FileServer {
public:
	// Constructor
	FileServer();
	
	// Destructor
	~FileServer();
	
	// Main file serving method
	FileResult serveFile(const HttpRequest& request, const RouteResult& route);
	
	// Serve a specific file path
	FileResult serveFilePath(const std::string& filePath);
	
	// Serve custom error page
	FileResult serveErrorPage(const ServerConfig& server, int errorCode);
	
	// Generate directory listing
	FileResult generateDirectoryListing(const std::string& dirPath, 
	                                     const std::string& requestUri);
	
	// MIME type lookup
	std::string getMimeType(const std::string& filePath) const;
	
	// Check if file exists and is readable
	bool fileExists(const std::string& path) const;
	bool isDirectory(const std::string& path) const;
	bool isReadable(const std::string& path) const;
	
	// Get file size
	size_t getFileSize(const std::string& path) const;
	
	// Get file modification time (for caching headers)
	std::string getLastModified(const std::string& path) const;
	
	// Delete file
	FileResult deleteFile(const HttpRequest& request, const RouteResult& route);

private:
	// Non-copyable
	FileServer(const FileServer& other);
	FileServer& operator=(const FileServer& rhs);
	
	// Initialize MIME types map
	void initMimeTypes();
	
	// Read file content
	bool readFile(const std::string& path, std::string& content) const;
	
	// Try to find index file in directory
	std::string findIndexFile(const std::string& dirPath, 
	                          const std::vector<std::string>& indexFiles) const;
	
	// HTML escape for directory listing
	std::string htmlEscape(const std::string& str) const;
	
	// Format file size for directory listing
	std::string formatSize(size_t size) const;
	
	// Format time for directory listing
	std::string formatTime(time_t time) const;
	
	// Get status text from code
	std::string getStatusText(int code) const;
	
	// Generate default error page
	std::string generateErrorPage(int code, const std::string& message) const;
	
	// MIME types map
	std::map<std::string, std::string> _mimeTypes;
	
	// Maximum file size to serve (to prevent memory issues)
	static const size_t MAX_FILE_SIZE = 100 * 1024 * 1024;  // 100MB
};