#pragma once
#include <map>
#include <vector>
#include "Client.hpp"
#include "Epoll.hpp"

class ClientManager {
public:
	// Constructor
	ClientManager(Epoll& epoll);
	
	// Destructor
	~ClientManager();
	
	// Client lifecycle
	Client* addClient(int fd, const std::string& address, int port);
	void removeClient(int fd);
	Client* getClient(int fd);
	
	// Check if fd is a client
	bool hasClient(int fd) const;
	
	// Get all client fds (for iteration)
	std::vector<int> getAllClientFds() const;
	
	// Timeout management
	void checkTimeouts(time_t timeout);
	std::vector<int> getTimedOutClients(time_t timeout) const;
	
	// Stats
	size_t getClientCount() const;

private:
	// Non-copyable
	ClientManager(const ClientManager& other);
	ClientManager& operator=(const ClientManager& rhs);
	
	// Members
	Epoll& _epoll;
	std::map<int, Client*> _clients;
};