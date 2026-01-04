#include <iostream>
#include <vector>
#include "Socket.hpp"
#include "Epoll.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "ConfigError.hpp"
#include <fstream>
#include <sstream>
#include <csignal>
#include <cstring>

// Global flag for graceful shutdown
static volatile bool g_running = true;

void signalHandler(int signum) {
	(void)signum;
	std::cout << "\nReceived shutdown signal..." << std::endl;
	g_running = false;
}

int main(int argc, char** argv) {
	// Setup signal handlers
	std::signal(SIGINT, signalHandler);
	std::signal(SIGTERM, signalHandler);
	
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
		std::cout << "✓ Epoll instance created (fd: " << epoll.getFd() << ")" << std::endl;
		
		// Create listening sockets for each server's listen addresses
		std::vector<Socket*> listenSockets;
		
		for (size_t i = 0; i < servers.size(); ++i) {
			const std::vector<ListenAddress>& addrs = servers[i].getListenAddresses();
			
			for (size_t j = 0; j < addrs.size(); ++j) {
				// Check if we already have a socket for this address
				bool exists = false;
				for (size_t k = 0; k < listenSockets.size(); ++k) {
					if (listenSockets[k]->getPort() == addrs[j].port &&
					    listenSockets[k]->getAddress() == (addrs[j].interface.empty() ? "0.0.0.0" : addrs[j].interface)) {
						exists = true;
						break;
					}
				}
				
				if (exists) {
					std::cout << "  (Skipping duplicate address)" << std::endl;
					continue;
				}
				
				Socket* sock = new Socket();
				sock->setReuseAddr(true);
				sock->setNonBlocking(true);
				sock->bind(addrs[j].interface, addrs[j].port);
				sock->listen();
				
				// Add to epoll for read events
				epoll.add(sock->getFd(), EVENT_READ);
				
				listenSockets.push_back(sock);
				
				std::cout << "✓ Listening on " 
				          << (addrs[j].interface.empty() ? "0.0.0.0" : addrs[j].interface)
				          << ":" << addrs[j].port 
				          << " (fd: " << sock->getFd() << ")" << std::endl;
			}
		}
		
		std::cout << "\n=== Server is running. Press Ctrl+C to stop ===\n" << std::endl;
		
		// Simple event loop - just accept and immediately close connections
		std::vector<Event> events;
		
		while (g_running) {
			int numEvents = epoll.wait(events, 1000); // 1 second timeout
			
			for (int i = 0; i < numEvents; ++i) {
				// Find which listen socket this is
				for (size_t j = 0; j < listenSockets.size(); ++j) {
					if (events[i].fd == listenSockets[j]->getFd()) {
						if (events[i].isReadable()) {
							std::string clientAddr;
							int clientPort;
							int clientFd = listenSockets[j]->accept(clientAddr, clientPort);
							
							if (clientFd >= 0) {
								std::cout << "Accepted connection from " 
								          << clientAddr << ":" << clientPort 
								          << " (fd: " << clientFd << ")" << std::endl;
								
								// For now, just send a simple response and close
								const char* response = 
									"HTTP/1.1 200 OK\r\n"
									"Content-Type: text/plain\r\n"
									"Content-Length: 13\r\n"
									"Connection: close\r\n"
									"\r\n"
									"Hello World!\n";
								
								write(clientFd, response, std::strlen(response));
								close(clientFd);
								
								std::cout << "Sent response and closed connection" << std::endl;
							}
						}
						break;
					}
				}
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