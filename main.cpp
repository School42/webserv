#include <iostream>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <csignal>
#include <cstring>

#include "Socket.hpp"
#include "Epoll.hpp"
#include "Client.hpp"
#include "ClientManager.hpp"
#include "HttpRequest.hpp"
#include "Response.hpp"
#include "Router.hpp"
#include "FileServer.hpp"
#include "CgiHandler.hpp"
#include "UploadHandler.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "ConfigError.hpp"

// Global flag for graceful shutdown
static volatile bool g_running = true;

// Timeout constants
static const time_t CLIENT_TIMEOUT = 60;
static const int EPOLL_TIMEOUT = 1000;
static const int MAX_KEEPALIVE_REQUESTS = 100;

// Map listen socket fd to port
static std::map<int, int> g_fdToPort;

void signalHandler(int signum);
bool isListenSocket(int fd, const std::vector<Socket*>& listenSockets);
Socket* findListenSocket(int fd, const std::vector<Socket*>& listenSockets);
void handleNewConnection(Socket* listenSocket, ClientManager& clientManager);
void processRequest(Client* client, Router& router, FileServer& fileServer, 
                    CgiHandler& cgiHandler, UploadHandler& uploadHandler);
void handleClientRead(Client* client, Epoll& epoll, ClientManager& clientManager, 
                      Router& router, FileServer& fileServer, CgiHandler& cgiHandler,
                      UploadHandler& uploadHandler);
void handleClientWrite(Client* client, Epoll& epoll, ClientManager& clientManager);
void handleClientEvent(const Event& event, Epoll& epoll, ClientManager& clientManager, 
                       Router& router, FileServer& fileServer, CgiHandler& cgiHandler,
                       UploadHandler& uploadHandler);

int main(int argc, char** argv) {
	std::signal(SIGINT, signalHandler);
	std::signal(SIGTERM, signalHandler);
	std::signal(SIGPIPE, SIG_IGN);
	
	try {
		if (argc != 2) {
			std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
			return 1;
		}
		
		std::string filename = argv[1];
		if (filename.length() < 6 || filename.substr(filename.length() - 5) != ".conf") {
			std::cerr << "Error: Configuration file must have .conf extension" << std::endl;
			return 1;
		}
		
		std::ifstream file(argv[1]);
		if (!file) {
			std::cerr << "Error: Cannot open file: " << argv[1] << std::endl;
			return 1;
		}
		
		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string config = buffer.str();
		file.close();
		
		Lexer lexer(config);
		Parser parser(lexer);
		std::vector<ServerConfig> servers = parser.parse();
		
		std::cout << "✓ Configuration parsed successfully!" << std::endl;
		
		// Create router, file server, CGI handler, and upload handler
		Router router(servers);
		FileServer fileServer;
		CgiHandler cgiHandler;
		UploadHandler uploadHandler;
		std::cout << "✓ Router initialized" << std::endl;
		std::cout << "✓ File server initialized" << std::endl;
		std::cout << "✓ CGI handler initialized" << std::endl;
		std::cout << "✓ Upload handler initialized" << std::endl;
		
		Epoll epoll;
		ClientManager clientManager(epoll);
		
		std::vector<Socket*> listenSockets;
		
		for (size_t i = 0; i < servers.size(); ++i) {
			const std::vector<ListenAddress>& addrs = servers[i].getListenAddresses();
			
			for (size_t j = 0; j < addrs.size(); ++j) {
				bool exists = false;
				for (size_t k = 0; k < listenSockets.size(); ++k) {
					std::string sockAddr = listenSockets[k]->getAddress();
					std::string confAddr = addrs[j].interface.empty() ? "0.0.0.0" : addrs[j].interface;
					if (listenSockets[k]->getPort() == addrs[j].port && sockAddr == confAddr) {
						exists = true;
						break;
					}
				}
				
				if (exists) continue;
				
				Socket* sock = new Socket();
				sock->setReuseAddr(true);
				sock->setNonBlocking(true);
				sock->bind(addrs[j].interface, addrs[j].port);
				sock->listen();
				
				epoll.add(sock->getFd(), EVENT_READ);
				listenSockets.push_back(sock);
				
				std::cout << "✓ Listening on " 
				          << (addrs[j].interface.empty() ? "0.0.0.0" : addrs[j].interface)
				          << ":" << addrs[j].port << std::endl;
			}
		}
		
		std::cout << "\n=== Server is running. Press Ctrl+C to stop ===" << std::endl;
		
		std::vector<Event> events;
		time_t lastTimeoutCheck = std::time(NULL);
		
		while (g_running) {
			int numEvents = epoll.wait(events, EPOLL_TIMEOUT);
			
			for (int i = 0; i < numEvents; ++i) {
				int fd = events[i].fd;
				
				if (isListenSocket(fd, listenSockets)) {
					if (events[i].isReadable()) {
						Socket* listenSock = findListenSocket(fd, listenSockets);
						if (listenSock) {
							handleNewConnection(listenSock, clientManager);
						}
					}
				} else if (clientManager.hasClient(fd)) {
					handleClientEvent(events[i], epoll, clientManager, router, fileServer, 
					                  cgiHandler, uploadHandler);
				}
			}
			
			time_t now = std::time(NULL);
			if (now - lastTimeoutCheck >= 1) {
				clientManager.checkTimeouts(CLIENT_TIMEOUT);
				lastTimeoutCheck = now;
			}
		}
		
		std::cout << "\nShutting down..." << std::endl;
		
		for (size_t i = 0; i < listenSockets.size(); ++i) {
			delete listenSockets[i];
		}
		
		std::cout << "✓ Server stopped gracefully" << std::endl;
		return 0;
		
	} catch (const SocketError& e) {
		std::cerr << "✗ Socket error: " << e.what() << std::endl;
		return 1;
	} catch (const EpollError& e) {
		std::cerr << "✗ Epoll error: " << e.what() << std::endl;
		return 1;
	} catch (const ConfigError& e) {
		std::cerr << "✗ " << e.formatMessage() << std::endl;
		return 1;
	} catch (const std::exception& e) {
		std::cerr << "✗ Error: " << e.what() << std::endl;
		return 1;
	}
}

void signalHandler(int signum) {
	(void)signum;
	std::cout << "\nReceived shutdown signal..." << std::endl;
	g_running = false;
}

// Check if fd is a listen socket
bool isListenSocket(int fd, const std::vector<Socket*>& listenSockets) {
	for (size_t i = 0; i < listenSockets.size(); ++i) {
		if (listenSockets[i]->getFd() == fd) {
			return true;
		}
	}
	return false;
}

// Find listen socket by fd
Socket* findListenSocket(int fd, const std::vector<Socket*>& listenSockets) {
	for (size_t i = 0; i < listenSockets.size(); ++i) {
		if (listenSockets[i]->getFd() == fd) {
			return listenSockets[i];
		}
	}
	return NULL;
}

// Handle new connection
void handleNewConnection(Socket* listenSocket, ClientManager& clientManager) {
	std::string clientAddr;
	int clientPort;
	
	int clientFd = listenSocket->accept(clientAddr, clientPort);
	
	if (clientFd >= 0) {
		Client* client = clientManager.addClient(clientFd, clientAddr, clientPort);
		// Store which port this client connected to
		g_fdToPort[clientFd] = listenSocket->getPort();
		
		std::cout << "New connection from " << clientAddr << ":" << clientPort 
		          << " on port " << listenSocket->getPort()
		          << " (fd: " << clientFd << ") - Total: " 
		          << clientManager.getClientCount() << std::endl;
		(void)client;
	}
}

// Process a completed HTTP request
void processRequest(Client* client, Router& router, FileServer& fileServer, 
                    CgiHandler& cgiHandler, UploadHandler& uploadHandler) {
	HttpRequest& request = client->getRequest();
	Response response;
	bool keepAlive = request.isKeepAlive();
	
	std::cout << "Request: " << request.getMethod() << " " << request.getUri() 
	          << " from " << client->getAddress() << std::endl;
	
	// Route the request
	int listenPort = g_fdToPort[client->getFd()];
	RouteResult route = router.route(request, listenPort);
	
	if (!route.matched) {
		// Routing failed - serve error page
		std::cout << "  Route error: " << route.errorCode << " " << route.errorMessage << std::endl;
		
		if (route.server) {
			FileResult errPage = fileServer.serveErrorPage(*route.server, route.errorCode);
			response.setStatusCode(errPage.statusCode);
			response.setStatusText(errPage.statusText);
			response.setContentType(errPage.contentType);
			response.setBody(errPage.body);
		} else {
			response = Response::error(route.errorCode, route.errorMessage);
		}
		keepAlive = false;
		
	} else if (router.hasRedirect(*route.location)) {
		// Handle redirect from config (return directive)
		int code;
		std::string url;
		router.getRedirect(*route.location, code, url);
		std::cout << "  Redirect: " << code << " -> " << url << std::endl;
		response = Response::redirect(code, url);
		keepAlive = false;
		
	} else {
		// Check for file upload first
		if (uploadHandler.isUploadRequest(request) && 
		    !route.location->getUploadStore().empty()) {
			std::cout << "  File upload detected" << std::endl;
			
			UploadResult uploadResult = uploadHandler.handleUpload(request, route);
			
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
			
		} else if (router.isCgiRequest(*route.location, route.resolvedPath)) {
			// CGI request
			std::cout << "  CGI request detected" << std::endl;
			std::cout << "  Resolved path: " << route.resolvedPath << std::endl;
			
			// Execute CGI
			CgiResult cgiResult = cgiHandler.execute(request, route,
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
		} else {
			// Serve static file
			std::cout << "  Resolved path: " << route.resolvedPath << std::endl;
			FileResult fileResult = fileServer.serveFile(request, route);
			
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
				FileResult errPage = fileServer.serveErrorPage(*route.server, fileResult.statusCode);
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

// Handle client read event
void handleClientRead(Client* client, Epoll& epoll, ClientManager& clientManager, 
                      Router& router, FileServer& fileServer, CgiHandler& cgiHandler,
                      UploadHandler& uploadHandler) {
	ssize_t bytesRead = client->readData();
	
	if (bytesRead < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			std::cerr << "Error reading from client " << client->getFd() 
			          << ": " << std::strerror(errno) << std::endl;
			g_fdToPort.erase(client->getFd());
			clientManager.removeClient(client->getFd());
		}
		return;
	}
	
	if (bytesRead == 0) {
		std::cout << "Client " << client->getAddress() << " closed connection" << std::endl;
		g_fdToPort.erase(client->getFd());
		clientManager.removeClient(client->getFd());
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
		epoll.modify(client->getFd(), EVENT_WRITE | EVENT_RDHUP);
		return;
	}
	
	if (result == PARSE_SUCCESS) {
		processRequest(client, router, fileServer, cgiHandler, uploadHandler);
		client->setState(STATE_WRITING_RESPONSE);
		epoll.modify(client->getFd(), EVENT_WRITE | EVENT_RDHUP);
	}
}

// Handle client write event
void handleClientWrite(Client* client, Epoll& epoll, ClientManager& clientManager) {
	ssize_t bytesWritten = client->writeData();
	
	if (bytesWritten < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			std::cerr << "Error writing to client " << client->getFd() 
			          << ": " << std::strerror(errno) << std::endl;
			g_fdToPort.erase(client->getFd());
			clientManager.removeClient(client->getFd());
		}
		return;
	}
	
	if (!client->hasDataToWrite()) {
		std::cout << "Response sent to " << client->getAddress() << std::endl;
		
		if (client->isKeepAlive() && client->getRequestCount() < MAX_KEEPALIVE_REQUESTS) {
			client->incrementRequestCount();
			client->reset();
			epoll.modify(client->getFd(), EVENT_READ | EVENT_RDHUP);
		} else {
			client->setState(STATE_DONE);
			g_fdToPort.erase(client->getFd());
			clientManager.removeClient(client->getFd());
		}
	}
}

// Handle client event
void handleClientEvent(const Event& event, Epoll& epoll, ClientManager& clientManager, 
                       Router& router, FileServer& fileServer, CgiHandler& cgiHandler,
                       UploadHandler& uploadHandler) {
	Client* client = clientManager.getClient(event.fd);
	if (!client) {
		return;
	}
	
	if (event.isError() || event.isHangup() || event.isPeerClosed()) {
		std::cout << "Client " << client->getAddress() << " disconnected" << std::endl;
		g_fdToPort.erase(event.fd);
		clientManager.removeClient(event.fd);
		return;
	}
	
	switch (client->getState()) {
		case STATE_READING_REQUEST:
			if (event.isReadable()) {
				handleClientRead(client, epoll, clientManager, router, fileServer, 
				                 cgiHandler, uploadHandler);
			}
			break;
			
		case STATE_WRITING_RESPONSE:
			if (event.isWritable()) {
				handleClientWrite(client, epoll, clientManager);
			}
			break;
			
		case STATE_PROCESSING:
		case STATE_DONE:
		case STATE_ERROR:
			g_fdToPort.erase(event.fd);
			clientManager.removeClient(event.fd);
			break;
	}
}