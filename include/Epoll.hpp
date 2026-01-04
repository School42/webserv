#pragma once
#include <sys/epoll.h>
#include <vector>
#include <stdexcept>

// Event types (wrapper around epoll events)
enum EventType {
	EVENT_READ   = EPOLLIN,
	EVENT_WRITE  = EPOLLOUT,
	EVENT_ERROR  = EPOLLERR,
	EVENT_HANGUP = EPOLLHUP,
	EVENT_RDHUP  = EPOLLRDHUP  // Peer closed connection
};

// Event structure returned by wait()
struct Event {
	int fd;
	uint32_t events;
	
	bool isReadable() const  { return events & EVENT_READ; }
	bool isWritable() const  { return events & EVENT_WRITE; }
	bool isError() const     { return events & EVENT_ERROR; }
	bool isHangup() const    { return events & EVENT_HANGUP; }
	bool isPeerClosed() const { return events & EVENT_RDHUP; }
};

class Epoll {
public:
	// Constructor - creates epoll instance
	Epoll();
	
	// Destructor - closes epoll fd
	~Epoll();
	
	// Add fd to epoll
	void add(int fd, uint32_t events);
	
	// Modify events for fd
	void modify(int fd, uint32_t events);
	
	// Remove fd from epoll
	void remove(int fd);
	
	// Wait for events (timeout in milliseconds, -1 for infinite)
	int wait(std::vector<Event>& events, int timeout = -1);
	
	// Getters
	int getFd() const;

private:
	// Non-copyable
	Epoll(const Epoll& other);
	Epoll& operator=(const Epoll& rhs);
	
	// Members
	int _epollFd;
	static const int MAX_EVENTS = 1024;
	struct epoll_event _eventBuffer[MAX_EVENTS];
};

// Epoll exception class
class EpollError : public std::runtime_error {
public:
	explicit EpollError(const std::string& message);
	EpollError(const std::string& message, int errnum);
	~EpollError() throw();
	
private:
	static std::string buildMessage(const std::string& message, int errnum);
};