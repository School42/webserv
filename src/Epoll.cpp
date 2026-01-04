#include "Epoll.hpp"
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sstream>

// ============================================================================
// EpollError Implementation
// ============================================================================

std::string EpollError::buildMessage(const std::string& message, int errnum) {
	std::stringstream ss;
	ss << message;
	if (errnum != 0) {
		ss << ": " << std::strerror(errnum);
	}
	return ss.str();
}

EpollError::EpollError(const std::string& message)
	: std::runtime_error(message) {}

EpollError::EpollError(const std::string& message, int errnum)
	: std::runtime_error(buildMessage(message, errnum)) {}

EpollError::~EpollError() throw() {}

// ============================================================================
// Epoll Implementation
// ============================================================================

// Constructor
Epoll::Epoll() : _epollFd(-1) {
	_epollFd = ::epoll_create1(0);
	if (_epollFd < 0) {
		throw EpollError("Failed to create epoll instance", errno);
	}
}

// Destructor
Epoll::~Epoll() {
	if (_epollFd >= 0) {
		::close(_epollFd);
		_epollFd = -1;
	}
}

// Add fd to epoll
void Epoll::add(int fd, uint32_t events) {
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = fd;
	
	if (::epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		std::stringstream ss;
		ss << "Failed to add fd " << fd << " to epoll";
		throw EpollError(ss.str(), errno);
	}
}

// Modify events for fd
void Epoll::modify(int fd, uint32_t events) {
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = fd;
	
	if (::epoll_ctl(_epollFd, EPOLL_CTL_MOD, fd, &ev) < 0) {
		std::stringstream ss;
		ss << "Failed to modify fd " << fd << " in epoll";
		throw EpollError(ss.str(), errno);
	}
}

// Remove fd from epoll
void Epoll::remove(int fd) {
	// Note: In Linux 2.6.9+, the event parameter can be NULL for EPOLL_CTL_DEL
	if (::epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL) < 0) {
		// Don't throw if fd was already removed or closed
		if (errno != ENOENT && errno != EBADF) {
			std::stringstream ss;
			ss << "Failed to remove fd " << fd << " from epoll";
			throw EpollError(ss.str(), errno);
		}
	}
}

// Wait for events
int Epoll::wait(std::vector<Event>& events, int timeout) {
	int numEvents = ::epoll_wait(_epollFd, _eventBuffer, MAX_EVENTS, timeout);
	
	if (numEvents < 0) {
		// EINTR means interrupted by signal - not an error
		if (errno == EINTR) {
			events.clear();
			return 0;
		}
		throw EpollError("epoll_wait failed", errno);
	}
	
	events.clear();
	events.reserve(numEvents);
	
	for (int i = 0; i < numEvents; ++i) {
		Event ev;
		ev.fd = _eventBuffer[i].data.fd;
		ev.events = _eventBuffer[i].events;
		events.push_back(ev);
	}
	
	return numEvents;
}

// Getter
int Epoll::getFd() const {
	return _epollFd;
}