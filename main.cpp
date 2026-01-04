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
#include "Lexer.hpp"
#include "Parser.hpp"
#include "ConfigError.hpp"

// Global flag for graceful shutdown
static volatile bool g_running = true;

// Timeout constants
static const time_t CLIENT_TIMEOUT = 60;  // 60 seconds
static const int EPOLL_TIMEOUT = 1000;    // 1 second (for timeout checks)
static const int MAX_KEEPALIVE_REQUESTS = 100;

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

// Build a simple error response
std::string buildErrorResponse(int statusCode, const std::string& statusText, const std::string& message) {
	std::stringstream body;
	body << "<html><head><title>" << statusCode << " " << statusText << "</title></head>"
	     << "<body><h1>" << statusCode << " " << statusText << "</h1>"
	     << "<p>" << message << "</p></body></html>";
	
	std::string bodyStr = body.str();
	
	std::stringstream response;
	response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
	         << "Content-Type: text/html\r\n"
	         << "Content-Length: " << bodyStr.size() << "\r\n"
	         << "Connection: close\r\n"
	         << "\r\n"
	         << bodyStr;
	
	return response.str();
}

// Build a simple success response
std::string buildSuccessResponse(const HttpRequest& request) {
	std::stringstream body;
	body << "<html><head><title>Request Received</title></head><body>"
	     << "<h1>Request Received</h1>"
	     << "<p><strong>Method:</strong> " << request.getMethod() << "</p>"
	     << "<p><strong>URI:</strong> " << request.getUri() << "</p>"
	     << "<p><strong>Path:</strong> " << request.getPath() << "</p>"
	     << "<p><strong>Query:</strong> " << request.getQueryString() << "</p>"
	     << "<p><strong>Host:</strong> " << request.getHost() << "</p>"
	     << "<p><strong>HTTP Version:</strong> " << request.getHttpVersion() << "</p>"
	     << "<h2>Headers:</h2><ul>";
	
	const std::map<std::string, std::string>& headers = request.getHeaders();
	for (std::map<std::string, std::string>::const_iterator it = headers.begin();
	     it != headers.end(); ++it) {
		body << "<li><strong>" << it->first << ":</strong> " << it->second << "</li>";
	}
	body << "</ul>";
	
	if (!request.getBody().empty()) {
		body << "<h2>Body:</h2><pre>" << request.getBody() << "</pre>";
	}
	
	body << "</body></html>";
	
	std::string bodyStr = body.str();
	
	std::stringstream response;
	response << "HTTP/1.1 200 OK\r\n"
	         << "Content-Type: text/html\r\n"
	         << "Content-Length: " << bodyStr.size() << "\r\n";
	
	if (request.isKeepAlive()) {
		response << "Connection: keep-alive\r\n";
	} else {
		response << "Connection: close\r\n";
	}
	
	response << "\r\n" << bodyStr;
	
	return response.str();
}

// Handle new connection on listen socket
void handleNewConnection(Socket* listenSocket, ClientManager& clientManager) {
	std::string clientAddr;
	int clientPort;
	
	int clientFd = listenSocket->accept(clientAddr, clientPort);
	
	if (clientFd >= 0) {
		Client* client = clientManager.addClient(clientFd, clientAddr, clientPort);
		std::cout << "New connection from " << clientAddr << ":" << clientPort 
		          << " (fd: " << clientFd << ") - Total clients: " 
		          << clientManager.getClientCount() << std::endl;
		(void)client;
	}
}

// Handle client read event
void handleClientRead(Client* client, Epoll& epoll, ClientManager& clientManager) {
	ssize_t bytesRead = client->readData();
	
	if (bytesRead < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			std::cerr << "Error reading from client " << client->getFd() 
			          << ": " << std::strerror(errno) << std::endl;
			clientManager.removeClient(client->getFd());
		}
		return;
	}
	
	if (bytesRead == 0) {
		std::cout << "Client " << client->getAddress() << ":" << client->getPort() 
		          << " closed connection" << std::endl;
		clientManager.removeClient(client->getFd());
		return;
	}
	
	// Parse the HTTP request
	HttpRequest& request = client->getRequest();
	size_t bytesConsumed = 0;
	HttpParseResult result = request.parse(client->getReadBuffer(), bytesConsumed);
	
	// Remove consumed bytes from buffer
	if (bytesConsumed > 0) {
		std::string& buffer = const_cast<std::string&>(client->getReadBuffer());
		buffer.erase(0, bytesConsumed);
	}
	
	if (result == PARSE_FAILED) {
		std::cerr << "Parse error from " << client->getAddress() << ": " 
		          << request.getErrorMessage() << std::endl;
		
		client->appendToWriteBuffer(buildErrorResponse(400, "Bad Request", request.getErrorMessage()));
		client->setState(STATE_WRITING_RESPONSE);
		client->setKeepAlive(false);
		epoll.modify(client->getFd(), EVENT_WRITE | EVENT_RDHUP);
		return;
	}
	
	if (result == PARSE_SUCCESS) {
		std::cout << "Request: " << request.getMethod() << " " << request.getUri() 
		          << " from " << client->getAddress() << std::endl;
		
		// TODO: Route request to proper handler based on config
		// For now, just return request info
		std::string response = buildSuccessResponse(request);
		
		client->appendToWriteBuffer(response);
		client->setState(STATE_WRITING_RESPONSE);
		client->setKeepAlive(request.isKeepAlive());
		epoll.modify(client->getFd(), EVENT_WRITE | EVENT_RDHUP);
	}
	// If PARSE_INCOMPLETE, wait for more data
}

// Handle client write event
void handleClientWrite(Client* client, Epoll& epoll, ClientManager& clientManager) {
	ssize_t bytesWritten = client->writeData();
	
	if (bytesWritten < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			std::cerr << "Error writing to client " << client->getFd() 
			          << ": " << std::strerror(errno) << std::endl;
			clientManager.removeClient(client->getFd());
		}
		return;
	}
	
	if (!client->hasDataToWrite()) {
		std::cout << "Response sent to " << client->getAddress() << ":" << client->getPort() << std::endl;
		
		if (client->isKeepAlive() && client->getRequestCount() < MAX_KEEPALIVE_REQUESTS) {
			client->incrementRequestCount();
			client->reset();
			epoll.modify(client->getFd(), EVENT_READ | EVENT_RDHUP);
			std::cout << "  (keeping connection alive, request #" << client->getRequestCount() << ")" << std::endl;
		} else {
			client->setState(STATE_DONE);
			clientManager.removeClient(client->getFd());
		}
	}
}

// Handle client event
void handleClientEvent(const Event& event, Epoll& epoll, ClientManager& clientManager) {
	Client* client = clientManager.getClient(event.fd);
	if (!client) {
		return;
	}
	
	if (event.isError() || event.isHangup() || event.isPeerClosed()) {
		std::cout << "Client " << client->getAddress() << " disconnected or error" << std::endl;
		clientManager.removeClient(event.fd);
		return;
	}
	
	switch (client->getState()) {
		case STATE_READING_REQUEST:
			if (event.isReadable()) {
				handleClientRead(client, epoll, clientManager);
			}
			break;
			
		case STATE_WRITING_RESPONSE:
			if (event.isWritable()) {
				handleClientWrite(client, epoll, clientManager);
			}
			break;
			
		case STATE_PROCESSING:
			// Will be used for CGI later
			break;
			
		case STATE_DONE:
		case STATE_ERROR:
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
		std::cout << "Total servers: " << servers.size() << std::endl;
		
		Epoll epoll;
		std::cout << "✓ Epoll instance created" << std::endl;
		
		ClientManager clientManager(epoll);
		
		std::vector<Socket*> listenSockets;
		std::map<int, int> fdToServerIndex;
		
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
				fdToServerIndex[sock->getFd()] = static_cast<int>(i);
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
					handleClientEvent(events[i], epoll, clientManager);
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
		listenSockets.clear();
		
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