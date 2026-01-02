#pragma once
#include <string>
#include <vector>
#include <set>
#include <map>
#include <stdexcept>

class LocationConfig;

class ServerConfig{
	public:
		ServerConfig();

		// setter used by parser
		void addListen(int port);
		void addServerName(const std::string& name);
		void setRoot(const std::string& path);
		void setHost(const std::string& host);
		void addIndex(const std::string& file);
		void setClientMaxBodySize(size_t size);
		void addErrorPage(int code, const std::string& uri);
		void addLocation(const LocationConfig& location);

		// getters
		const std::vector<int>& getListenPorts() const;
		const std::vector<std::string>& getServerNames() const;
		const std::string& getRoot() const;
		const std::vector<std::string>& getIndex() const;
		size_t getClientMaxBodySize() const;
		const std::map<int, std::string>& getErrorPages() const;
		const std::vector<LocationConfig>& getLocations() const;

	private:

		std::vector<int> listen_ports;
		std::string host;
		std::vector<std::string> server_names;
		std::string root;
		std::vector<std::string> index;
		size_t client_max_body_size;
		std::map<int, std::string> error_pages;
		std::vector<LocationConfig> locations;

		// duplicate detection
		std::set<int> seen_listen;
		std::set<std::string> seen_server_names;
		std::set<std::string> seen_index;
		std::set<int> seen_error_codes;
		std::set<std::string> seen_location_paths;

		bool root_set;
		bool host_set;
		bool client_max_body_size_set;
};