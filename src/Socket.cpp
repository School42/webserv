#include "Socket.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <sstream>

// ============================================================================
// SocketError Implementation
// ============================================================================

std::string SocketError::buildMessage(const std::string& message, int errnum) {
	std::stringstream ss;
	ss << message;
	if (errnum != 0) {
		ss << ": " << std::strerror(errnum);
	}
	return ss.str();
}

SocketError::SocketError(const std::string& message)
	: std::runtime_error(message) {}

SocketError::SocketError(const std::string& message, int errnum)
	: std::runtime_error(buildMessage(message, errnum)) {}

SocketError::~SocketError() throw() {}

// ============================================================================
// Socket Implementation
// ============================================================================

// Default constructor - creates a new socket
Socket::Socket()
	: _fd(-1), _address(""), _port(0), _listening(false), _closed(false) {
	_fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (_fd < 0) {
		throw SocketError("Failed to create socket", errno);
	}
}

// Constructor wrapping existing fd (for accepted connections)
Socket::Socket(int fd)
	: _fd(fd), _address(""), _port(0), _listening(false), _closed(false) {
	if (_fd < 0) {
		throw SocketError("Invalid file descriptor");
	}
}

// Destructor
Socket::~Socket() {
	close();
}

// Bind to address and port
void Socket::bind(const std::string& address, int port) {
	if (_fd < 0 || _closed) {
		throw SocketError("Cannot bind: socket is closed");
	}
	
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET; // IPv4
	addr.sin_port = htons(static_cast<uint16_t>(port)); // Convert the port number to network byte order using htons()
	
	// Handle address
	if (address.empty() || address == "0.0.0.0") {
		addr.sin_addr.s_addr = INADDR_ANY; // Bind to all local interfaces
	} else {
		// inet_pton - convert char string src into a network address struct in af (address family)
		if (::inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
			throw SocketError("Invalid address: " + address);
		}
	}
	
	if (::bind(_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
		std::stringstream ss;
		ss << "Failed to bind to " << (address.empty() ? "0.0.0.0" : address) << ":" << port;
		throw SocketError(ss.str(), errno);
	}
	
	_address = address.empty() ? "0.0.0.0" : address;
	_port = port;
}

// Start listening
void Socket::listen(int backlog) {
	if (_fd < 0 || _closed) {
		throw SocketError("Cannot listen: socket is closed");
	}
	
	if (::listen(_fd, backlog) < 0) {
		throw SocketError("Failed to listen", errno);
	}
	
	_listening = true;
}

// Accept new connection
int Socket::accept(std::string& clientAddr, int& clientPort) {
	if (_fd < 0 || _closed) {
		throw SocketError("Cannot accept: socket is closed");
	}
	
	if (!_listening) {
		throw SocketError("Cannot accept: socket is not listening");
	}
	
	struct sockaddr_in addr;
	socklen_t addrLen = sizeof(addr);
	
	int clientFd = ::accept(_fd, reinterpret_cast<struct sockaddr*>(&addr), &addrLen);
	
	if (clientFd < 0) {
		// EAGAIN/EWOULDBLOCK means no pending connections (non-blocking mode)
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return -1;
		}
		throw SocketError("Failed to accept connection", errno);
	}
	
	// Extract client address info
	char ipStr[INET_ADDRSTRLEN];
	if (::inet_ntop(AF_INET, &addr.sin_addr, ipStr, sizeof(ipStr)) != NULL) {
		clientAddr = ipStr;
	} else {
		clientAddr = "unknown";
	}
	clientPort = ntohs(addr.sin_port);
	
	// Set the new client socket to non-blocking
	setNonBlockingFd(clientFd);
	
	return clientFd;
}

// Set SO_REUSEADDR option
void Socket::setReuseAddr(bool enable) {
	if (_fd < 0 || _closed) {
		throw SocketError("Cannot set option: socket is closed");
	}
	
	int optval = enable ? 1 : 0;
	// manipulate socket option , SO_REUSEADDR - to bind to a local address that is already in use
	// allows a socket to bind to a port that is already in use
	// to avoid "address already in use" errors when restarting quickly, as the old sockets may still be in the TIME_WAIT state
	if (::setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		throw SocketError("Failed to set SO_REUSEADDR", errno);
	}
}

// Set non-blocking mode
void Socket::setNonBlocking(bool enable) {
	if (_fd < 0 || _closed) {
		throw SocketError("Cannot set non-blocking: socket is closed");
	}
	
	if (enable) {
		setNonBlockingFd(_fd);
	} else {
		int flags = ::fcntl(_fd, F_GETFL, 0);
		if (flags < 0) {
			throw SocketError("Failed to get socket flags", errno);
		}
		if (::fcntl(_fd, F_SETFL, flags & ~O_NONBLOCK) < 0) {
			throw SocketError("Failed to clear non-blocking flag", errno);
		}
	}
}

// Static helper to set non-blocking on any fd
void Socket::setNonBlockingFd(int fd) {
	int flags = ::fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		throw SocketError("Failed to get socket flags", errno);
	}
	if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		throw SocketError("Failed to set non-blocking flag", errno);
	}
}

// Getters
int Socket::getFd() const {
	return _fd;
}

int Socket::getPort() const {
	return _port;
}

const std::string& Socket::getAddress() const {
	return _address;
}

bool Socket::isListening() const {
	return _listening;
}

// Close socket
void Socket::close() {
	if (_fd >= 0 && !_closed) {
		::close(_fd);
		_fd = -1;
		_closed = true;
		_listening = false;
	}
}