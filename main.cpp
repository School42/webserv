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
#include "Router.hpp"
#include "FileServer.hpp"
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

// Build HTTP response
std::string buildResponse(int statusCode, const std::string& statusText,
                          const std::string& contentType, const std::string& body,
                          bool keepAlive, const std::string& extraHeaders = "") {
	std::stringstream response;
	response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
	         << "Content-Type: " << contentType << "\r\n"
	         << "Content-Length: " << body.size() << "\r\n"
	         << "Server: webserv/1.0\r\n";
	
	if (keepAlive) {
		response << "Connection: keep-alive\r\n";
	} else {
		response << "Connection: close\r\n";
	}
	
	if (!extraHeaders.empty()) {
		response << extraHeaders;
	}
	
	response << "\r\n" << body;
	
	return response.str();
}

// Build redirect response
std::string buildRedirectResponse(int code, const std::string& location, bool keepAlive) {
	std::string statusText;
	switch (code) {
		case 301: statusText = "Moved Permanently"; break;
		case 302: statusText = "Found"; break;
		case 303: statusText = "See Other"; break;
		case 307: statusText = "Temporary Redirect"; break;
		case 308: statusText = "Permanent Redirect"; break;
		default:  statusText = "Redirect"; break;
	}
	
	std::stringstream body;
	body << "<!DOCTYPE html><html><head><title>" << code << " " << statusText << "</title></head>"
	     << "<body><h1>" << code << " " << statusText << "</h1>"
	     << "<p>Redirecting to <a href=\"" << location << "\">" << location << "</a></p></body></html>";
	
	std::string extraHeaders = "Location: " + location + "\r\n";
	
	return buildResponse(code, statusText, "text/html", body.str(), keepAlive, extraHeaders);
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
void processRequest(Client* client, Router& router, FileServer& fileServer) {
	HttpRequest& request = client->getRequest();
	std::string response;
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
			response = buildResponse(errPage.statusCode, errPage.statusText,
			                         errPage.contentType, errPage.body, false);
		} else {
			// No server found - use default error page
			FileResult errPage;
			errPage.statusCode = route.errorCode;
			errPage.statusText = route.errorCode == 404 ? "Not Found" :
			                     route.errorCode == 405 ? "Method Not Allowed" :
			                     route.errorCode == 403 ? "Forbidden" : "Error";
			std::stringstream errBody;
			errBody << "<!DOCTYPE html><html><head><title>" << route.errorCode 
			        << "</title></head><body><h1>" << route.errorCode << " " 
			        << errPage.statusText << "</h1><p>" << route.errorMessage 
			        << "</p><hr><p>webserv</p></body></html>";
			response = buildResponse(route.errorCode, errPage.statusText,
			                         "text/html", errBody.str(), false);
		}
		keepAlive = false;
		
	} else if (router.hasRedirect(*route.location)) {
		// Handle redirect from config (return directive)
		int code;
		std::string url;
		router.getRedirect(*route.location, code, url);
		std::cout << "  Redirect: " << code << " -> " << url << std::endl;
		response = buildRedirectResponse(code, url, false);
		keepAlive = false;
		
	} else {
		// Try to serve file
		std::cout << "  Resolved path: " << route.resolvedPath << std::endl;
		
		// Check if it's a CGI request (will be implemented in Phase 6)
		if (router.isCgiRequest(*route.location, route.resolvedPath)) {
			std::cout << "  CGI request detected (not implemented yet)" << std::endl;
			FileResult errPage = fileServer.serveErrorPage(*route.server, 501);
			response = buildResponse(501, "Not Implemented", "text/html", 
			                         errPage.body, false);
			keepAlive = false;
		} else {
			// Serve static file
			FileResult fileResult = fileServer.serveFile(request, route);
			
			if (fileResult.statusCode == 301 && !fileResult.redirectPath.empty()) {
				// Directory redirect (add trailing slash)
				std::cout << "  Directory redirect: " << fileResult.redirectPath << std::endl;
				response = buildRedirectResponse(301, fileResult.redirectPath, keepAlive);
			} else if (fileResult.success) {
				// Success - serve file
				std::cout << "  Serving: " << fileResult.contentType 
				          << " (" << fileResult.body.size() << " bytes)" << std::endl;
				response = buildResponse(fileResult.statusCode, fileResult.statusText,
				                         fileResult.contentType, fileResult.body, keepAlive);
			} else {
				// Error - try custom error page
				std::cout << "  File error: " << fileResult.statusCode 
				          << " " << fileResult.errorMessage << std::endl;
				FileResult errPage = fileServer.serveErrorPage(*route.server, fileResult.statusCode);
				response = buildResponse(fileResult.statusCode, fileResult.statusText,
				                         errPage.contentType, errPage.body, false);
				keepAlive = false;
			}
		}
	}
	
	client->appendToWriteBuffer(response);
	client->setKeepAlive(keepAlive);
}

// Handle client read event
void handleClientRead(Client* client, Epoll& epoll, ClientManager& clientManager, 
                      Router& router, FileServer& fileServer) {
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
		
		std::stringstream errBody;
		errBody << "<!DOCTYPE html><html><head><title>400 Bad Request</title></head>"
		        << "<body><h1>400 Bad Request</h1><p>" << request.getErrorMessage() 
		        << "</p><hr><p>webserv</p></body></html>";
		
		client->appendToWriteBuffer(
			buildResponse(400, "Bad Request", "text/html", errBody.str(), false));
		client->setState(STATE_WRITING_RESPONSE);
		client->setKeepAlive(false);
		epoll.modify(client->getFd(), EVENT_WRITE | EVENT_RDHUP);
		return;
	}
	
	if (result == PARSE_SUCCESS) {
		processRequest(client, router, fileServer);
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
                       Router& router, FileServer& fileServer) {
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
				handleClientRead(client, epoll, clientManager, router, fileServer);
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
		
		// Create router and file server
		Router router(servers);
		FileServer fileServer;
		std::cout << "✓ Router initialized" << std::endl;
		std::cout << "✓ File server initialized" << std::endl;
		
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
					handleClientEvent(events[i], epoll, clientManager, router, fileServer);
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