#pragma once
#include <string>
#include <vector>
#include <map>
#include "HttpRequest.hpp"
#include "LocationConfig.hpp"
#include "Router.hpp"

// Represents a single uploaded file
struct UploadedFile {
	std::string filename;       // Original filename from client
	std::string savedPath;      // Path where file was saved
	std::string contentType;    // MIME type
	size_t size;                // File size in bytes
	
	UploadedFile()
		: filename(""),
		  savedPath(""),
		  contentType("application/octet-stream"),
		  size(0) {}
};

// Upload result
struct UploadResult {
	bool success;
	int statusCode;
	std::string statusText;
	std::string errorMessage;
	std::vector<UploadedFile> files;  // Successfully uploaded files
	
	UploadResult()
		: success(false),
		  statusCode(500),
		  statusText("Internal Server Error"),
		  errorMessage("") {}
};

// Multipart form data part
struct MultipartPart {
	std::map<std::string, std::string> headers;
	std::string name;           // Form field name
	std::string filename;       // Filename (if file upload)
	std::string contentType;    // Content-Type of this part
	std::string data;           // Part data/content
	bool isFile;                // True if this part is a file upload
	
	MultipartPart()
		: contentType("text/plain"),
		  isFile(false) {}
};

class UploadHandler {
public:
	// Constructor
	UploadHandler();
	
	// Destructor
	~UploadHandler();
	
	// Check if request is a file upload
	bool isUploadRequest(const HttpRequest& request) const;
	
	// Check if request is multipart/form-data
	bool isMultipartRequest(const HttpRequest& request) const;
	
	// Handle file upload
	UploadResult handleUpload(const HttpRequest& request,
	                          const RouteResult& route);
	
	// Parse multipart/form-data body
	bool parseMultipart(const std::string& body,
	                    const std::string& boundary,
	                    std::vector<MultipartPart>& parts);
	
	// Save uploaded file to disk
	bool saveFile(const std::string& uploadDir,
	              const std::string& filename,
	              const std::string& data,
	              std::string& savedPath);
	
	// Generate unique filename to avoid overwrites
	std::string generateUniqueFilename(const std::string& uploadDir,
	                                    const std::string& originalFilename) const;
	
	// Sanitize filename (remove path components, dangerous chars)
	std::string sanitizeFilename(const std::string& filename) const;
	
	// Get content type from Content-Type header (without parameters)
	std::string getContentType(const std::string& header) const;
	
	// Extract boundary from Content-Type header
	std::string extractBoundary(const std::string& contentType) const;

private:
	// Non-copyable
	UploadHandler(const UploadHandler& other);
	UploadHandler& operator=(const UploadHandler& rhs);
	
	// Parse Content-Disposition header
	void parseContentDisposition(const std::string& header,
	                             std::string& name,
	                             std::string& filename) const;
	
	// Parse a single multipart part's headers
	bool parsePartHeaders(const std::string& headerSection,
	                      MultipartPart& part) const;
	
	// Check if directory exists and is writable
	bool isWritableDirectory(const std::string& path) const;
	
	// Create directory if it doesn't exist
	bool ensureDirectory(const std::string& path) const;
	
	// Generate HTML response for upload result
	std::string generateUploadResponse(const UploadResult& result) const;
	
	// Maximum number of files in single upload
	static const size_t MAX_FILES_PER_UPLOAD = 100;
	
	// Maximum filename length
	static const size_t MAX_FILENAME_LENGTH = 255;
};