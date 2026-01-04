#pragma once
#include <string>
#include <ctime>
#include <sys/types.h>
#include "ServerConfig.hpp"
#include "HttpRequest.hpp"

// Client connection states
enum ClientState {
	STATE_READING_REQUEST,    // Reading HTTP request
	STATE_PROCESSING,         // Processing request (CGI, file read, etc.)
	STATE_WRITING_RESPONSE,   // Writing HTTP response
	STATE_DONE,               // Ready to close or keep-alive
	STATE_ERROR               // Error occurred
};

class Client {
public:
	// Constructor
	Client(int fd, const std::string& address, int port);
	
	// Destructor
	~Client();
	
	// I/O operations
	ssize_t readData();      // Read from socket into buffer
	ssize_t writeData();     // Write from buffer to socket
	
	// Buffer management
	void appendToReadBuffer(const char* data, size_t len);
	void appendToWriteBuffer(const char* data, size_t len);
	void appendToWriteBuffer(const std::string& data);
	void clearReadBuffer();
	void clearWriteBuffer();
	
	// Getters
	int getFd() const;
	ClientState getState() const;
	const std::string& getAddress() const;
	int getPort() const;
	const std::string& getReadBuffer() const;
	const std::string& getWriteBuffer() const;
	size_t getReadBufferSize() const;
	size_t getWriteBufferSize() const;
	time_t getLastActivity() const;
	bool hasDataToWrite() const;
	
	// Setters
	void setState(ClientState state);
	void updateLastActivity();
	
	// Server config association
	void setServerConfig(const ServerConfig* config);
	const ServerConfig* getServerConfig() const;
	
	// HTTP request access
	HttpRequest& getRequest();
	const HttpRequest& getRequest() const;
	
	// Timeout check
	bool isTimedOut(time_t timeout) const;
	
	// Keep-alive
	void setKeepAlive(bool keepAlive);
	bool isKeepAlive() const;
	
	// Request count (for keep-alive limit)
	void incrementRequestCount();
	int getRequestCount() const;
	
	// Reset for keep-alive (new request on same connection)
	void reset();

private:
	// Non-copyable
	Client(const Client& other);
	Client& operator=(const Client& rhs);
	
	// Connection info
	int _fd;
	std::string _address;
	int _port;
	
	// State
	ClientState _state;
	
	// Buffers
	std::string _readBuffer;
	std::string _writeBuffer;
	size_t _writeOffset;  // How much of write buffer has been sent
	
	// Timing
	time_t _lastActivity;
	
	// Associated server config (set after Host header is parsed)
	const ServerConfig* _serverConfig;
	
	// HTTP request parser
	HttpRequest _request;
	
	// Keep-alive
	bool _keepAlive;
	int _requestCount;
	
	// Buffer limits
	static const size_t MAX_READ_BUFFER = 1024 * 1024;  // 1MB default (overridden by client_max_body_size)
};