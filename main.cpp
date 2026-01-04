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
		(void)client;  // Will be used later
	}
}

// Handle client read event
void handleClientRead(Client* client, Epoll& epoll, ClientManager& clientManager) {
	ssize_t bytesRead = client->readData();
	
	if (bytesRead < 0) {
		// Error reading (EAGAIN/EWOULDBLOCK means try again later)
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			std::cerr << "Error reading from client " << client->getFd() 
			          << ": " << std::strerror(errno) << std::endl;
			clientManager.removeClient(client->getFd());
		}
		return;
	}
	
	if (bytesRead == 0) {
		// Client closed connection
		std::cout << "Client " << client->getAddress() << ":" << client->getPort() 
		          << " closed connection (fd: " << client->getFd() << ")" << std::endl;
		clientManager.removeClient(client->getFd());
		return;
	}
	
	// Data received - for now, check if we have a complete HTTP request
	// (Simple check: look for \r\n\r\n which marks end of headers)
	const std::string& buffer = client->getReadBuffer();
	size_t headerEnd = buffer.find("\r\n\r\n");
	
	if (headerEnd != std::string::npos) {
		// We have complete headers - for now, send a simple response
		std::cout << "Received complete request from " << client->getAddress() 
		          << " (" << buffer.size() << " bytes)" << std::endl;
		
		// TODO: Parse request and generate proper response
		// For now, send a simple response
		std::string response = 
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: 48\r\n"
			"Connection: close\r\n"
			"\r\n"
			"<html><body><h1>Hello World!</h1></body></html>";
		
		client->appendToWriteBuffer(response);
		client->clearReadBuffer();
		client->setState(STATE_WRITING_RESPONSE);
		client->setKeepAlive(false);  // For now, close after response
		
		// Switch to write mode in epoll
		epoll.modify(client->getFd(), EVENT_WRITE | EVENT_RDHUP);
	}
	
	// If buffer is too large, reject (will be replaced by proper request parsing)
	if (buffer.size() > 1024 * 1024) {
		std::cerr << "Request too large from client " << client->getFd() << std::endl;
		clientManager.removeClient(client->getFd());
	}
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
	
	// Check if all data has been written
	if (!client->hasDataToWrite()) {
		std::cout << "Response sent to " << client->getAddress() << ":" << client->getPort() << std::endl;
		
		if (client->isKeepAlive() && client->getRequestCount() < MAX_KEEPALIVE_REQUESTS) {
			// Keep connection alive for next request
			client->incrementRequestCount();
			client->reset();
			epoll.modify(client->getFd(), EVENT_READ | EVENT_RDHUP);
			std::cout << "Keeping connection alive (request #" << client->getRequestCount() << ")" << std::endl;
		} else {
			// Close connection
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
	
	// Check for errors or hangup
	if (event.isError() || event.isHangup() || event.isPeerClosed()) {
		std::cout << "Client " << client->getAddress() << " disconnected or error" << std::endl;
		clientManager.removeClient(event.fd);
		return;
	}
	
	// Handle based on state
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
	// Setup signal handlers
	std::signal(SIGINT, signalHandler);
	std::signal(SIGTERM, signalHandler);
	std::signal(SIGPIPE, SIG_IGN);  // Ignore SIGPIPE (handle write errors instead)
	
	try {
		if (argc != 2) {
			std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
			return 1;
		}
		
		// Check file extension
		std::string filename = argv[1];
		if (filename.length() < 6 || filename.substr(filename.length() - 5) != ".conf") {
			std::cerr << "Error: Configuration file must have .conf extension" << std::endl;
			return 1;
		}
		
		// Read config file
		std::ifstream file(argv[1]);
		if (!file) {
			std::cerr << "Error: Cannot open file: " << argv[1] << std::endl;
			return 1;
		}
		
		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string config = buffer.str();
		file.close();
		
		// Parse configuration
		Lexer lexer(config);
		Parser parser(lexer);
		std::vector<ServerConfig> servers = parser.parse();
		
		std::cout << "✓ Configuration parsed successfully!" << std::endl;
		std::cout << "Total servers: " << servers.size() << std::endl;
		
		// Create epoll instance
		Epoll epoll;
		std::cout << "✓ Epoll instance created" << std::endl;
		
		// Create client manager
		ClientManager clientManager(epoll);
		
		// Create listening sockets
		std::vector<Socket*> listenSockets;
		std::map<int, int> fdToServerIndex;  // Map listen fd to server index
		
		for (size_t i = 0; i < servers.size(); ++i) {
			const std::vector<ListenAddress>& addrs = servers[i].getListenAddresses();
			
			for (size_t j = 0; j < addrs.size(); ++j) {
				// Check for duplicate
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
		std::cout << "Active clients: 0" << std::endl;
		
		// Main event loop
		std::vector<Event> events;
		time_t lastTimeoutCheck = std::time(NULL);
		
		while (g_running) {
			int numEvents = epoll.wait(events, EPOLL_TIMEOUT);
			
			// Process events
			for (int i = 0; i < numEvents; ++i) {
				int fd = events[i].fd;
				
				if (isListenSocket(fd, listenSockets)) {
					// New connection
					if (events[i].isReadable()) {
						Socket* listenSock = findListenSocket(fd, listenSockets);
						if (listenSock) {
							handleNewConnection(listenSock, clientManager);
						}
					}
				} else if (clientManager.hasClient(fd)) {
					// Client event
					handleClientEvent(events[i], epoll, clientManager);
				}
			}
			
			// Periodic timeout check (every second)
			time_t now = std::time(NULL);
			if (now - lastTimeoutCheck >= 1) {
				clientManager.checkTimeouts(CLIENT_TIMEOUT);
				lastTimeoutCheck = now;
			}
		}
		
		// Cleanup
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