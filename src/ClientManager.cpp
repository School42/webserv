#include "ClientManager.hpp"
#include <iostream>

// Constructor
ClientManager::ClientManager(Epoll& epoll)
	: _epoll(epoll) {}

// Destructor
ClientManager::~ClientManager() {
	// Clean up all clients
	for (std::map<int, Client*>::iterator it = _clients.begin(); 
	     it != _clients.end(); ++it) {
		_epoll.remove(it->first);
		delete it->second;
	}
	_clients.clear();
}

// Add a new client
Client* ClientManager::addClient(int fd, const std::string& address, int port) {
	// Check if already exists
	if (_clients.find(fd) != _clients.end()) {
		return _clients[fd];
	}
	
	// Create new client
	Client* client = new Client(fd, address, port);
	_clients[fd] = client;
	
	// Add to epoll - initially interested in read events
	// Also add EPOLLRDHUP to detect peer close
	_epoll.add(fd, EVENT_READ | EVENT_RDHUP);
	
	return client;
}

// Remove a client
void ClientManager::removeClient(int fd) {
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it != _clients.end()) {
		_epoll.remove(fd);
		delete it->second;
		_clients.erase(it);
	}
}

// Get a client by fd
Client* ClientManager::getClient(int fd) {
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it != _clients.end()) {
		return it->second;
	}
	return NULL;
}

// Check if fd is a client
bool ClientManager::hasClient(int fd) const {
	return _clients.find(fd) != _clients.end();
}

// Get all client fds
std::vector<int> ClientManager::getAllClientFds() const {
	std::vector<int> fds;
	fds.reserve(_clients.size());
	
	for (std::map<int, Client*>::const_iterator it = _clients.begin();
	     it != _clients.end(); ++it) {
		fds.push_back(it->first);
	}
	
	return fds;
}

// Check and remove timed out clients
void ClientManager::checkTimeouts(time_t timeout) {
	std::vector<int> timedOut = getTimedOutClients(timeout);
	
	for (size_t i = 0; i < timedOut.size(); ++i) {
		std::cout << "Client " << timedOut[i] << " timed out, closing connection" << std::endl;
		removeClient(timedOut[i]);
	}
}

// Get list of timed out client fds
std::vector<int> ClientManager::getTimedOutClients(time_t timeout) const {
	std::vector<int> timedOut;
	
	for (std::map<int, Client*>::const_iterator it = _clients.begin();
	     it != _clients.end(); ++it) {
		if (it->second->isTimedOut(timeout)) {
			timedOut.push_back(it->first);
		}
	}
	
	return timedOut;
}

// Get client count
size_t ClientManager::getClientCount() const {
	return _clients.size();
}