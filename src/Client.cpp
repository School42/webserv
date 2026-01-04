#include "Client.hpp"
#include <unistd.h>
#include <cerrno>
#include <cstring>

// Constructor
Client::Client(int fd, const std::string& address, int port)
	: _fd(fd),
	  _address(address),
	  _port(port),
	  _state(STATE_READING_REQUEST),
	  _readBuffer(""),
	  _writeBuffer(""),
	  _writeOffset(0),
	  _lastActivity(std::time(NULL)),
	  _serverConfig(NULL),
	  _keepAlive(true),  // HTTP/1.1 defaults to keep-alive
	  _requestCount(0) {}

// Destructor
Client::~Client() {
	if (_fd >= 0) {
		::close(_fd);
		_fd = -1;
	}
}

// Read data from socket into buffer
ssize_t Client::readData() {
	char buffer[8192];
	
	ssize_t bytesRead = ::read(_fd, buffer, sizeof(buffer));
	
	if (bytesRead > 0) {
		_readBuffer.append(buffer, bytesRead);
		updateLastActivity();
	}
	
	return bytesRead;
}

// Write data from buffer to socket
ssize_t Client::writeData() {
	if (_writeOffset >= _writeBuffer.size()) {
		return 0;  // Nothing to write
	}
	
	size_t remaining = _writeBuffer.size() - _writeOffset;
	const char* data = _writeBuffer.c_str() + _writeOffset;
	
	ssize_t bytesWritten = ::write(_fd, data, remaining);
	
	if (bytesWritten > 0) {
		_writeOffset += bytesWritten;
		updateLastActivity();
		
		// If all data written, clear buffer
		if (_writeOffset >= _writeBuffer.size()) {
			clearWriteBuffer();
		}
	}
	
	return bytesWritten;
}

// Buffer management
void Client::appendToReadBuffer(const char* data, size_t len) {
	_readBuffer.append(data, len);
}

void Client::appendToWriteBuffer(const char* data, size_t len) {
	_writeBuffer.append(data, len);
}

void Client::appendToWriteBuffer(const std::string& data) {
	_writeBuffer.append(data);
}

void Client::clearReadBuffer() {
	_readBuffer.clear();
}

void Client::clearWriteBuffer() {
	_writeBuffer.clear();
	_writeOffset = 0;
}

// Getters
int Client::getFd() const {
	return _fd;
}

ClientState Client::getState() const {
	return _state;
}

const std::string& Client::getAddress() const {
	return _address;
}

int Client::getPort() const {
	return _port;
}

const std::string& Client::getReadBuffer() const {
	return _readBuffer;
}

const std::string& Client::getWriteBuffer() const {
	return _writeBuffer;
}

size_t Client::getReadBufferSize() const {
	return _readBuffer.size();
}

size_t Client::getWriteBufferSize() const {
	return _writeBuffer.size() - _writeOffset;
}

time_t Client::getLastActivity() const {
	return _lastActivity;
}

bool Client::hasDataToWrite() const {
	return _writeOffset < _writeBuffer.size();
}

// Setters
void Client::setState(ClientState state) {
	_state = state;
}

void Client::updateLastActivity() {
	_lastActivity = std::time(NULL);
}

// Server config association
void Client::setServerConfig(const ServerConfig* config) {
	_serverConfig = config;
}

const ServerConfig* Client::getServerConfig() const {
	return _serverConfig;
}

// Timeout check
bool Client::isTimedOut(time_t timeout) const {
	return (std::time(NULL) - _lastActivity) > timeout;
}

// Keep-alive
void Client::setKeepAlive(bool keepAlive) {
	_keepAlive = keepAlive;
}

bool Client::isKeepAlive() const {
	return _keepAlive;
}

// Request count
void Client::incrementRequestCount() {
	_requestCount++;
}

int Client::getRequestCount() const {
	return _requestCount;
}

// Reset for keep-alive
void Client::reset() {
	_state = STATE_READING_REQUEST;
	_readBuffer.clear();
	_writeBuffer.clear();
	_writeOffset = 0;
	_serverConfig = NULL;
	updateLastActivity();
}