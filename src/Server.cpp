#include "Server.hpp"
#include "Response.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

// Constructor
Server::Server(const std::vector<ServerConfig>& servers)
	: _servers(servers),
	  _clientManager(_epoll),
	  _router(servers),
	  _running(false),
	  _lastTimeoutCheck(std::time(NULL)) {
	
	std::cout << "âœ“ Router initialized" << std::endl;
	std::cout << "âœ“ File server initialized" << std::endl;
	std::cout << "âœ“ CGI handler initialized" << std::endl;
	std::cout << "âœ“ Upload handler initialized" << std::endl;
}

// Destructor
Server::~Server() {
	stop();
	
	// Clean up listen sockets
	for (size_t i = 0; i < _listenSockets.size(); ++i) {
		delete _listenSockets[i];
	}
	_listenSockets.clear();
}

// Setup listen sockets
void Server::setupListenSockets() {
	for (size_t i = 0; i < _servers.size(); ++i) {
		const std::vector<ListenAddress>& addrs = _servers[i].getListenAddresses();
		
		for (size_t j = 0; j < addrs.size(); ++j) {
			// Check if we already have a socket for this address:port
			bool exists = false;
			for (size_t k = 0; k < _listenSockets.size(); ++k) {
				std::string sockAddr = _listenSockets[k]->getAddress();
				std::string confAddr = addrs[j].interface.empty() ? "0.0.0.0" : addrs[j].interface;
				if (_listenSockets[k]->getPort() == addrs[j].port && sockAddr == confAddr) {
					exists = true;
					break;
				}
			}
			
			if (exists) continue;
			
			// Create new listen socket
			Socket* sock = new Socket();
			sock->setReuseAddr(true); 
			// for the old fd, need to use this to avoid "address already in use" errors 
			//when restarting quickly, as the old sockets may still be in the TIME_WAIT state
			sock->setNonBlocking(true);
			sock->bind(addrs[j].interface, addrs[j].port);
			sock->listen();
			
			_epoll.add(sock->getFd(), EVENT_READ);
			_listenSockets.push_back(sock);
			
			std::cout << "âœ“ Listening on " 
			          << (addrs[j].interface.empty() ? "0.0.0.0" : addrs[j].interface)
			          << ":" << addrs[j].port << std::endl;
		}
	}
}

// Print startup info
void Server::printStartupInfo() const {
	std::cout << "\n=== Server is running. Press Ctrl+C to stop ===" << std::endl;
}

// Main run method
void Server::run() {
	if (_running) {
		std::cerr << "Server is already running" << std::endl;
		return;
	}
	
	setupListenSockets();
	printStartupInfo();
	
	_running = true;
	eventLoop();
}

// Stop server
void Server::stop() {
	if (_running) {
		std::cout << "\nShutting down..." << std::endl;
		_running = false;
	}
}

// Check if running
bool Server::isRunning() const {
	return _running;
}

// Main event loop
void Server::eventLoop() {
	std::vector<Event> events;
	
	while (_running) {
		// collect events for a specific amount of time "EPOLL_TIMEOUT"
		int numEvents = _epoll.wait(events, EPOLL_TIMEOUT);
		
		// Process every events
		for (int i = 0; i < numEvents; ++i) {
			int fd = events[i].fd;
			
			if (isListenSocket(fd)) {
				if (events[i].isReadable()) {
					Socket* listenSock = findListenSocket(fd);
					if (listenSock) {
						handleNewConnection(listenSock);
					}
				}
			} else if (isCgiPipe(fd)) {
				handleCgiEvent(events[i]);
			} else if (_clientManager.hasClient(fd)) {
				handleClientEvent(events[i]);
			}
		}
		
		// Check for timeouts
		checkTimeouts();
		checkCgiTimeouts();
	}
	
	std::cout << "âœ“ Server stopped gracefully" << std::endl;
}

// Check and handle client timeouts
void Server::checkTimeouts() {
	time_t now = std::time(NULL);
	if (now - _lastTimeoutCheck >= 1) {
		_clientManager.checkTimeouts(CLIENT_TIMEOUT);
		_lastTimeoutCheck = now;
	}
}

// Handle new connection
void Server::handleNewConnection(Socket* listenSocket) {
	std::string clientAddr;
	int clientPort;
	
	int clientFd = listenSocket->accept(clientAddr, clientPort);
	
	if (clientFd >= 0) {
		Client* client = _clientManager.addClient(clientFd, clientAddr, clientPort);
		
		// Store which port this client connected to
		_fdToPort[clientFd] = listenSocket->getPort();
		
		std::cout << "New connection from " << clientAddr << ":" << clientPort 
		          << " on port " << listenSocket->getPort()
		          << " (fd: " << clientFd << ") - Total: " 
		          << _clientManager.getClientCount() << std::endl;
		(void)client;
	}
}

// Handle client event
void Server::handleClientEvent(const Event& event) {
	Client* client = _clientManager.getClient(event.fd);
	if (!client) {
		return;
	}
	
	// Check for errors or disconnection
	if (event.isError() || event.isHangup() || event.isPeerClosed()) {
		std::cout << "Client " << client->getAddress() << " disconnected" << std::endl;
		_fdToPort.erase(event.fd);
		_clientManager.removeClient(event.fd);
		return;
	}
	
	// Handle based on client state
	switch (client->getState()) {
		case STATE_READING_REQUEST:
			if (event.isReadable()) {
				handleClientRead(client);
			}
			break;
			
		case STATE_WRITING_RESPONSE:
			if (event.isWritable()) {
				handleClientWrite(client);
			}
			break;
			
		case STATE_PROCESSING:
		case STATE_DONE:
		case STATE_ERROR:
			_fdToPort.erase(event.fd);
			_clientManager.removeClient(event.fd);
			break;
	}
}

// Handle client read
void Server::handleClientRead(Client* client) {
	ssize_t bytesRead = client->readData();
	
	if (bytesRead < 0) {
		std::cerr << "Error reading from client " << client->getFd() << std::endl;
		_fdToPort.erase(client->getFd());
		_clientManager.removeClient(client->getFd());
		return;
	}
	
	if (bytesRead == 0) {
		std::cout << "Client " << client->getAddress() << " closed connection" << std::endl;
		_fdToPort.erase(client->getFd());
		_clientManager.removeClient(client->getFd());
		return;
	}
	
	// Parse HTTP request
	HttpRequest& request = client->getRequest();
	size_t bytesConsumed = 0;
	HttpParseResult result = request.parse(client->getReadBuffer(), bytesConsumed);
	
	if (bytesConsumed > 0) {
		std::string& buffer = const_cast<std::string&>(client->getReadBuffer());
		buffer.erase(0, bytesConsumed);
	}
	
	if (result == PARSE_FAILED) {
		std::cerr << "Parse error from " << client->getAddress() << ": " 
		          << request.getErrorMessage() << std::endl;
		
		Response response = Response::error(400, request.getErrorMessage());
		response.setHeader("Server", "webserv/1.0");
		
		client->appendToWriteBuffer(response.build());
		client->setState(STATE_WRITING_RESPONSE);
		client->setKeepAlive(false);
		_epoll.modify(client->getFd(), EVENT_WRITE | EVENT_RDHUP);
		return;
	}
	
	if (result == PARSE_SUCCESS) {
		processRequest(client);
		client->setState(STATE_WRITING_RESPONSE);
		_epoll.modify(client->getFd(), EVENT_WRITE | EVENT_RDHUP);
	}
}

// Handle client write
void Server::handleClientWrite(Client* client) {
	ssize_t bytesWritten = client->writeData();
	
	if (bytesWritten < 0) {
		std::cerr << "Error writing to client " << client->getFd() << std::endl;
		_fdToPort.erase(client->getFd());
		_clientManager.removeClient(client->getFd());
		return;
	}
	
	if (!client->hasDataToWrite()) {
		std::cout << "Response sent to " << client->getAddress() << std::endl;
		
		if (client->isKeepAlive() && client->getRequestCount() < MAX_KEEPALIVE_REQUESTS) {
			client->incrementRequestCount();
			client->reset();
			_epoll.modify(client->getFd(), EVENT_READ | EVENT_RDHUP);
		} else {
			client->setState(STATE_DONE);
			_fdToPort.erase(client->getFd());
			_clientManager.removeClient(client->getFd());
		}
	}
}

// Process HTTP request
void Server::processRequest(Client* client) {
	HttpRequest& request = client->getRequest();
	Response response;
	bool keepAlive = request.isKeepAlive();
	
	std::cout << "Request: " << request.getMethod() << " " << request.getUri() 
	          << " from " << client->getAddress() << std::endl;
	
	// Route the request
	int listenPort = _fdToPort[client->getFd()];
	RouteResult route = _router.route(request, listenPort);
	
	if (!route.matched) {
		// Routing failed - serve error page
		std::cout << "  Route error: " << route.errorCode << " " << route.errorMessage << std::endl;
		
		if (route.server) {
			FileResult errPage = _fileServer.serveErrorPage(*route.server, route.errorCode);
			response.setStatusCode(errPage.statusCode);
			response.setStatusText(errPage.statusText);
			response.setContentType(errPage.contentType);
			response.setBody(errPage.body);
		} else {
			response = Response::error(route.errorCode, route.errorMessage);
		}
		keepAlive = false;
		
	} else if (_router.hasRedirect(*route.location)) {
		// Handle redirect from config (return directive)
		int code;
		std::string url;
		_router.getRedirect(*route.location, code, url);
		std::cout << "  Redirect: " << code << " -> " << url << std::endl;
		response = Response::redirect(code, url);
		keepAlive = false;
		
	} else {
		// Check for file upload first
		if (_uploadHandler.isUploadRequest(request) && 
		    !route.location->getUploadStore().empty()) {
			std::cout << "  File upload detected" << std::endl;
			
			UploadResult uploadResult = _uploadHandler.handleUpload(request, route);
			
			if (uploadResult.success) {
				std::cout << "  Upload success: " << uploadResult.files.size() 
				          << " file(s) uploaded" << std::endl;
				
				// Generate success response
				std::stringstream body;
				body << "<!DOCTYPE html>\n"
				     << "<html>\n"
				     << "<head><title>Upload Successful</title></head>\n"
				     << "<body>\n"
				     << "<h1>Upload Successful</h1>\n"
				     << "<p>Uploaded " << uploadResult.files.size() << " file(s):</p>\n"
				     << "<ul>\n";
				
				for (size_t i = 0; i < uploadResult.files.size(); ++i) {
					body << "<li>" << uploadResult.files[i].filename 
					     << " (" << uploadResult.files[i].size << " bytes)</li>\n";
				}
				
				body << "</ul>\n"
				     << "<p><a href=\"/\">Back to Home</a></p>\n"
				     << "</body>\n"
				     << "</html>\n";
				
				response = Response::created(body.str());
			} else {
				std::cout << "  Upload error: " << uploadResult.statusCode 
				          << " " << uploadResult.errorMessage << std::endl;
				response = Response::error(uploadResult.statusCode, uploadResult.errorMessage);
				keepAlive = false;
			}
			
		} else if (_router.isCgiRequest(*route.location, route.resolvedPath)) {
			// CGI request - start non-blocking
			std::cout << "  CGI request detected" << std::endl;
			std::cout << "  Resolved path: " << route.resolvedPath << std::endl;
			
			startCgiSession(client, route);
			return;  // Don't send response yet - will be sent when CGI completes
		} else if (request.getMethod() == "DELETE") {
			// Handle DELETE request
			std::cout << "  DELETE request detected" << std::endl;
			std::cout << "  Resolved path: " << route.resolvedPath << std::endl;
			
			FileResult deleteResult = _fileServer.deleteFile(request, route);
			
			if (deleteResult.success) {
				std::cout << "  File deleted successfully" << std::endl;
				response.setStatusCode(deleteResult.statusCode);
				response.setStatusText(deleteResult.statusText);
				response.setContentType(deleteResult.contentType);
				response.setBody(deleteResult.body);
			} else {
				std::cout << "  Delete error: " << deleteResult.statusCode 
				          << " " << deleteResult.errorMessage << std::endl;
				
				// Try custom error page
				FileResult errPage = _fileServer.serveErrorPage(*route.server, deleteResult.statusCode);
				response.setStatusCode(deleteResult.statusCode);
				response.setStatusText(deleteResult.statusText);
				response.setContentType(errPage.contentType);
				response.setBody(errPage.body);
				keepAlive = false;
			}
		} else {
			// Serve static file
			std::cout << "  Resolved path: " << route.resolvedPath << std::endl;
			FileResult fileResult = _fileServer.serveFile(request, route);
			
			if (fileResult.statusCode == 301 && !fileResult.redirectPath.empty()) {
				// Directory redirect (add trailing slash)
				std::cout << "  Directory redirect: " << fileResult.redirectPath << std::endl;
				response = Response::redirect(301, fileResult.redirectPath);
			} else if (fileResult.success) {
				// Success - serve file
				std::cout << "  Serving: " << fileResult.contentType 
				          << " (" << fileResult.body.size() << " bytes)" << std::endl;
				response.setStatusCode(fileResult.statusCode);
				response.setStatusText(fileResult.statusText);
				response.setContentType(fileResult.contentType);
				response.setBody(fileResult.body);
			} else {
				// Error - try custom error page
				std::cout << "  File error: " << fileResult.statusCode 
				          << " " << fileResult.errorMessage << std::endl;
				FileResult errPage = _fileServer.serveErrorPage(*route.server, fileResult.statusCode);
				response.setStatusCode(fileResult.statusCode);
				response.setStatusText(fileResult.statusText);
				response.setContentType(errPage.contentType);
				response.setBody(errPage.body);
				keepAlive = false;
			}
		}
	}
	
	response.setKeepAlive(keepAlive);
	response.setHeader("Server", "webserv/1.0");
	client->appendToWriteBuffer(response.build());
	client->setKeepAlive(keepAlive);
}

// Check if fd is a listen socket
bool Server::isListenSocket(int fd) const {
	for (size_t i = 0; i < _listenSockets.size(); ++i) {
		if (_listenSockets[i]->getFd() == fd) {
			return true;
		}
	}
	return false;
}

// Find listen socket by fd
Socket* Server::findListenSocket(int fd) const {
	for (size_t i = 0; i < _listenSockets.size(); ++i) {
		if (_listenSockets[i]->getFd() == fd) {
			return _listenSockets[i];
		}
	}
	return NULL;
}
// Check if fd is a CGI pipe
bool Server::isCgiPipe(int fd) const {
	// Check if it's a stdout fd (primary session key)
	if (_cgiSessions.find(fd) != _cgiSessions.end()) {
		return true;
	}
	// Check if it's a stdin fd
	if (_stdinToStdout.find(fd) != _stdinToStdout.end()) {
		return true;
	}
	return false;
}

// Handle CGI event (read/write on CGI pipes)
void Server::handleCgiEvent(const Event& event) {
	int fd = event.fd;
	
	// Find the session - check if this is a stdout fd or stdin fd
	std::map<int, CgiSession>::iterator sessionIt;
	bool isStdin = false;
	
	// First check if this fd is directly a session (stdout fd)
	sessionIt = _cgiSessions.find(fd);
	
	// If not found, check if it's a stdin fd
	if (sessionIt == _cgiSessions.end()) {
		std::map<int, int>::iterator stdinIt = _stdinToStdout.find(fd);
		if (stdinIt != _stdinToStdout.end()) {
			// This is a stdin fd, get the corresponding stdout fd
			int stdoutFd = stdinIt->second;
			sessionIt = _cgiSessions.find(stdoutFd);
			isStdin = true;
		}
	}
	
	if (sessionIt == _cgiSessions.end()) {
		return;  // Session already cleaned up
	}
	
	CgiSession& session = sessionIt->second;
	
	// Handle errors
	if (event.isError() || event.isHangup() || event.isPeerClosed()) {
		std::cout << "  [CGI] Error or hangup on fd " << fd << std::endl;
		
		// Always cleanup using stdout fd
		finalizeCgiSession(session.stdoutFd);
		return;
	}
	
	// Handle stdout (reading from CGI)
	if (!isStdin && fd == session.stdoutFd && event.isReadable()) {
		char buffer[4096];
		ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
		
		if (bytesRead > 0) {
			session.outputBuffer.append(buffer, bytesRead);
			
			// Check size limit
			if (session.outputBuffer.size() > 10 * 1024 * 1024) {  // 10MB
				std::cout << "  [CGI] Output too large" << std::endl;
				cleanupCgiSession(session.stdoutFd, true);
			}
		} else if (bytesRead == 0) {
			// EOF - CGI finished
			std::cout << "  [CGI] EOF on stdout" << std::endl;
			finalizeCgiSession(session.stdoutFd);
		}
		// bytesRead < 0: EAGAIN/EWOULDBLOCK or error
		// Errors are handled by event.isError() above
	}
	
	// Handle stdin (writing to CGI)
	if (isStdin && fd == session.stdinFd && event.isWritable() && !session.inputComplete) {
		size_t remaining = session.inputBuffer.size() - session.inputSent;
		if (remaining > 0) {
			ssize_t bytesWritten = write(fd, 
			                              session.inputBuffer.c_str() + session.inputSent,
			                              remaining);
			
			if (bytesWritten > 0) {
				session.inputSent += bytesWritten;
				
				// Check if all input sent
				if (session.inputSent >= session.inputBuffer.size()) {
					session.inputComplete = true;
					
					// Close stdin to signal EOF to CGI
					close(session.stdinFd);
					_epoll.remove(session.stdinFd);
					
					// Remove from helper map
					_stdinToStdout.erase(session.stdinFd);
					
					// Mark stdin as closed in session
					session.stdinFd = -1;
					
					std::cout << "  [CGI] All input sent, closed stdin" << std::endl;
				}
			}
			// If bytesWritten <= 0, just wait for next writable event
		}
	}
}

// Finalize CGI session (parse output and send response)
void Server::finalizeCgiSession(int cgiFd) {
	std::map<int, CgiSession>::iterator it = _cgiSessions.find(cgiFd);
	if (it == _cgiSessions.end()) {
		return;
	}
	
	CgiSession& session = it->second;
	Client* client = session.client;
	
	if (!client) {
		cleanupCgiSession(cgiFd, false);
		return;
	}
	
	std::cout << "  [CGI] Finalizing session (output: " 
	          << session.outputBuffer.size() << " bytes)" << std::endl;
	
	// Wait for child process
	int status;
	waitpid(session.pid, &status, 0);
	
	if (WIFEXITED(status)) {
		std::cout << "  [CGI] Process exited with status: " << WEXITSTATUS(status) << std::endl;
	} else if (WIFSIGNALED(status)) {
		std::cout << "  [CGI] Process killed by signal: " << WTERMSIG(status) << std::endl;
	}
	
	// Parse CGI output
	std::map<std::string, std::string> headers;
	std::string body;
	int statusCode = 200;
	std::string statusText = "OK";
	
	if (!_cgiHandler.parseCgiOutput(session.outputBuffer, headers, body, 
	                                 statusCode, statusText)) {
		std::cout << "  [CGI] Failed to parse output" << std::endl;
		Response response = Response::error(502, "Bad Gateway: Failed to parse CGI output");
		response.setHeader("Server", "webserv/1.0");
		client->appendToWriteBuffer(response.build());
		client->setState(STATE_WRITING_RESPONSE);
		client->setKeepAlive(false);
		_epoll.modify(client->getFd(), EVENT_WRITE | EVENT_RDHUP);
		cleanupCgiSession(cgiFd, false);
		return;
	}
	
	// Build response
	Response response;
	response.setStatusCode(statusCode);
	response.setStatusText(statusText);
	response.setBody(body);
	
	// Set content type
	std::map<std::string, std::string>::iterator ctIt = headers.find("Content-Type");
	if (ctIt != headers.end()) {
		response.setContentType(ctIt->second);
	} else {
		response.setContentType("text/html");
	}
	
	// Add other headers
	for (std::map<std::string, std::string>::iterator it = headers.begin();
	     it != headers.end(); ++it) {
		if (it->first != "Content-Type") {
			response.setHeader(it->first, it->second);
		}
	}
	
	response.setKeepAlive(client->isKeepAlive());
	response.setHeader("Server", "webserv/1.0");
	
	std::cout << "  [CGI] Sending response: " << statusCode << " " << statusText 
	          << " (" << body.size() << " bytes)" << std::endl;
	
	// Send response to client
	client->appendToWriteBuffer(response.build());
	client->setState(STATE_WRITING_RESPONSE);
	_epoll.modify(client->getFd(), EVENT_WRITE | EVENT_RDHUP);
	
	// Cleanup CGI session
	cleanupCgiSession(cgiFd, false);
}

// Cleanup CGI session
void Server::cleanupCgiSession(int cgiFd, bool sendError) {
	std::map<int, CgiSession>::iterator it = _cgiSessions.find(cgiFd);
	if (it == _cgiSessions.end()) {
		return;
	}
	
	CgiSession& session = it->second;
	Client* client = session.client;
	
	std::cout << "  [CGI] Cleaning up session (fd: " << cgiFd << ")" << std::endl;
	
	// Send error response if requested
	if (sendError && client) {
		Response response = Response::error(502, "Bad Gateway: CGI execution failed");
		response.setHeader("Server", "webserv/1.0");
		client->appendToWriteBuffer(response.build());
		client->setState(STATE_WRITING_RESPONSE);
		client->setKeepAlive(false);
		_epoll.modify(client->getFd(), EVENT_WRITE | EVENT_RDHUP);
	}
	
	// Remove stdin from helper map if it exists
	if (session.stdinFd >= 0) {
		_stdinToStdout.erase(session.stdinFd);
	}
	
	// Remove pipes from epoll and close them
	if (session.stdoutFd >= 0) {
		_epoll.remove(session.stdoutFd);
		close(session.stdoutFd);
	}
	if (session.stdinFd >= 0) {
		_epoll.remove(session.stdinFd);
		close(session.stdinFd);
	}
	
	// Kill process if still running
	if (session.pid > 0) {
		kill(session.pid, SIGKILL);
		waitpid(session.pid, NULL, 0);
	}
	
	// Remove session
	_cgiSessions.erase(it);
}

// Check CGI timeouts
void Server::checkCgiTimeouts() {
	time_t now = std::time(NULL);
	std::vector<int> timedOut;
	
	// Iterate over sessions (keyed by stdout fd)
	for (std::map<int, CgiSession>::iterator it = _cgiSessions.begin();
	     it != _cgiSessions.end(); ++it) {
		if (now - it->second.startTime > 30) {  // 30 second timeout
			std::cout << "  [CGI] Session timed out (stdout fd: " << it->first << ")" << std::endl;
			timedOut.push_back(it->first);
		}
	}
	
	for (size_t i = 0; i < timedOut.size(); ++i) {
		cleanupCgiSession(timedOut[i], true);
	}
}

// Start CGI session (non-blocking)
void Server::startCgiSession(Client* client, const RouteResult& route) {
	int listenPort = _fdToPort[client->getFd()];
	HttpRequest& request = client->getRequest();
	
	// Start CGI process
	CgiStartResult result = _cgiHandler.startNonBlocking(
		request, route,
		client->getAddress(),
		client->getPort(),
		listenPort
	);
	
	if (!result.success) {
		// Failed to start CGI
		std::cout << "  [CGI] Failed to start: " << result.errorMessage << std::endl;
		
		Response response = Response::error(result.errorCode, result.errorMessage);
		response.setHeader("Server", "webserv/1.0");
		client->appendToWriteBuffer(response.build());
		client->setState(STATE_WRITING_RESPONSE);
		client->setKeepAlive(false);
		_epoll.modify(client->getFd(), EVENT_WRITE | EVENT_RDHUP);
		return;
	}
	
	// Create CGI session
	CgiSession session;
	session.client = client;
	session.pid = result.pid;
	session.stdoutFd = result.stdoutFd;
	session.stdinFd = result.stdinFd;
	session.startTime = std::time(NULL);
	session.inputBuffer = request.getBody();
	session.inputSent = 0;
	session.inputComplete = request.getBody().empty();
	session.route = route;
	session.requestMethod = request.getMethod();
	session.requestUri = request.getUri();
	session.clientIp = client->getAddress();
	session.clientPort = client->getPort();
	session.serverPort = listenPort;
	
	// Add stdout to epoll for reading and store session (keyed by stdout fd)
	_epoll.add(session.stdoutFd, EVENT_READ);
	_cgiSessions[session.stdoutFd] = session;
	
	// If we have input to send, add stdin to epoll for writing
	if (!session.inputComplete) {
		_epoll.add(session.stdinFd, EVENT_WRITE);
		// Map stdin fd to stdout fd (for lookup in handleCgiEvent)
		_stdinToStdout[session.stdinFd] = session.stdoutFd;
	} else {
		// No input - close stdin immediately
		close(session.stdinFd);
		_cgiSessions[session.stdoutFd].stdinFd = -1;
	}
	
	// Set client to processing state
	client->setState(STATE_PROCESSING);
	
	std::cout << "  [CGI] Session started (stdout: " << session.stdoutFd 
	          << ", stdin: " << session.stdinFd << ")" << std::endl;
}