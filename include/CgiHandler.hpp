#pragma once
#include <string>
#include <map>
#include <vector>
#include "HttpRequest.hpp"
#include "LocationConfig.hpp"
#include "Router.hpp"

// CGI execution result
struct CgiResult {
	bool success;
	int statusCode;
	std::string statusText;
	std::string contentType;
	std::string body;
	std::string errorMessage;
	std::map<std::string, std::string> headers;  // Additional headers from CGI
	
	CgiResult()
		: success(false),
		  statusCode(500),
		  statusText("Internal Server Error"),
		  contentType("text/html"),
		  body(""),
		  errorMessage("") {}
};

// CGI process state (for non-blocking execution)
struct CgiProcess {
	pid_t pid;
	int pipeIn[2];   // Pipe to send data to CGI (stdin)
	int pipeOut[2];  // Pipe to receive data from CGI (stdout)
	int clientFd;    // Associated client fd
	std::string inputData;   // Request body to send
	size_t inputSent;        // How much input has been sent
	std::string outputData;  // Response data received
	time_t startTime;        // For timeout tracking
	bool inputComplete;      // All input sent
	bool headersParsed;      // CGI headers have been parsed
	
	CgiProcess()
		: pid(-1),
		  clientFd(-1),
		  inputSent(0),
		  startTime(0),
		  inputComplete(false),
		  headersParsed(false) {
		pipeIn[0] = pipeIn[1] = -1;
		pipeOut[0] = pipeOut[1] = -1;
	}
};

// CGI start result (for non-blocking execution)
struct CgiStartResult {
	bool success;
	pid_t pid;
	int stdinFd;   // Write to this to send data to CGI
	int stdoutFd;  // Read from this to get CGI output
	std::string errorMessage;
	int errorCode;
	
	CgiStartResult()
		: success(false),
		  pid(-1),
		  stdinFd(-1),
		  stdoutFd(-1),
		  errorMessage(""),
		  errorCode(500) {}
};

class CgiHandler {
public:
	// Constructor
	CgiHandler();
	
	// Destructor
	~CgiHandler();
	
	// Start CGI script (non-blocking - returns immediately after fork)
	CgiStartResult startNonBlocking(const HttpRequest& request,
	                                const RouteResult& route,
	                                const std::string& clientIp,
	                                int clientPort,
	                                int serverPort);
	
	// Parse CGI output (headers + body) - made public for Server to use
	bool parseCgiOutput(const std::string& output,
	                    std::map<std::string, std::string>& headers,
	                    std::string& body,
	                    int& statusCode,
	                    std::string& statusText) const;
	
	// Check if a file is executable
	bool isExecutable(const std::string& path) const;
	
	// Get CGI interpreter for a script (based on extension or cgi_pass)
	std::string getInterpreter(const std::string& scriptPath,
	                           const LocationConfig& location) const;
	
	// Set timeout (in seconds)
	void setTimeout(int timeout);
	
	// Get timeout
	int getTimeout() const;
	
	// Generate error page - made public for Server to use
	std::string generateErrorPage(int code, const std::string& message) const;

private:
	// Non-copyable
	CgiHandler(const CgiHandler& other);
	CgiHandler& operator=(const CgiHandler& rhs);
	
	// Build environment variables for CGI
	std::vector<std::string> buildEnvironment(const HttpRequest& request,
	                                          const RouteResult& route,
	                                          const std::string& scriptPath,
	                                          const std::string& clientIp,
	                                          int clientPort,
	                                          int serverPort) const;
	
	// Convert vector of strings to char** for execve
	char** vectorToEnvp(const std::vector<std::string>& env) const;
	
	// Free envp array
	void freeEnvp(char** envp) const;
	
	// Set fd to non-blocking
	void setNonBlocking(int fd) const;
	
	// Close pipe safely
	void closePipe(int* pipe) const;
	
	// Extract PATH_INFO from URI
	std::string extractPathInfo(const std::string& uri,
	                            const std::string& scriptName) const;
	
	// Timeout in seconds
	int _timeout;
	
	// Default timeout
	static const int DEFAULT_TIMEOUT = 30;
	
	// Maximum output size
	static const size_t MAX_OUTPUT_SIZE = 10 * 1024 * 1024;  // 10MB
};