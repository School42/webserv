#include "ServerConfig.hpp"
#include "LocationConfig.hpp"

ServerConfig::ServerConfig()
	: client_max_body_size(0),
		root_set(false),
		host_set(false),
		client_max_body_size_set(false) {}

void ServerConfig::addListen(int port) {
	if (!seen_listen.insert(port).second)
		throw std::runtime_error("duplicate listen port");
	
	listen_ports.push_back(port);
}

void ServerConfig::addServerName(const std::string& name) {
	if (!seen_server_names.insert(name).second)
		throw std::runtime_error("duplicate server_name");
	server_names.push_back(name);
}

void ServerConfig::setRoot(const std::string& path) {
	if (root_set)
		throw std::runtime_error("duplicate root directive");
	
	root = path;
	root_set = true;
}

void ServerConfig::setHost(const std::string& host) {
	if (host_set)
		throw std::runtime_error("duplicate host directive");
	
	this->host = host;
	host_set = true;
}

void ServerConfig::setClientMaxBodySize(size_t size) {
	if (client_max_body_size_set)
		throw std::runtime_error("duplicate client_max_body_size");
	
	client_max_body_size = size;
	client_max_body_size_set = true;
}

void ServerConfig::addIndex(const std::string& file) {
	if (!seen_index.insert(file).second)
		throw std::runtime_error("duplicate index entry");
	
	index.push_back(file);
}

void ServerConfig::addErrorPage(int code, const std::string& uri) {
	if (!seen_error_codes.insert(code).second)
		throw std::runtime_error("duplicate error_page code");
	
	error_pages[code] = uri;
}

void ServerConfig::addLocation(const LocationConfig& location) {
	const std::string& path = location.getPath();

	if (!seen_location_paths.insert(path).second)
		throw std::runtime_error("duplicate location path");
	
	locations.push_back(location);
}

// getters
const std::vector<int>& ServerConfig::getListenPorts() const {return listen_ports;}
const std::vector<std::string>& ServerConfig::getServerNames() const {return server_names;}
const std::string& ServerConfig::getRoot() const {return root;}
const std::vector<std::string>& ServerConfig::getIndex() const {return index;}
size_t ServerConfig::getClientMaxBodySize() const {return client_max_body_size;}
const std::map<int, std::string>& ServerConfig::getErrorPages() const {return error_pages;}
const std::vector<LocationConfig>& ServerConfig::getLocations() const {return locations;}