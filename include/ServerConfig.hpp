#pragma once
#include <string>
#include <vector>
#include <set>
#include <map>
#include <stdexcept>

class LocationConfig;

// Struct to hold interface:port pair
struct ListenAddress {
	std::string interface; // IP address or empty for default (0.0.0.0)
	int port;              // Port number
	
	// Constructor
	ListenAddress() : interface(""), port(0) {}
	ListenAddress(const std::string& iface, int p) : interface(iface), port(p) {}
	
	// For use in std::set (comparison)
	bool operator<(const ListenAddress& other) const {
		if (port != other.port)
			return port < other.port;
		return interface < other.interface;
	}
	
	// Equality check
	bool operator==(const ListenAddress& other) const {
		return port == other.port && interface == other.interface;
	}
};

class ServerConfig {
public:
	// Orthodox Canonical Form
	ServerConfig();
	ServerConfig(const ServerConfig& other);
	ServerConfig& operator=(const ServerConfig& rhs);
	~ServerConfig();
	
	// Setters (used by parser)
	void addListen(const ListenAddress& addr);
	void addServerName(const std::string& name);
	void addErrorPage(int code, const std::string& uri);
	void addLocation(const LocationConfig& location);
	
	// Server-level directives that can be inherited by locations
	void setRoot(const std::string& path);
	void addIndex(const std::string& file);
	void setAutoIndex(bool value);
	void setClientMaxBodySize(size_t size);
	
	// Getters
	const std::vector<ListenAddress>& getListenAddresses() const;
	const std::vector<std::string>& getServerNames() const;
	const std::map<int, std::string>& getErrorPages() const;
	const std::vector<LocationConfig>& getLocations() const;
	std::vector<LocationConfig>& getLocations(); // non-const for inheritance resolution
	
	const std::string& getRoot() const;
	const std::vector<std::string>& getIndex() const;
	bool getAutoIndex() const;
	size_t getClientMaxBodySize() const;
	
	// Presence checks
	bool hasRoot() const;
	bool hasIndex() const;
	bool hasAutoIndex() const;
	bool hasClientMaxBodySize() const;
	
	// Create a "parent" LocationConfig for inheritance
	LocationConfig createParentConfig() const;
	
	// Resolve inheritance for all locations
	void resolveLocationInheritance();

private:
	// Server-only directives
	std::vector<ListenAddress> _listen_addresses;
	std::vector<std::string> _server_names;
	std::map<int, std::string> _error_pages;
	
	// Directives that can be inherited by locations
	std::string _root;
	std::vector<std::string> _index;
	bool _autoindex;
	size_t _client_max_body_size;
	
	// Locations
	std::vector<LocationConfig> _locations;
	
	// Flags for presence tracking
	bool _root_set;
	bool _autoindex_set;
	bool _client_max_body_size_set;
	
	// Duplicate detection
	std::set<ListenAddress> _seen_listen;
	std::set<std::string> _seen_server_names;
	std::set<std::string> _seen_index;
	std::set<int> _seen_error_codes;
	std::set<std::string> _seen_location_paths;
};