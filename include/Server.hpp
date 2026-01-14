#pragma once
#include <vector>
#include <map>
#include <ctime>
#include "ServerConfig.hpp"
#include "Socket.hpp"
#include "Epoll.hpp"
#include "Client.hpp"
#include "ClientManager.hpp"
#include "Router.hpp"
#include "FileServer.hpp"
#include "CgiHandler.hpp"
#include "UploadHandler.hpp"

class Server {
public:
	// Constructor
	explicit Server(const std::vector<ServerConfig>& servers);
	
	// Destructor
	~Server();
	
	// Main server control
	void run();
	void stop();
	
	// Check if server is running
	bool isRunning() const;

private:
	// Non-copyable
	Server(const Server& other);
	Server& operator=(const Server& rhs);
	
	// Setup
	void setupListenSockets();
	void printStartupInfo() const;
	
	// Event loop
	void eventLoop();
	void checkTimeouts();
	
	// Event handlers
	void handleNewConnection(Socket* listenSocket);
	void handleClientEvent(const Event& event);
	void handleClientRead(Client* client);
	void handleClientWrite(Client* client);
	
	// Request processing
	void processRequest(Client* client);
	
	// Helpers
	bool isListenSocket(int fd) const;
	Socket* findListenSocket(int fd) const;
	
	// Members - Configuration
	std::vector<ServerConfig> _servers;
	
	// Members - Sockets
	std::vector<Socket*> _listenSockets;
	std::map<int, int> _fdToPort;  // Map client fd to listen port
	
	// Members - Core components
	Epoll _epoll;
	ClientManager _clientManager;
	Router _router;
	FileServer _fileServer;
	CgiHandler _cgiHandler;
	UploadHandler _uploadHandler;
	
	// Members - State
	bool _running;
	time_t _lastTimeoutCheck;
	
	// Constants
	static const time_t CLIENT_TIMEOUT = 60;
	static const int EPOLL_TIMEOUT = 1000;
	static const int MAX_KEEPALIVE_REQUESTS = 100;
};