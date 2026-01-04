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
	         << "Content-Length: " << body.size() << "\r\n";
	
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

// Build error response
std::string buildErrorResponse(int statusCode, const std::string& statusText,
                               const std::string& message, bool keepAlive) {
	std::stringstream body;
	body << "<!DOCTYPE html><html><head><title>" << statusCode << " " << statusText << "</title></head>"
	     << "<body><h1>" << statusCode << " " << statusText << "</h1>"
	     << "<p>" << message << "</p><hr><p>webserv</p></body></html>";
	
	return buildResponse(statusCode, statusText, "text/html", body.str(), keepAlive);
}

// Build redirect response
std::string buildRedirectResponse(int code, const std::string& location) {
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
	
	return buildResponse(code, statusText, "text/html", body.str(), false, extraHeaders);
}

// Build success response showing routing info (for testing)
std::string buildRoutingInfoResponse(const HttpRequest& request, const RouteResult& route) {
	std::stringstream body;
	body << "<!DOCTYPE html><html><head><title>Routing Info</title></head><body>"
	     << "<h1>Request Routed Successfully</h1>"
	     << "<h2>Request Info</h2>"
	     << "<p><strong>Method:</strong> " << request.getMethod() << "</p>"
	     << "<p><strong>URI:</strong> " << request.getUri() << "</p>"
	     << "<p><strong>Path:</strong> " << request.getPath() << "</p>"
	     << "<p><strong>Query:</strong> " << request.getQueryString() << "</p>"
	     << "<p><strong>Host:</strong> " << request.getHost() << "</p>"
	     << "<h2>Routing Result</h2>"
	     << "<p><strong>Matched Server:</strong> ";
	
	if (route.server) {
		const std::vector<std::string>& names = route.server->getServerNames();
		if (names.empty()) {
			body << "(default)";
		} else {
			body << names[0];
		}
	}
	
	body << "</p><p><strong>Matched Location:</strong> ";
	if (route.location) {
		body << route.location->getPath();
	}
	
	body << "</p><p><strong>Resolved Path:</strong> " << route.resolvedPath << "</p>"
	     << "<p><strong>Root:</strong> " << (route.location ? route.location->getRoot() : "(none)") << "</p>"
	     << "<p><strong>Allowed Methods:</strong> ";
	
	if (route.location) {
		const std::vector<std::string>& methods = route.location->getAllowedMethods();
		for (size_t i = 0; i < methods.size(); ++i) {
			body << methods[i];
			if (i < methods.size() - 1) body << ", ";
		}
	}
	
	body << "</p><p><strong>Is CGI:</strong> " 
	     << (route.location && !route.location->getCgiExtension().empty() ? "Possible" : "No")
	     << "</p></body></html>";
	
	return buildResponse(200, "OK", "text/html", body.str(), request.isKeepAlive());
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

// Handle client read event
void handleClientRead(Client* client, Epoll& epoll, ClientManager& clientManager, Router& router) {
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
		
		client->appendToWriteBuffer(
			buildErrorResponse(400, "Bad Request", request.getErrorMessage(), false));
		client->setState(STATE_WRITING_RESPONSE);
		client->setKeepAlive(false);
		epoll.modify(client->getFd(), EVENT_WRITE | EVENT_RDHUP);
		return;
	}
	
	if (result == PARSE_SUCCESS) {
		std::cout << "Request: " << request.getMethod() << " " << request.getUri() 
		          << " from " << client->getAddress() << std::endl;
		
		// Route the request
		int listenPort = g_fdToPort[client->getFd()];
		RouteResult route = router.route(request, listenPort);
		
		std::string response;
		
		if (!route.matched) {
			// Routing failed
			std::cout << "  Route error: " << route.errorCode << " " << route.errorMessage << std::endl;
			response = buildErrorResponse(route.errorCode, 
			                              route.errorCode == 404 ? "Not Found" :
			                              route.errorCode == 405 ? "Method Not Allowed" :
			                              route.errorCode == 403 ? "Forbidden" : "Error",
			                              route.errorMessage, false);
			client->setKeepAlive(false);
		} else if (router.hasRedirect(*route.location)) {
			// Handle redirect
			int code;
			std::string url;
			router.getRedirect(*route.location, code, url);
			std::cout << "  Redirect: " << code << " -> " << url << std::endl;
			response = buildRedirectResponse(code, url);
			client->setKeepAlive(false);
		} else {
			// Success - for now, show routing info
			// TODO: Actually serve files, handle CGI, etc.
			std::cout << "  Routed to: " << route.location->getPath() 
			          << " -> " << route.resolvedPath << std::endl;
			response = buildRoutingInfoResponse(request, route);
			client->setKeepAlive(request.isKeepAlive());
		}
		
		client->appendToWriteBuffer(response);
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
void handleClientEvent(const Event& event, Epoll& epoll, ClientManager& clientManager, Router& router) {
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
				handleClientRead(client, epoll, clientManager, router);
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
		
		// Create router
		Router router(servers);
		std::cout << "✓ Router initialized" << std::endl;
		
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
					handleClientEvent(events[i], epoll, clientManager, router);
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