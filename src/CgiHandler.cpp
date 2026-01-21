#include "CgiHandler.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <iostream>

// Constructor
CgiHandler::CgiHandler() : _timeout(DEFAULT_TIMEOUT) {}

// Destructor
CgiHandler::~CgiHandler() {}

// Set timeout
void CgiHandler::setTimeout(int timeout) {
	_timeout = timeout;
}

// Get timeout
int CgiHandler::getTimeout() const {
	return _timeout;
}

// Check if file is executable
bool CgiHandler::isExecutable(const std::string& path) const {
	struct stat st;
	if (stat(path.c_str(), &st) != 0) {
		return false;
	}
	// Check if it's a regular file and has execute permission
	return S_ISREG(st.st_mode) && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
}

// Get interpreter for script
std::string CgiHandler::getInterpreter(const std::string& scriptPath,
                                        const LocationConfig& location) const {
	// Check cgi_pass directive first
	const std::vector<std::string>& cgiPass = location.getCgiPass();
	if (!cgiPass.empty()) {
		return cgiPass[0];  // Use first configured interpreter
	}
	
	// Try to determine from extension
	size_t dotPos = scriptPath.rfind('.');
	if (dotPos != std::string::npos) {
		std::string ext = scriptPath.substr(dotPos);
		
		// Common interpreters - try multiple paths
		if (ext == ".py") {
			if (isExecutable("/usr/bin/python3")) return "/usr/bin/python3";
			if (isExecutable("/usr/bin/python")) return "/usr/bin/python";
			return "python3";
		}
		if (ext == ".pl") {
			if (isExecutable("/usr/bin/perl")) return "/usr/bin/perl";
			return "perl";
		}
		if (ext == ".rb") {
			if (isExecutable("/usr/bin/ruby")) return "/usr/bin/ruby";
			return "ruby";
		}
		if (ext == ".php") {
			// Try php-cgi first (proper CGI mode), then php
			if (isExecutable("/usr/bin/php-cgi")) return "/usr/bin/php-cgi";
			if (isExecutable("/usr/bin/php")) return "/usr/bin/php";
			if (isExecutable("/usr/local/bin/php-cgi")) return "/usr/local/bin/php-cgi";
			if (isExecutable("/usr/local/bin/php")) return "/usr/local/bin/php";
			return "php";
		}
		if (ext == ".sh") {
			if (isExecutable("/bin/bash")) return "/bin/bash";
			if (isExecutable("/bin/sh")) return "/bin/sh";
			return "sh";
		}
	}
	
	// Default: try to execute directly (script must have shebang)
	return "";
}

// Extract PATH_INFO from URI
std::string CgiHandler::extractPathInfo(const std::string& uri,
                                         const std::string& scriptName) const {
	// PATH_INFO is the part of the URI after the script name
	size_t scriptPos = uri.find(scriptName);
	if (scriptPos != std::string::npos) {
		size_t afterScript = scriptPos + scriptName.length();
		if (afterScript < uri.length()) {
			// Check if there's more path after the script
			std::string remaining = uri.substr(afterScript);
			// Remove query string if present
			size_t queryPos = remaining.find('?');
			if (queryPos != std::string::npos) {
				remaining = remaining.substr(0, queryPos);
			}
			return remaining;
		}
	}
	return "";
}

// Build environment variables
std::vector<std::string> CgiHandler::buildEnvironment(const HttpRequest& request,
                                                       const RouteResult& route,
                                                       const std::string& scriptPath,
                                                       const std::string& clientIp,
                                                       int clientPort,
                                                       int serverPort) const {
	std::vector<std::string> env;
	std::stringstream ss;
	
	// Required CGI/1.1 variables
	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("SERVER_PROTOCOL=" + request.getHttpVersion());
	env.push_back("SERVER_SOFTWARE=webserv/1.0");
	env.push_back("REQUEST_METHOD=" + request.getMethod());
	
	// Server info
	ss.str("");
	ss << "SERVER_PORT=" << serverPort;
	env.push_back(ss.str());
	
	std::string host = request.getHost();
	if (host.empty()) {
		host = "localhost";
	}
	env.push_back("SERVER_NAME=" + host);
	
	// Script info
	env.push_back("SCRIPT_NAME=" + request.getPath());
	env.push_back("SCRIPT_FILENAME=" + scriptPath);
	
	// PATH_INFO and PATH_TRANSLATED
	std::string pathInfo = extractPathInfo(request.getUri(), request.getPath());
	if (!pathInfo.empty()) {
		env.push_back("PATH_INFO=" + pathInfo);
		env.push_back("PATH_TRANSLATED=" + route.location->getRoot() + pathInfo);
	}
	
	// Query string
	env.push_back("QUERY_STRING=" + request.getQueryString());
	
	// Request URI (full URI with query string)
	env.push_back("REQUEST_URI=" + request.getUri());
	
	// Document root
	if (route.location) {
		env.push_back("DOCUMENT_ROOT=" + route.location->getRoot());
	}
	
	// Client info
	env.push_back("REMOTE_ADDR=" + clientIp);
	ss.str("");
	ss << "REMOTE_PORT=" << clientPort;
	env.push_back(ss.str());
	
	// Content info (for POST requests)
	if (request.getMethod() == "POST") {
		ss.str("");
		ss << "CONTENT_LENGTH=" << request.getBody().size();
		env.push_back(ss.str());
		
		std::string contentType = request.getHeader("Content-Type");
		if (!contentType.empty()) {
			env.push_back("CONTENT_TYPE=" + contentType);
		}
	}
	
	// Convert HTTP headers to environment variables
	// HTTP_* format (header name uppercase, dashes become underscores)
	const std::map<std::string, std::string>& headers = request.getHeaders();
	for (std::map<std::string, std::string>::const_iterator it = headers.begin();
	     it != headers.end(); ++it) {
		// Skip Content-Type and Content-Length (already handled)
		if (it->first == "content-type" || it->first == "content-length") {
			continue;
		}
		
		// Convert header name: lowercase to uppercase, - to _
		std::string envName = "HTTP_";
		for (size_t i = 0; i < it->first.size(); ++i) {
			char c = it->first[i];
			if (c == '-') {
				envName += '_';
			} else {
				envName += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
			}
		}
		env.push_back(envName + "=" + it->second);
	}
	
	// Preserve some system environment variables
	const char* path = std::getenv("PATH");
	if (path) {
		env.push_back(std::string("PATH=") + path);
	} else {
		env.push_back("PATH=/usr/local/bin:/usr/bin:/bin");
	}
	
	const char* home = std::getenv("HOME");
	if (home) {
		env.push_back(std::string("HOME=") + home);
	}
	
	// Redirect status (for PHP)
	env.push_back("REDIRECT_STATUS=200");
	
	return env;
}

// Convert vector to envp array
char** CgiHandler::vectorToEnvp(const std::vector<std::string>& env) const {
	char** envp = new char*[env.size() + 1];
	
	for (size_t i = 0; i < env.size(); ++i) {
		envp[i] = new char[env[i].size() + 1];
		std::strcpy(envp[i], env[i].c_str());
	}
	envp[env.size()] = NULL;
	
	return envp;
}

// Free envp array
void CgiHandler::freeEnvp(char** envp) const {
	if (envp) {
		for (size_t i = 0; envp[i] != NULL; ++i) {
			delete[] envp[i];
		}
		delete[] envp;
	}
}

// Set fd to non-blocking
void CgiHandler::setNonBlocking(int fd) const {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags >= 0) {
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	}
}

// Close pipe safely
void CgiHandler::closePipe(int* pipe) const {
	if (pipe[0] >= 0) {
		close(pipe[0]);
		pipe[0] = -1;
	}
	if (pipe[1] >= 0) {
		close(pipe[1]);
		pipe[1] = -1;
	}
}

// Parse CGI output
bool CgiHandler::parseCgiOutput(const std::string& output,
                                 std::map<std::string, std::string>& headers,
                                 std::string& body,
                                 int& statusCode,
                                 std::string& statusText) const {
	// Find header/body separator (blank line)
	size_t headerEnd = output.find("\r\n\r\n");
	if (headerEnd == std::string::npos) {
		headerEnd = output.find("\n\n");
		if (headerEnd == std::string::npos) {
			// No headers found - treat entire output as body
			body = output;
			return true;
		}
	}
	
	// Parse headers
	std::string headerSection = output.substr(0, headerEnd);
	body = output.substr(headerEnd + (output[headerEnd] == '\r' ? 4 : 2));
	
	// Parse individual headers
	std::istringstream headerStream(headerSection);
	std::string line;
	
	while (std::getline(headerStream, line)) {
		// Remove \r if present
		if (!line.empty() && line[line.size() - 1] == '\r') {
			line = line.substr(0, line.size() - 1);
		}
		
		if (line.empty()) {
			continue;
		}
		
		// Find colon separator
		size_t colonPos = line.find(':');
		if (colonPos == std::string::npos) {
			continue;
		}
		
		std::string name = line.substr(0, colonPos);
		std::string value = line.substr(colonPos + 1);
		
		// Trim leading whitespace from value
		size_t valueStart = value.find_first_not_of(" \t");
		if (valueStart != std::string::npos) {
			value = value.substr(valueStart);
		}
		
		// Convert header name to lowercase for comparison
		std::string lowerName = name;
		for (size_t i = 0; i < lowerName.size(); ++i) {
			lowerName[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowerName[i])));
		}
		
		// Handle special headers
		if (lowerName == "status") {
			// Parse status line: "200 OK" or just "200"
			char* end = NULL;
			long code = std::strtol(value.c_str(), &end, 10);
			if (end != value.c_str()) {
				statusCode = static_cast<int>(code);
				// Skip whitespace after code
				while (*end == ' ' || *end == '\t') {
					++end;
				}
				if (*end != '\0') {
					statusText = end;
				}
			}
		} else if (lowerName == "content-type") {
			headers["Content-Type"] = value;
		} else if (lowerName == "location") {
			headers["Location"] = value;
			// If Location is set without explicit status, use 302
			if (statusCode == 200) {
				statusCode = 302;
				statusText = "Found";
			}
		} else {
			// Store other headers with original case
			headers[name] = value;
		}
	}
	
	return true;
}

// Generate error page
std::string CgiHandler::generateErrorPage(int code, const std::string& message) const {
	std::stringstream html;
	html << "<!DOCTYPE html>\n"
	     << "<html>\n"
	     << "<head><title>" << code << " Error</title></head>\n"
	     << "<body>\n"
	     << "<h1>" << code << " Error</h1>\n"
	     << "<p>" << message << "</p>\n"
	     << "<hr><p>webserv</p>\n"
	     << "</body>\n"
	     << "</html>\n";
	return html.str();
}

// Main execute method
CgiResult CgiHandler::execute(const HttpRequest& request,
                               const RouteResult& route,
                               const std::string& clientIp,
                               int clientPort,
                               int serverPort) {
	CgiResult result;
	
	// Validate route
	if (!route.matched || !route.location) {
		result.errorMessage = "Invalid route for CGI execution";
		result.body = generateErrorPage(500, result.errorMessage);
		return result;
	}
	
	std::string scriptPath = route.resolvedPath;
	std::cout << "  [CGI] Script path: " << scriptPath << std::endl;

	// Check if script exists
	struct stat st;
	if (stat(scriptPath.c_str(), &st) != 0) {
		result.statusCode = 404;
		result.statusText = "Not Found";
		result.errorMessage = "CGI script not found: " + scriptPath;
		result.body = generateErrorPage(404, result.errorMessage);
		std::cout << "  [CGI] ERROR: " << result.errorMessage << std::endl;
		return result;
	}
	
	// Get interpreter
	std::string interpreter = getInterpreter(scriptPath, *route.location);
	std::cout << "  [CGI] Interpreter: " << (interpreter.empty() ? "(direct)" : interpreter) << std::endl;
	
	// If no interpreter, script must be executable
	if (interpreter.empty() && !isExecutable(scriptPath)) {
		result.statusCode = 403;
		result.statusText = "Forbidden";
		result.errorMessage = "CGI script is not executable";
		result.body = generateErrorPage(403, result.errorMessage);
		return result;
	}
	
	// If interpreter specified, check it exists
	if (!interpreter.empty() && !isExecutable(interpreter)) {
		result.statusCode = 500;
		result.statusText = "Internal Server Error";
		result.errorMessage = "CGI interpreter not found: " + interpreter;
		result.body = generateErrorPage(500, result.errorMessage);
		return result;
	}
	// Create pipes
	int pipeIn[2];   // Parent writes, child reads (stdin)
	int pipeOut[2];  // Child writes, parent reads (stdout)
	
	if (pipe(pipeIn) < 0) {
		result.errorMessage = "Failed to create input pipe";
		result.body = generateErrorPage(500, result.errorMessage);
		return result;
	}
	
	if (pipe(pipeOut) < 0) {
		close(pipeIn[0]);
		close(pipeIn[1]);
		result.errorMessage = "Failed to create output pipe";
		result.body = generateErrorPage(500, result.errorMessage);
		return result;
	}
	
	// Build environment
	std::vector<std::string> envVec = buildEnvironment(request, route, scriptPath,
	                                                    clientIp, clientPort, serverPort);
	char** envp = vectorToEnvp(envVec);
	
	// Fork
	pid_t pid = fork();
	
	if (pid < 0) {
		// Fork failed
		close(pipeIn[0]);
		close(pipeIn[1]);
		close(pipeOut[0]);
		close(pipeOut[1]);
		freeEnvp(envp);
		result.errorMessage = "Failed to fork CGI process";
		result.body = generateErrorPage(500, result.errorMessage);
		return result;
	}
	
	if (pid == 0) {
		// Child process
		
		// Redirect stdin to pipeIn
		close(pipeIn[1]);  // Close write end
		dup2(pipeIn[0], STDIN_FILENO);
		close(pipeIn[0]);
		
		// Redirect stdout to pipeOut
		close(pipeOut[0]);  // Close read end
		dup2(pipeOut[1], STDOUT_FILENO);
		
		// Also redirect stderr to stdout so we can capture error messages
		dup2(pipeOut[1], STDERR_FILENO);
		
		close(pipeOut[1]);
		
		// Execute
		if (interpreter.empty()) {
			// Direct execution
			char* argv[] = {
				const_cast<char*>(scriptPath.c_str()),
				NULL
			};
			execve(scriptPath.c_str(), argv, envp);
		} else {
			// Use interpreter
			char* argv[] = {
				const_cast<char*>(interpreter.c_str()),
				const_cast<char*>(scriptPath.c_str()),
				NULL
			};
			execve(interpreter.c_str(), argv, envp);
		}
		
		// If we get here, exec failed
		std::cerr << "CGI exec failed: " << std::strerror(errno) << std::endl;
		_exit(1);
	}
	
	// Parent process
	freeEnvp(envp);
	
	// Close unused ends
	close(pipeIn[0]);   // Close read end of input pipe
	close(pipeOut[1]);  // Close write end of output pipe
	
	// Send request body to CGI (for POST)
	const std::string& body = request.getBody();
	if (!body.empty()) {
		ssize_t written = write(pipeIn[1], body.c_str(), body.size());
		(void)written;  // We don't handle partial writes in blocking mode
	}
	close(pipeIn[1]);  // Close write end to signal EOF
	
	// Read CGI output with timeout
	std::string output;
	char buffer[4096];
	time_t startTime = time(NULL);
	
	// Set non-blocking for timeout handling
	setNonBlocking(pipeOut[0]);
	
	while (true) {
		// Check timeout
		if (time(NULL) - startTime > _timeout) {
			// Kill the CGI process
			kill(pid, SIGKILL);
			waitpid(pid, NULL, 0);
			close(pipeOut[0]);
			
			result.statusCode = 504;
			result.statusText = "Gateway Timeout";
			result.errorMessage = "CGI script timed out";
			result.body = generateErrorPage(504, result.errorMessage);
			return result;
		}
		
		ssize_t bytesRead = read(pipeOut[0], buffer, sizeof(buffer));
		
		if (bytesRead > 0) {
			output.append(buffer, bytesRead);
			
			// Check output size limit
			if (output.size() > MAX_OUTPUT_SIZE) {
				kill(pid, SIGKILL);
				waitpid(pid, NULL, 0);
				close(pipeOut[0]);
				
				result.statusCode = 502;
				result.statusText = "Bad Gateway";
				result.errorMessage = "CGI output too large";
				result.body = generateErrorPage(502, result.errorMessage);
				return result;
			}
		} else if (bytesRead == 0) {
			// EOF - CGI finished
			break;
		} else {
			// bytesRead < 0 - check if it's EAGAIN/EWOULDBLOCK or real error
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				// No data available yet, sleep briefly and continue
				usleep(1000);  // Sleep 1ms
				continue;
			} else {
				// Real read error
				std::cerr << "  [CGI] Read error: " << std::strerror(errno) << std::endl;
				break;
			}
		}
	}
	
	close(pipeOut[0]);
	
	// Wait for child process
	int status;
	waitpid(pid, &status, 0);
	
	// Check exit status
	if (WIFEXITED(status)) {
		std::cout << "  [CGI] Process exited with status: " << WEXITSTATUS(status) << std::endl;
	} else if (WIFSIGNALED(status)) {
		std::cout << "  [CGI] Process killed by signal: " << WTERMSIG(status) << std::endl;
	}
	
	std::cout << "  [CGI] Output size: " << output.size() << " bytes" << std::endl;
	if (output.size() < 500) {
		std::cout << "  [CGI] Output: " << output << std::endl;
	} else {
		std::cout << "  [CGI] Output (first 500 chars): " << output.substr(0, 500) << std::endl;
	}
	
	// Parse CGI output
	result.statusCode = 200;
	result.statusText = "OK";
	
	std::string responseBody;
	if (!parseCgiOutput(output, result.headers, responseBody, 
	                    result.statusCode, result.statusText)) {
		result.statusCode = 502;
		result.statusText = "Bad Gateway";
		result.errorMessage = "Failed to parse CGI output";
		result.body = generateErrorPage(502, result.errorMessage);
		std::cout << "  [CGI] ERROR: Failed to parse output" << std::endl;
		return result;
	}
	
	std::cout << "  [CGI] Parsed body size: " << responseBody.size() << " bytes" << std::endl;
	
	// Set content type
	std::map<std::string, std::string>::iterator ctIt = result.headers.find("Content-Type");
	if (ctIt != result.headers.end()) {
		result.contentType = ctIt->second;
		std::cout << "  [CGI] Content-Type: " << result.contentType << std::endl;
	} else {
		result.contentType = "text/html";  // Default
		std::cout << "  [CGI] Content-Type: (default) text/html" << std::endl;
	}
	
	result.success = true;
	result.body = responseBody;
	
	return result;
}