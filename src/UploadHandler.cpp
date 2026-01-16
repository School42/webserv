#include "UploadHandler.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <ctime>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <iostream>

// Constructor
UploadHandler::UploadHandler() {}

// Destructor
UploadHandler::~UploadHandler() {}

// Check if request is a file upload (POST with body)
bool UploadHandler::isUploadRequest(const HttpRequest& request) const {
	if (request.getMethod() != "POST") {
		return false;
	}
	
	// Check for multipart/form-data or application content types
	std::string contentType = request.getHeader("Content-Type");
	if (contentType.empty()) {
		return false;
	}
	
	// Convert to lowercase for comparison
	std::string lowerCT = contentType;
	for (size_t i = 0; i < lowerCT.size(); ++i) {
		lowerCT[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowerCT[i])));
	}
	
	return lowerCT.find("multipart/form-data") != std::string::npos ||
	       lowerCT.find("application/octet-stream") != std::string::npos;
}

// Check if request is multipart/form-data
bool UploadHandler::isMultipartRequest(const HttpRequest& request) const {
	std::string contentType = request.getHeader("Content-Type");
	std::string lowerCT = contentType;
	for (size_t i = 0; i < lowerCT.size(); ++i) {
		lowerCT[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowerCT[i])));
	}
	return lowerCT.find("multipart/form-data") != std::string::npos;
}

// Get content type without parameters
std::string UploadHandler::getContentType(const std::string& header) const {
	size_t semicolonPos = header.find(';');
	if (semicolonPos != std::string::npos) {
		return header.substr(0, semicolonPos);
	}
	return header;
}

// Extract boundary from Content-Type header
std::string UploadHandler::extractBoundary(const std::string& contentType) const {
	// Look for boundary=
	std::string lowerCT = contentType;
	for (size_t i = 0; i < lowerCT.size(); ++i) {
		lowerCT[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowerCT[i])));
	}
	
	size_t boundaryPos = lowerCT.find("boundary=");
	if (boundaryPos == std::string::npos) {
		return "";
	}
	
	// Extract boundary value
	size_t valueStart = boundaryPos + 9;  // Length of "boundary="
	std::string boundary;
	
	// Check if quoted
	if (valueStart < contentType.size() && contentType[valueStart] == '"') {
		// Quoted boundary
		size_t endQuote = contentType.find('"', valueStart + 1);
		if (endQuote != std::string::npos) {
			boundary = contentType.substr(valueStart + 1, endQuote - valueStart - 1);
		}
	} else {
		// Unquoted boundary
		size_t valueEnd = contentType.find_first_of("; \t", valueStart);
		if (valueEnd == std::string::npos) {
			boundary = contentType.substr(valueStart);
		} else {
			boundary = contentType.substr(valueStart, valueEnd - valueStart);
		}
	}
	
	return boundary;
}

// Parse Content-Disposition header
void UploadHandler::parseContentDisposition(const std::string& header,
                                             std::string& name,
                                             std::string& filename) const {
	name.clear();
	filename.clear();
	
	// Look for name="..."
	size_t namePos = header.find("name=\"");
	if (namePos != std::string::npos) {
		size_t nameStart = namePos + 6;
		size_t nameEnd = header.find('"', nameStart);
		if (nameEnd != std::string::npos) {
			name = header.substr(nameStart, nameEnd - nameStart);
		}
	}
	
	// Look for filename="..."
	size_t filenamePos = header.find("filename=\"");
	if (filenamePos != std::string::npos) {
		size_t fnStart = filenamePos + 10;
		size_t fnEnd = header.find('"', fnStart);
		if (fnEnd != std::string::npos) {
			filename = header.substr(fnStart, fnEnd - fnStart);
		}
	}
}

// Parse part headers
bool UploadHandler::parsePartHeaders(const std::string& headerSection,
                                      MultipartPart& part) const {
	std::istringstream stream(headerSection);
	std::string line;
	
	while (std::getline(stream, line)) {
		// Remove \r if present
		if (!line.empty() && line[line.size() - 1] == '\r') {
			line = line.substr(0, line.size() - 1);
		}
		
		if (line.empty()) {
			continue;
		}
		
		// Parse header
		size_t colonPos = line.find(':');
		if (colonPos == std::string::npos) {
			continue;
		}
		
		std::string headerName = line.substr(0, colonPos);
		std::string headerValue = line.substr(colonPos + 1);
		
		// Trim whitespace
		size_t valueStart = headerValue.find_first_not_of(" \t");
		if (valueStart != std::string::npos) {
			headerValue = headerValue.substr(valueStart);
		}
		
		// Store header (lowercase name)
		std::string lowerName = headerName;
		for (size_t i = 0; i < lowerName.size(); ++i) {
			lowerName[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowerName[i])));
		}
		part.headers[lowerName] = headerValue;
		
		// Handle specific headers
		if (lowerName == "content-disposition") {
			parseContentDisposition(headerValue, part.name, part.filename);
			part.isFile = !part.filename.empty();
		} else if (lowerName == "content-type") {
			part.contentType = headerValue;
		}
	}
	
	return true;
}

// Parse multipart/form-data body
bool UploadHandler::parseMultipart(const std::string& body,
                                    const std::string& boundary,
                                    std::vector<MultipartPart>& parts) {
	parts.clear();
	
	if (boundary.empty()) {
		return false;
	}
	
	// Boundary markers
	std::string delimiter = "--" + boundary;
	std::string endDelimiter = "--" + boundary + "--";
	
	// Find first boundary
	size_t pos = body.find(delimiter);
	if (pos == std::string::npos) {
		return false;
	}
	
	// Skip past first boundary and CRLF
	pos += delimiter.size();
	if (pos < body.size() && body[pos] == '\r') pos++;
	if (pos < body.size() && body[pos] == '\n') pos++;
	
	while (pos < body.size() && parts.size() < MAX_FILES_PER_UPLOAD) {
		// Find next boundary
		size_t nextBoundary = body.find(delimiter, pos);
		if (nextBoundary == std::string::npos) {
			break;
		}
		
		// Extract part content (without trailing CRLF before boundary)
		std::string partContent = body.substr(pos, nextBoundary - pos);
		
		// Remove trailing CRLF
		if (partContent.size() >= 2 && 
		    partContent[partContent.size() - 2] == '\r' && 
		    partContent[partContent.size() - 1] == '\n') {
			partContent = partContent.substr(0, partContent.size() - 2);
		}
		
		// Split headers and body
		size_t headerEnd = partContent.find("\r\n\r\n");
		if (headerEnd == std::string::npos) {
			headerEnd = partContent.find("\n\n");
			if (headerEnd == std::string::npos) {
				// Malformed part
				pos = nextBoundary + delimiter.size();
				continue;
			}
		}
		
		std::string headerSection = partContent.substr(0, headerEnd);
		size_t bodyStart = headerEnd + (partContent[headerEnd] == '\r' ? 4 : 2);
		std::string partBody = (bodyStart < partContent.size()) 
		                        ? partContent.substr(bodyStart) 
		                        : "";
		
		// Parse part
		MultipartPart part;
		if (parsePartHeaders(headerSection, part)) {
			part.data = partBody;
			parts.push_back(part);
		}
		
		// Move to next part
		pos = nextBoundary + delimiter.size();
		
		// Check for end delimiter
		if (pos + 2 <= body.size() && body.substr(pos, 2) == "--") {
			break;  // End of multipart
		}
		
		// Skip CRLF after boundary
		if (pos < body.size() && body[pos] == '\r') pos++;
		if (pos < body.size() && body[pos] == '\n') pos++;
	}
	
	return !parts.empty();
}

// Sanitize filename
std::string UploadHandler::sanitizeFilename(const std::string& filename) const {
	if (filename.empty()) {
		return "unnamed";
	}
	
	std::string result;
	result.reserve(filename.size());
	
	// Extract just the filename (remove any path components)
	size_t lastSlash = filename.find_last_of("/\\");
	std::string baseName = (lastSlash != std::string::npos) 
	                        ? filename.substr(lastSlash + 1) 
	                        : filename;
	
	// Filter dangerous characters
	for (size_t i = 0; i < baseName.size() && result.size() < MAX_FILENAME_LENGTH; ++i) {
		char c = baseName[i];
		
		// Allow alphanumeric, dots, dashes, underscores
		if (std::isalnum(static_cast<unsigned char>(c)) || 
		    c == '.' || c == '-' || c == '_') {
			result += c;
		} else if (c == ' ') {
			result += '_';  // Replace spaces with underscores
		}
		// Skip other characters
	}
	
	// Don't allow filenames starting with dot (hidden files)
	if (!result.empty() && result[0] == '.') {
		result = "_" + result;
	}
	
	// Don't allow empty result
	if (result.empty()) {
		result = "unnamed";
	}
	
	return result;
}

// Generate unique filename
std::string UploadHandler::generateUniqueFilename(const std::string& uploadDir,
                                                   const std::string& originalFilename) const {
	std::string sanitized = sanitizeFilename(originalFilename);
	
	// Check if file exists
	std::string basePath = uploadDir;
	if (!basePath.empty() && basePath[basePath.size() - 1] != '/') {
		basePath += '/';
	}
	
	std::string fullPath = basePath + sanitized;
	
	struct stat st;
	if (stat(fullPath.c_str(), &st) != 0) {
		// File doesn't exist, use original name
		return sanitized;
	}
	
	// File exists, add timestamp/counter
	std::string nameBase = sanitized;
	std::string extension;
	
	size_t dotPos = sanitized.rfind('.');
	if (dotPos != std::string::npos && dotPos > 0) {
		nameBase = sanitized.substr(0, dotPos);
		extension = sanitized.substr(dotPos);
	}
	
	// Try with timestamp
	std::stringstream ss;
	ss << nameBase << "_" << std::time(NULL) << extension;
	std::string newName = ss.str();
	
	fullPath = basePath + newName;
	if (stat(fullPath.c_str(), &st) != 0) {
		return newName;
	}
	
	// Try with counter
	for (int i = 1; i < 1000; ++i) {
		ss.str("");
		ss << nameBase << "_" << std::time(NULL) << "_" << i << extension;
		newName = ss.str();
		fullPath = basePath + newName;
		
		if (stat(fullPath.c_str(), &st) != 0) {
			return newName;
		}
	}
	
	// Give up and use random suffix
	ss.str("");
	ss << nameBase << "_" << std::time(NULL) << "_" << rand() << extension;
	return ss.str();
}

// Check if directory is writable
bool UploadHandler::isWritableDirectory(const std::string& path) const {
	struct stat st;
	if (stat(path.c_str(), &st) != 0) {
		return false;
	}
	
	if (!S_ISDIR(st.st_mode)) {
		return false;
	}
	
	// Check write permission
	return (st.st_mode & S_IWUSR) || (st.st_mode & S_IWGRP) || (st.st_mode & S_IWOTH);
}

// Ensure directory exists
bool UploadHandler::ensureDirectory(const std::string& path) const {
	struct stat st;
	if (stat(path.c_str(), &st) == 0) {
		return S_ISDIR(st.st_mode);
	}
	
	// Try to create directory
	if (mkdir(path.c_str(), 0755) == 0) {
		return true;
	}
	
	return false;
}

// Save file to disk
bool UploadHandler::saveFile(const std::string& uploadDir,
                              const std::string& filename,
                              const std::string& data,
                              std::string& savedPath) {
	// Ensure upload directory exists
	if (!ensureDirectory(uploadDir)) {
		if (!isWritableDirectory(uploadDir)) {
			return false;
		}
	}
	
	// Generate unique filename
	std::string uniqueName = generateUniqueFilename(uploadDir, filename);
	
	// Build full path
	savedPath = uploadDir;
	if (!savedPath.empty() && savedPath[savedPath.size() - 1] != '/') {
		savedPath += '/';
	}
	savedPath += uniqueName;
	
	// Write file
	std::ofstream file(savedPath.c_str(), std::ios::binary);
	if (!file) {
		return false;
	}
	
	file.write(data.c_str(), data.size());
	
	return file.good();
}

// Handle file upload
UploadResult UploadHandler::handleUpload(const HttpRequest& request,
                                          const RouteResult& route) {
	UploadResult result;
	
	// Validate route
	if (!route.matched || !route.location) {
		result.statusCode = 500;
		result.statusText = "Internal Server Error";
		result.errorMessage = "Invalid route for upload";
		return result;
	}
	
	// Get upload directory
	std::string uploadDir = route.location->getUploadStore();
	if (uploadDir.empty()) {
		// Use root + location path as fallback
		uploadDir = route.location->getRoot();
		if (!uploadDir.empty() && uploadDir[uploadDir.size() - 1] != '/') {
			uploadDir += '/';
		}
		uploadDir += "uploads";
	}
	
	// Check request method
	if (request.getMethod() != "POST") {
		result.statusCode = 405;
		result.statusText = "Method Not Allowed";
		result.errorMessage = "Upload requires POST method";
		return result;
	}
	
	// Check body size
	size_t maxBodySize = route.location->getClientMaxBodySize();
	if (maxBodySize > 0 && request.getBody().size() > maxBodySize) {
		result.statusCode = 413;
		result.statusText = "Payload Too Large";
		result.errorMessage = "Request body exceeds maximum allowed size";
		return result;
	}
	
	// Check content type
	std::string contentType = request.getHeader("Content-Type");
	if (contentType.empty()) {
		result.statusCode = 400;
		result.statusText = "Bad Request";
		result.errorMessage = "Missing Content-Type header";
		return result;
	}
	
	// Handle multipart/form-data
	if (isMultipartRequest(request)) {
		std::string boundary = extractBoundary(contentType);
		if (boundary.empty()) {
			result.statusCode = 400;
			result.statusText = "Bad Request";
			result.errorMessage = "Missing boundary in multipart request";
			return result;
		}
		
		// Parse multipart body
		std::vector<MultipartPart> parts;
		if (!parseMultipart(request.getBody(), boundary, parts)) {
			result.statusCode = 400;
			result.statusText = "Bad Request";
			result.errorMessage = "Failed to parse multipart body";
			return result;
		}
		
		// Process each file part
		for (size_t i = 0; i < parts.size(); ++i) {
			if (!parts[i].isFile) {
				continue;  // Skip non-file parts
			}
			
			UploadedFile uploadedFile;
			uploadedFile.filename = parts[i].filename;
			uploadedFile.contentType = parts[i].contentType;
			uploadedFile.size = parts[i].data.size();
			
			// Save file
			std::string savedPath;
			if (saveFile(uploadDir, parts[i].filename, parts[i].data, savedPath)) {
				uploadedFile.savedPath = savedPath;
				result.files.push_back(uploadedFile);
			}
			// Continue even if some files fail
		}
		
		if (result.files.empty() && !parts.empty()) {
			// Had parts but no files were saved
			bool hadFiles = false;
			for (size_t i = 0; i < parts.size(); ++i) {
				if (parts[i].isFile) {
					hadFiles = true;
					break;
				}
			}
			
			if (hadFiles) {
				result.statusCode = 500;
				result.statusText = "Internal Server Error";
				result.errorMessage = "Failed to save uploaded files";
				return result;
			}
			// No file parts - just form data
		}
	} else {
		// Handle raw upload (application/octet-stream)
		// Use filename from Content-Disposition header if present
		std::string filename = "upload";
		std::string disposition = request.getHeader("Content-Disposition");
		if (!disposition.empty()) {
			std::string name;
			parseContentDisposition(disposition, name, filename);
		}
		
		if (filename.empty()) {
			// Try to get filename from X-Filename header
			filename = request.getHeader("X-Filename");
			if (filename.empty()) {
				filename = "upload";
			}
		}
		
		UploadedFile uploadedFile;
		uploadedFile.filename = filename;
		uploadedFile.contentType = getContentType(contentType);
		uploadedFile.size = request.getBody().size();
		
		std::string savedPath;
		if (!saveFile(uploadDir, filename, request.getBody(), savedPath)) {
			result.statusCode = 500;
			result.statusText = "Internal Server Error";
			result.errorMessage = "Failed to save uploaded file";
			return result;
		}
		
		uploadedFile.savedPath = savedPath;
		result.files.push_back(uploadedFile);
	}
	
	// Success
	result.success = true;
	result.statusCode = 201;
	result.statusText = "Created";
	
	return result;
}

// Generate HTML response for upload result
std::string UploadHandler::generateUploadResponse(const UploadResult& result) const {
	std::stringstream html;
	
	html << "<!DOCTYPE html>\n"
	     << "<html>\n"
	     << "<head>\n"
	     << "  <meta charset=\"UTF-8\">\n"
	     << "  <title>Upload Result</title>\n"
	     << "  <style>\n"
	     << "    body { font-family: sans-serif; margin: 40px; }\n"
	     << "    .success { color: green; }\n"
	     << "    .error { color: red; }\n"
	     << "    table { border-collapse: collapse; margin-top: 20px; }\n"
	     << "    th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n"
	     << "    th { background-color: #f0f0f0; }\n"
	     << "  </style>\n"
	     << "</head>\n"
	     << "<body>\n";
	
	if (result.success) {
		html << "  <h1 class=\"success\">Upload Successful</h1>\n";
		
		if (!result.files.empty()) {
			html << "  <p>Uploaded " << result.files.size() << " file(s):</p>\n"
			     << "  <table>\n"
			     << "    <tr><th>Filename</th><th>Size</th><th>Type</th></tr>\n";
			
			for (size_t i = 0; i < result.files.size(); ++i) {
				html << "    <tr>\n"
				     << "      <td>" << result.files[i].filename << "</td>\n"
				     << "      <td>" << result.files[i].size << " bytes</td>\n"
				     << "      <td>" << result.files[i].contentType << "</td>\n"
				     << "    </tr>\n";
			}
			
			html << "  </table>\n";
		}
	} else {
		html << "  <h1 class=\"error\">Upload Failed</h1>\n"
		     << "  <p>" << result.errorMessage << "</p>\n";
	}
	
	html << "</body>\n"
	     << "</html>\n";
	
	return html.str();
}