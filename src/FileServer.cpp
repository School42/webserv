#include "FileServer.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <ctime>
#include <algorithm>
#include <cerrno>
#include <cstring>

// Constructor
FileServer::FileServer() {
	initMimeTypes();
}

// Destructor
FileServer::~FileServer() {}

// Initialize MIME types
void FileServer::initMimeTypes() {
	// Text types
	_mimeTypes["html"] = "text/html";
	_mimeTypes["htm"] = "text/html";
	_mimeTypes["css"] = "text/css";
	_mimeTypes["js"] = "text/javascript";
	_mimeTypes["json"] = "application/json";
	_mimeTypes["xml"] = "application/xml";
	_mimeTypes["txt"] = "text/plain";
	_mimeTypes["csv"] = "text/csv";
	_mimeTypes["md"] = "text/markdown";
	
	// Image types
	_mimeTypes["png"] = "image/png";
	_mimeTypes["jpg"] = "image/jpeg";
	_mimeTypes["jpeg"] = "image/jpeg";
	_mimeTypes["gif"] = "image/gif";
	_mimeTypes["ico"] = "image/x-icon";
	_mimeTypes["svg"] = "image/svg+xml";
	_mimeTypes["webp"] = "image/webp";
	_mimeTypes["bmp"] = "image/bmp";
	
	// Audio types
	_mimeTypes["mp3"] = "audio/mpeg";
	_mimeTypes["wav"] = "audio/wav";
	_mimeTypes["ogg"] = "audio/ogg";
	_mimeTypes["flac"] = "audio/flac";
	
	// Video types
	_mimeTypes["mp4"] = "video/mp4";
	_mimeTypes["webm"] = "video/webm";
	_mimeTypes["avi"] = "video/x-msvideo";
	_mimeTypes["mov"] = "video/quicktime";
	_mimeTypes["mkv"] = "video/x-matroska";
	
	// Application types
	_mimeTypes["pdf"] = "application/pdf";
	_mimeTypes["zip"] = "application/zip";
	_mimeTypes["gz"] = "application/gzip";
	_mimeTypes["tar"] = "application/x-tar";
	_mimeTypes["rar"] = "application/vnd.rar";
	_mimeTypes["7z"] = "application/x-7z-compressed";
	_mimeTypes["doc"] = "application/msword";
	_mimeTypes["docx"] = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
	_mimeTypes["xls"] = "application/vnd.ms-excel";
	_mimeTypes["xlsx"] = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
	_mimeTypes["ppt"] = "application/vnd.ms-powerpoint";
	_mimeTypes["pptx"] = "application/vnd.openxmlformats-officedocument.presentationml.presentation";
	
	// Font types
	_mimeTypes["woff"] = "font/woff";
	_mimeTypes["woff2"] = "font/woff2";
	_mimeTypes["ttf"] = "font/ttf";
	_mimeTypes["otf"] = "font/otf";
	_mimeTypes["eot"] = "application/vnd.ms-fontobject";
	
	// Other
	_mimeTypes["wasm"] = "application/wasm";
	_mimeTypes["bin"] = "application/octet-stream";
}

// Get MIME type for file
std::string FileServer::getMimeType(const std::string& filePath) const {
	// Find last dot in filename
	size_t dotPos = filePath.rfind('.');
	if (dotPos == std::string::npos || dotPos == filePath.size() - 1) {
		return "application/octet-stream";
	}
	
	// Extract extension (lowercase)
	std::string ext = filePath.substr(dotPos + 1);
	for (size_t i = 0; i < ext.size(); ++i) {
		ext[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[i])));
	}
	
	// Look up in map
	std::map<std::string, std::string>::const_iterator it = _mimeTypes.find(ext);
	if (it != _mimeTypes.end()) {
		return it->second;
	}
	
	return "application/octet-stream";
}

// Check if file exists
bool FileServer::fileExists(const std::string& path) const {
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

// Check if path is a directory
bool FileServer::isDirectory(const std::string& path) const {
	struct stat st;
	if (stat(path.c_str(), &st) != 0) {
		return false;
	}
	return S_ISDIR(st.st_mode);
}

// Check if file is readable
bool FileServer::isReadable(const std::string& path) const {
	struct stat st;
	if (stat(path.c_str(), &st) != 0) {
		return false;
	}
	// Check if current process can read
	return (st.st_mode & S_IRUSR) || (st.st_mode & S_IRGRP) || (st.st_mode & S_IROTH);
}

// Get file size
size_t FileServer::getFileSize(const std::string& path) const {
	struct stat st;
	if (stat(path.c_str(), &st) != 0) {
		return 0;
	}
	return static_cast<size_t>(st.st_size);
}

// Get last modified time formatted for HTTP
std::string FileServer::getLastModified(const std::string& path) const {
	struct stat st;
	if (stat(path.c_str(), &st) != 0) {
		return "";
	}
	
	char buf[128];
	struct tm* tm_info = gmtime(&st.st_mtime);
	strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", tm_info);
	return std::string(buf);
}

// Read file content
bool FileServer::readFile(const std::string& path, std::string& content) const {
	std::ifstream file(path.c_str(), std::ios::binary);
	if (!file) {
		return false;
	}
	
	// Get file size
	file.seekg(0, std::ios::end);
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	
	// Check size limit
	if (static_cast<size_t>(size) > MAX_FILE_SIZE) {
		return false;
	}
	
	// Read content
	content.resize(static_cast<size_t>(size));
	if (size > 0) {
		file.read(&content[0], size);
	}
	
	return file.good() || file.eof();
}

// Find index file in directory
std::string FileServer::findIndexFile(const std::string& dirPath, 
                                       const std::vector<std::string>& indexFiles) const {
	for (size_t i = 0; i < indexFiles.size(); ++i) {
		std::string indexPath = dirPath;
		if (!indexPath.empty() && indexPath[indexPath.size() - 1] != '/') {
			indexPath += "/";
		}
		indexPath += indexFiles[i];
		
		if (fileExists(indexPath) && !isDirectory(indexPath)) {
			return indexPath;
		}
	}
	return "";
}

// Main file serving method
FileResult FileServer::serveFile(const HttpRequest& request, const RouteResult& route) {
	FileResult result;
	
	if (!route.matched || !route.location) {
		result.statusCode = 500;
		result.statusText = "Internal Server Error";
		result.errorMessage = "Invalid route result";
		result.body = generateErrorPage(500, result.errorMessage);
		return result;
	}
	
	std::string filePath = route.resolvedPath;
	
	// Check if path exists
	if (!fileExists(filePath)) {
		result.statusCode = 404;
		result.statusText = "Not Found";
		result.errorMessage = "File not found: " + request.getPath();
		result.body = generateErrorPage(404, result.errorMessage);
		return result;
	}
	
	// Handle directory
	if (isDirectory(filePath)) {
		result.isDirectory = true;
		
		// Check if URI ends with slash
		std::string uri = request.getPath();
		if (uri.empty() || uri[uri.size() - 1] != '/') {
			// Redirect to add trailing slash
			result.statusCode = 301;
			result.statusText = "Moved Permanently";
			result.redirectPath = uri + "/";
			return result;
		}
		
		// Try to find index file
		const std::vector<std::string>& indexFiles = route.location->getIndex();
		std::string indexPath = findIndexFile(filePath, indexFiles);
		
		if (!indexPath.empty()) {
			// Serve index file
			filePath = indexPath;
		} else if (route.location->getAutoIndex()) {
			// Generate directory listing
			return generateDirectoryListing(filePath, uri);
		} else {
			// No index file and autoindex off = 403 Forbidden
			result.statusCode = 403;
			result.statusText = "Forbidden";
			result.errorMessage = "Directory listing not allowed";
			result.body = generateErrorPage(403, result.errorMessage);
			return result;
		}
	}
	
	// Check if file is readable
	if (!isReadable(filePath)) {
		result.statusCode = 403;
		result.statusText = "Forbidden";
		result.errorMessage = "Permission denied";
		result.body = generateErrorPage(403, result.errorMessage);
		return result;
	}
	
	// Check file size
	size_t fileSize = getFileSize(filePath);
	if (fileSize > MAX_FILE_SIZE) {
		result.statusCode = 413;
		result.statusText = "Payload Too Large";
		result.errorMessage = "File too large to serve";
		result.body = generateErrorPage(413, result.errorMessage);
		return result;
	}
	
	// Read file
	std::string content;
	if (!readFile(filePath, content)) {
		result.statusCode = 500;
		result.statusText = "Internal Server Error";
		result.errorMessage = "Failed to read file";
		result.body = generateErrorPage(500, result.errorMessage);
		return result;
	}
	
	// Success!
	result.success = true;
	result.statusCode = 200;
	result.statusText = "OK";
	result.contentType = getMimeType(filePath);
	result.body = content;
	
	return result;
}

// Serve a specific file path (for error pages, etc.)
FileResult FileServer::serveFilePath(const std::string& filePath) {
	FileResult result;
	
	if (!fileExists(filePath)) {
		result.statusCode = 404;
		result.statusText = "Not Found";
		result.errorMessage = "File not found";
		result.body = generateErrorPage(404, result.errorMessage);
		return result;
	}
	
	if (isDirectory(filePath)) {
		result.statusCode = 403;
		result.statusText = "Forbidden";
		result.errorMessage = "Cannot serve directory";
		result.body = generateErrorPage(403, result.errorMessage);
		return result;
	}
	
	if (!isReadable(filePath)) {
		result.statusCode = 403;
		result.statusText = "Forbidden";
		result.errorMessage = "Permission denied";
		result.body = generateErrorPage(403, result.errorMessage);
		return result;
	}
	
	std::string content;
	if (!readFile(filePath, content)) {
		result.statusCode = 500;
		result.statusText = "Internal Server Error";
		result.errorMessage = "Failed to read file";
		result.body = generateErrorPage(500, result.errorMessage);
		return result;
	}
	
	result.success = true;
	result.statusCode = 200;
	result.statusText = "OK";
	result.contentType = getMimeType(filePath);
	result.body = content;
	
	return result;
}

// Serve custom error page
FileResult FileServer::serveErrorPage(const ServerConfig& server, int errorCode) {
	FileResult result;
	
	const std::map<int, std::string>& errorPages = server.getErrorPages();
	std::map<int, std::string>::const_iterator it = errorPages.find(errorCode);
	
	if (it != errorPages.end()) {
		// Custom error page configured
		std::string errorPagePath = it->second;
		
		// If path is relative, prepend root
		if (!errorPagePath.empty() && errorPagePath[0] != '/') {
			if (server.hasRoot()) {
				errorPagePath = server.getRoot() + "/" + errorPagePath;
			}
		} else if (server.hasRoot()) {
			// Absolute path within server root
			errorPagePath = server.getRoot() + errorPagePath;
		}
		
		// Try to read custom error page
		std::string content;
		if (fileExists(errorPagePath) && !isDirectory(errorPagePath) && 
		    isReadable(errorPagePath) && readFile(errorPagePath, content)) {
			result.success = true;
			result.statusCode = errorCode;
			result.statusText = getStatusText(errorCode);
			result.contentType = getMimeType(errorPagePath);
			result.body = content;
			return result;
		}
	}
	
	// Fall back to default error page
	result.success = true;
	result.statusCode = errorCode;
	result.statusText = getStatusText(errorCode);
	result.contentType = "text/html";
	result.body = generateErrorPage(errorCode, getStatusText(errorCode));
	
	return result;
}

// Generate directory listing
FileResult FileServer::generateDirectoryListing(const std::string& dirPath, 
                                                 const std::string& requestUri) {
	FileResult result;
	
	DIR* dir = opendir(dirPath.c_str());
	if (!dir) {
		result.statusCode = 500;
		result.statusText = "Internal Server Error";
		result.errorMessage = "Failed to open directory";
		result.body = generateErrorPage(500, result.errorMessage);
		return result;
	}
	
	// Collect directory entries
	std::vector<std::string> entries;
	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL) {
		std::string name = entry->d_name;
		// Skip . but keep ..
		if (name == ".") {
			continue;
		}
		entries.push_back(name);
	}
	closedir(dir);
	
	// Sort entries (directories first, then alphabetically)
	std::sort(entries.begin(), entries.end());
	
	// Generate HTML
	std::stringstream html;
	html << "<!DOCTYPE html>\n"
	     << "<html>\n"
	     << "<head>\n"
	     << "  <meta charset=\"UTF-8\">\n"
	     << "  <title>Index of " << htmlEscape(requestUri) << "</title>\n"
	     << "  <style>\n"
	     << "    body { font-family: monospace; margin: 20px; }\n"
	     << "    h1 { border-bottom: 1px solid #ccc; padding-bottom: 10px; }\n"
	     << "    table { border-collapse: collapse; width: 100%; }\n"
	     << "    th, td { text-align: left; padding: 8px; }\n"
	     << "    th { background-color: #f0f0f0; }\n"
	     << "    tr:nth-child(even) { background-color: #f9f9f9; }\n"
	     << "    tr:hover { background-color: #e0e0e0; }\n"
	     << "    a { text-decoration: none; color: #0066cc; }\n"
	     << "    a:hover { text-decoration: underline; }\n"
	     << "    .dir { font-weight: bold; }\n"
	     << "    .size { text-align: right; }\n"
	     << "  </style>\n"
	     << "</head>\n"
	     << "<body>\n"
	     << "  <h1>Index of " << htmlEscape(requestUri) << "</h1>\n"
	     << "  <table>\n"
	     << "    <tr><th>Name</th><th>Size</th><th>Last Modified</th></tr>\n";
	
	for (size_t i = 0; i < entries.size(); ++i) {
		std::string name = entries[i];
		std::string fullPath = dirPath;
		if (!fullPath.empty() && fullPath[fullPath.size() - 1] != '/') {
			fullPath += "/";
		}
		fullPath += name;
		
		bool isDir = isDirectory(fullPath);
		std::string displayName = name;
		std::string link = name;
		
		if (isDir) {
			displayName += "/";
			link += "/";
		}
		
		// Get file info
		struct stat st;
		std::string sizeStr = "-";
		std::string timeStr = "-";
		
		if (stat(fullPath.c_str(), &st) == 0) {
			if (!isDir) {
				sizeStr = formatSize(static_cast<size_t>(st.st_size));
			}
			timeStr = formatTime(st.st_mtime);
		}
		
		html << "    <tr>\n"
		     << "      <td><a href=\"" << htmlEscape(link) << "\"" 
		     << (isDir ? " class=\"dir\"" : "") << ">" 
		     << htmlEscape(displayName) << "</a></td>\n"
		     << "      <td class=\"size\">" << sizeStr << "</td>\n"
		     << "      <td>" << timeStr << "</td>\n"
		     << "    </tr>\n";
	}
	
	html << "  </table>\n"
	     << "  <hr>\n"
	     << "  <p><em>webserv</em></p>\n"
	     << "</body>\n"
	     << "</html>\n";
	
	result.success = true;
	result.statusCode = 200;
	result.statusText = "OK";
	result.contentType = "text/html";
	result.body = html.str();
	result.isDirectory = true;
	
	return result;
}

// HTML escape
std::string FileServer::htmlEscape(const std::string& str) const {
	std::string result;
	result.reserve(str.size() * 1.1);
	
	for (size_t i = 0; i < str.size(); ++i) {
		switch (str[i]) {
			case '&':  result += "&amp;";  break;
			case '<':  result += "&lt;";   break;
			case '>':  result += "&gt;";   break;
			case '"':  result += "&quot;"; break;
			case '\'': result += "&#39;";  break;
			default:   result += str[i];   break;
		}
	}
	
	return result;
}

// Format file size
std::string FileServer::formatSize(size_t size) const {
	std::stringstream ss;
	
	if (size < 1024) {
		ss << size << " B";
	} else if (size < 1024 * 1024) {
		ss << (size / 1024) << " KB";
	} else if (size < 1024 * 1024 * 1024) {
		ss << (size / (1024 * 1024)) << " MB";
	} else {
		ss << (size / (1024 * 1024 * 1024)) << " GB";
	}
	
	return ss.str();
}

// Format time
std::string FileServer::formatTime(time_t time) const {
	char buf[64];
	struct tm* tm_info = localtime(&time);
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm_info);
	return std::string(buf);
}

// Get status text from code
std::string FileServer::getStatusText(int code) const {
	switch (code) {
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 303: return "See Other";
		case 304: return "Not Modified";
		case 307: return "Temporary Redirect";
		case 308: return "Permanent Redirect";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 408: return "Request Timeout";
		case 409: return "Conflict";
		case 410: return "Gone";
		case 411: return "Length Required";
		case 413: return "Payload Too Large";
		case 414: return "URI Too Long";
		case 415: return "Unsupported Media Type";
		case 429: return "Too Many Requests";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 504: return "Gateway Timeout";
		case 505: return "HTTP Version Not Supported";
		default:  return "Unknown";
	}
}

// Generate default error page
std::string FileServer::generateErrorPage(int code, const std::string& message) const {
	std::string statusText = getStatusText(code);
	
	std::stringstream html;
	html << "<!DOCTYPE html>\n"
	     << "<html>\n"
	     << "<head>\n"
	     << "  <meta charset=\"UTF-8\">\n"
	     << "  <title>" << code << " " << statusText << "</title>\n"
	     << "  <style>\n"
	     << "    body {\n"
	     << "      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\n"
	     << "      display: flex;\n"
	     << "      justify-content: center;\n"
	     << "      align-items: center;\n"
	     << "      min-height: 100vh;\n"
	     << "      margin: 0;\n"
	     << "      background-color: #f5f5f5;\n"
	     << "    }\n"
	     << "    .container {\n"
	     << "      text-align: center;\n"
	     << "      padding: 40px;\n"
	     << "      background: white;\n"
	     << "      border-radius: 8px;\n"
	     << "      box-shadow: 0 2px 10px rgba(0,0,0,0.1);\n"
	     << "    }\n"
	     << "    h1 {\n"
	     << "      font-size: 72px;\n"
	     << "      margin: 0;\n"
	     << "      color: #333;\n"
	     << "    }\n"
	     << "    h2 {\n"
	     << "      color: #666;\n"
	     << "      margin: 10px 0 20px;\n"
	     << "    }\n"
	     << "    p {\n"
	     << "      color: #888;\n"
	     << "      margin: 0;\n"
	     << "    }\n"
	     << "    hr {\n"
	     << "      border: none;\n"
	     << "      border-top: 1px solid #eee;\n"
	     << "      margin: 20px 0;\n"
	     << "    }\n"
	     << "    .server {\n"
	     << "      color: #aaa;\n"
	     << "      font-size: 12px;\n"
	     << "    }\n"
	     << "  </style>\n"
	     << "</head>\n"
	     << "<body>\n"
	     << "  <div class=\"container\">\n"
	     << "    <h1>" << code << "</h1>\n"
	     << "    <h2>" << htmlEscape(statusText) << "</h2>\n"
	     << "    <p>" << htmlEscape(message) << "</p>\n"
	     << "    <hr>\n"
	     << "    <p class=\"server\">webserv</p>\n"
	     << "  </div>\n"
	     << "</body>\n"
	     << "</html>\n";
	
	return html.str();
}