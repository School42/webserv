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

struct CgiSession {
    Client* client;
    
    pid_t pid;
    int stdoutFd;   // Read from CGI
    int stdinFd;    // Write to CGI
    
    time_t startTime;
    std::string inputBuffer;   // Request body to send to CGI
    size_t inputSent;          // How much input has been sent
    std::string outputBuffer;  // Accumulated CGI output
    bool inputComplete;        // All input sent to CGI
    
    RouteResult route;
    // Store request info as strings instead of copying HttpRequest
    std::string requestMethod;
    std::string requestUri;
    std::string clientIp;
    int clientPort;
    int serverPort;
    
    CgiSession()
        : client(NULL),
          pid(-1),
          stdoutFd(-1),
          stdinFd(-1),
          startTime(0),
          inputSent(0),
          inputComplete(false),
          clientPort(0),
          serverPort(0) {}
};

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
	void handleCgiEvent(const Event& event);
	
	// Request processing
	void processRequest(Client* client);
	
	// CGI session management
	void startCgiSession(Client* client, const RouteResult& route);
	void finalizeCgiSession(int cgiFd);
	void cleanupCgiSession(int cgiFd, bool sendError);
	void checkCgiTimeouts();
	
	// Helpers
	bool isListenSocket(int fd) const;
	bool isCgiPipe(int fd) const;
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
	std::map<int, CgiSession> _cgiSessions;
	
	// Members - State
	bool _running;
	time_t _lastTimeoutCheck;
	
	// Constants
	static const time_t CLIENT_TIMEOUT = 60;
	static const int EPOLL_TIMEOUT = 1000;
	static const int MAX_KEEPALIVE_REQUESTS = 100;
};