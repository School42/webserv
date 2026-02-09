#include "ServerConfig.hpp"
#include "LocationConfig.hpp"
#include <sstream>
#include <iostream>

// Constructor
ServerConfig::ServerConfig()
	: _autoindex(false),
	  _client_max_body_size(0),
	  _root_set(false),
	  _autoindex_set(false),
	  _client_max_body_size_set(false) {}

// Copy constructor
ServerConfig::ServerConfig(const ServerConfig& other)
	: _listen_addresses(other._listen_addresses),
	  _server_names(other._server_names),
	  _error_pages(other._error_pages),
	  _root(other._root),
	  _index(other._index),
	  _autoindex(other._autoindex),
	  _client_max_body_size(other._client_max_body_size),
	  _locations(other._locations),
	  _root_set(other._root_set),
	  _autoindex_set(other._autoindex_set),
	  _client_max_body_size_set(other._client_max_body_size_set),
	  _seen_listen(other._seen_listen),
	  _seen_server_names(other._seen_server_names),
	  _seen_index(other._seen_index),
	  _seen_error_codes(other._seen_error_codes),
	  _seen_location_paths(other._seen_location_paths) {}

// Copy assignment operator
ServerConfig& ServerConfig::operator=(const ServerConfig& rhs) {
	if (this != &rhs) {
		_listen_addresses = rhs._listen_addresses;
		_server_names = rhs._server_names;
		_error_pages = rhs._error_pages;
		_root = rhs._root;
		_index = rhs._index;
		_autoindex = rhs._autoindex;
		_client_max_body_size = rhs._client_max_body_size;
		_locations = rhs._locations;
		_root_set = rhs._root_set;
		_autoindex_set = rhs._autoindex_set;
		_client_max_body_size_set = rhs._client_max_body_size_set;
		_seen_listen = rhs._seen_listen;
		_seen_server_names = rhs._seen_server_names;
		_seen_index = rhs._seen_index;
		_seen_error_codes = rhs._seen_error_codes;
		_seen_location_paths = rhs._seen_location_paths;
	}
	return *this;
}

// Destructor
ServerConfig::~ServerConfig() {}

// Setters - Server-only directives
void ServerConfig::addListen(const ListenAddress& addr) {
	if (!_seen_listen.insert(addr).second) {
		std::stringstream ss;
		if (addr.interface.empty())
			ss << "Duplicate listen address: " << addr.port;
		else
			ss << "Duplicate listen address: " << addr.interface << ":" << addr.port;
		throw std::runtime_error(ss.str());
	}
	_listen_addresses.push_back(addr);
}

void ServerConfig::addServerName(const std::string& name) {
	if (!_seen_server_names.insert(name).second)
		throw std::runtime_error("Duplicate server_name: " + name);
	_server_names.push_back(name);
}

void ServerConfig::addErrorPage(int code, const std::string& uri) {
	if (!_seen_error_codes.insert(code).second)
		throw std::runtime_error("Duplicate error_page code");
	_error_pages[code] = uri;
}

void ServerConfig::addLocation(const LocationConfig& location) {
	const std::string& path = location.getPath();
	if (!_seen_location_paths.insert(path).second)
		throw std::runtime_error("Duplicate location path: " + path);
	_locations.push_back(location);
}

// Setters - Inheritable directives
void ServerConfig::setRoot(const std::string& path) {
	if (_root_set)
		throw std::runtime_error("Duplicate 'root' directive in server block");
	_root = path;
	_root_set = true;
}

void ServerConfig::addIndex(const std::string& file) {
	if (!_seen_index.insert(file).second)
		throw std::runtime_error("Duplicate index file: " + file);
	_index.push_back(file);
}

void ServerConfig::setAutoIndex(bool value) {
	if (_autoindex_set)
		throw std::runtime_error("Duplicate 'autoindex' directive in server block");
	_autoindex = value;
	_autoindex_set = true;
}

void ServerConfig::setClientMaxBodySize(size_t size) {
	if (_client_max_body_size_set)
		throw std::runtime_error("Duplicate 'client_max_body_size' directive in server block");
	if (size > (1024 * 1024 * 1024))
		throw std::runtime_error("'client_max_body_size' cannot exceed 1024M");
	_client_max_body_size = size;
	_client_max_body_size_set = true;
}

// Getters
const std::vector<ListenAddress>& ServerConfig::getListenAddresses() const { 
	return _listen_addresses; 
}
const std::vector<std::string>& ServerConfig::getServerNames() const { 
	return _server_names; 
}
const std::map<int, std::string>& ServerConfig::getErrorPages() const { 
	return _error_pages; 
}
const std::vector<LocationConfig>& ServerConfig::getLocations() const { 
	return _locations; 
}
std::vector<LocationConfig>& ServerConfig::getLocations() { 
	return _locations; 
}

const std::string& ServerConfig::getRoot() const { return _root; }
const std::vector<std::string>& ServerConfig::getIndex() const { return _index; }
bool ServerConfig::getAutoIndex() const { return _autoindex; }
size_t ServerConfig::getClientMaxBodySize() const { return _client_max_body_size; }

// Presence checks
bool ServerConfig::hasRoot() const { return _root_set; }
bool ServerConfig::hasIndex() const { return !_index.empty(); }
bool ServerConfig::hasAutoIndex() const { return _autoindex_set; }
bool ServerConfig::hasClientMaxBodySize() const { return _client_max_body_size_set; }

// Create a "parent" LocationConfig for inheritance
LocationConfig ServerConfig::createParentConfig() const {
	// Create a dummy location to hold server-level inheritable values
	LocationConfig parent("_server_parent_");
	
	if (_root_set)
		parent.setRoot(_root);
	
	for (size_t i = 0; i < _index.size(); ++i)
		parent.addIndex(_index[i]);
	
	if (_autoindex_set)
		parent.setAutoIndex(_autoindex);
	
	if (_client_max_body_size_set)
		parent.setClientMaxBodySize(_client_max_body_size);
	
	return parent;
}

// Resolve inheritance for all locations
void ServerConfig::resolveLocationInheritance() {
	LocationConfig parent = createParentConfig();
	
	for (size_t i = 0; i < _locations.size(); ++i) {
		_locations[i].inheritFrom(parent);
	}
}