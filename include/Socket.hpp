#pragma once
#include <string>
#include <netinet/in.h>
#include <stdexcept>

class Socket {
public:
	// Constructor - creates a socket
	Socket();
	
	// Constructor - wraps existing fd (for accepted connections)
	explicit Socket(int fd);
	
	// Destructor - closes socket
	~Socket();
	
	// Core operations
	void bind(const std::string& address, int port);
	void listen(int backlog = 128);
	int accept(std::string& clientAddr, int& clientPort);
	
	// Socket options
	void setReuseAddr(bool enable);
	void setNonBlocking(bool enable);
	
	// Getters
	int getFd() const;
	int getPort() const;
	const std::string& getAddress() const;
	bool isListening() const;
	
	// Close socket
	void close();

private:
	// Non-copyable
	Socket(const Socket& other);
	Socket& operator=(const Socket& rhs);
	
	// Members
	int _fd;
	std::string _address;
	int _port;
	bool _listening;
	bool _closed;
	
	// Helper
	static void setNonBlockingFd(int fd);
};

// Socket exception class
class SocketError : public std::runtime_error {
public:
	explicit SocketError(const std::string& message);
	SocketError(const std::string& message, int errnum);
	~SocketError() throw();
	
private:
	static std::string buildMessage(const std::string& message, int errnum);
};