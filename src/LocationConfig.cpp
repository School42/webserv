#include "LocationConfig.hpp"

LocationConfig::LocationConfig(const std::string& path)
	: path(path),
		autoindex(false),
		client_max_body_size(0),
		root_set(false),
		autoindex_set(false),
		return_set(false),
		upload_store_set(false),
		client_max_body_size_set(false) {}

void LocationConfig::setRoot(const std::string& path) {
	if (root_set)
		throw std::runtime_error("duplicate root in location");
	
	root = path;
	root_set = true;
}

void LocationConfig::setUploadStore(const std::string& store) {
	if (upload_store_set)
		throw std::runtime_error("duplicate upload_store directory in location");
	
	upload_store = store;
	upload_store_set = true;
}

void LocationConfig::addIndex(const std::string& file) {
	if (!seen_index.insert(file).second)
		throw std::runtime_error("duplicate index in location");

	index.push_back(file);
}

void LocationConfig::setAutoIndex(bool value) {
	if (autoindex_set)
		throw std::runtime_error("duplicate autoindex in location");

	autoindex = value;
	autoindex_set = true;
}

void LocationConfig::addAllowedMethod(const std::string& method) {
	if (!seen_methods.insert(method).second)
		throw std::runtime_error("duplicate allow_methods");
	
	allowed_methods.push_back(method);
}

void LocationConfig::setReturn(const std::string& value, const std::string& code) {
	if (return_set)
		throw std::runtime_error("duplicate return in location");
	return_directive = value;
	return_code = code;
	return_set = true;
}

void LocationConfig::setCgiPass(const std::string& path) {
	if (!seen_cgi_pass.insert(path).second)
		throw std::runtime_error("duplicate cgi_pass in location");
	
	cgi_pass.push_back(path);
}

void LocationConfig::setClientMaxBodySize(size_t size) {
	if (client_max_body_size_set)
		throw std::runtime_error("duplicate client_max_body_size in location");
	
	client_max_body_size = size;
	client_max_body_size_set = true;
}

bool LocationConfig::hasRoot() const { return root_set; }
bool LocationConfig::hasAutoIndex() const { return autoindex_set; }
bool LocationConfig::hasReturn() const { return return_set; }
bool LocationConfig::hasClientMaxBodySize() const { return client_max_body_size_set; }

// getters
const std::string& LocationConfig::getPath() const {return path;}
const std::string& LocationConfig::getRoot() const {return root;}
const std::vector<std::string>& LocationConfig::getIndex() const {return index;}
bool LocationConfig::getAutoIndex() const {return autoindex;}
const std::vector<std::string>& LocationConfig::getAllowedMethods() const {return allowed_methods;}
const std::string& LocationConfig::getReturnValue() const {return return_directive;}
const std::string& LocationConfig::getReturnCode() const {return return_code;}
const std::vector<std::string>& LocationConfig::getCgiPass() const {return cgi_pass;}
size_t LocationConfig::getClientMaxBodySize() const {return client_max_body_size;}
const std::string& LocationConfig::getUploadStore() const {return upload_store;}