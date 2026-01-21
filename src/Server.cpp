#include "Server.hpp"
#include "Response.hpp"
#include <iostream>
#include <sstream>
#include <cstring>

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
			} else if (_clientManager.hasClient(fd)) {
				handleClientEvent(events[i]);
			}
		}
		
		// Check for timeouts
		checkTimeouts();
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
			// CGI request
			std::cout << "  CGI request detected" << std::endl;
			std::cout << "  Resolved path: " << route.resolvedPath << std::endl;
			
			// Execute CGI
			CgiResult cgiResult = _cgiHandler.execute(request, route,
			                                          client->getAddress(),
			                                          client->getPort(),
			                                          listenPort);
			
			if (cgiResult.success) {
				std::cout << "  CGI success: " << cgiResult.statusCode << " "
				          << cgiResult.statusText << " (" << cgiResult.body.size() 
				          << " bytes)" << std::endl;
				
				response.setStatusCode(cgiResult.statusCode);
				response.setStatusText(cgiResult.statusText);
				response.setContentType(cgiResult.contentType);
				response.setBody(cgiResult.body);
				
				// Add CGI headers
				for (std::map<std::string, std::string>::iterator it = cgiResult.headers.begin();
				     it != cgiResult.headers.end(); ++it) {
					if (it->first != "Content-Type") {
						response.setHeader(it->first, it->second);
					}
				}
			} else {
				std::cout << "  CGI error: " << cgiResult.statusCode << " "
				          << cgiResult.errorMessage << std::endl;
				response.setStatusCode(cgiResult.statusCode);
				response.setStatusText(cgiResult.statusText);
				response.setContentType(cgiResult.contentType);
				response.setBody(cgiResult.body);
				keepAlive = false;
			}
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