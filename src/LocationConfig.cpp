#include "LocationConfig.hpp"

// Constructor
LocationConfig::LocationConfig(const std::string& path)
	: _path(path),
	  _autoindex(false),
	  _client_max_body_size(0),
	  _root_set(false),
	  _autoindex_set(false),
	  _client_max_body_size_set(false),
	  _return_set(false),
	  _upload_store_set(false) {}

// Copy constructor
LocationConfig::LocationConfig(const LocationConfig& other)
	: _path(other._path),
	  _root(other._root),
	  _index(other._index),
	  _autoindex(other._autoindex),
	  _client_max_body_size(other._client_max_body_size),
	  _allowed_methods(other._allowed_methods),
	  _return_code(other._return_code),
	  _return_value(other._return_value),
	  _cgi_pass(other._cgi_pass),
	  _cgi_extension(other._cgi_extension),
	  _upload_store(other._upload_store),
	  _root_set(other._root_set),
	  _autoindex_set(other._autoindex_set),
	  _client_max_body_size_set(other._client_max_body_size_set),
	  _return_set(other._return_set),
	  _upload_store_set(other._upload_store_set),
	  _seen_index(other._seen_index),
	  _seen_methods(other._seen_methods),
	  _seen_cgi_pass(other._seen_cgi_pass),
	  _seen_cgi_extension(other._seen_cgi_extension) {}

// Copy assignment operator
LocationConfig& LocationConfig::operator=(const LocationConfig& rhs) {
	if (this != &rhs) {
		_path = rhs._path;
		_root = rhs._root;
		_index = rhs._index;
		_autoindex = rhs._autoindex;
		_client_max_body_size = rhs._client_max_body_size;
		_allowed_methods = rhs._allowed_methods;
		_return_code = rhs._return_code;
		_return_value = rhs._return_value;
		_cgi_pass = rhs._cgi_pass;
		_cgi_extension = rhs._cgi_extension;
		_upload_store = rhs._upload_store;
		_root_set = rhs._root_set;
		_autoindex_set = rhs._autoindex_set;
		_client_max_body_size_set = rhs._client_max_body_size_set;
		_return_set = rhs._return_set;
		_upload_store_set = rhs._upload_store_set;
		_seen_index = rhs._seen_index;
		_seen_methods = rhs._seen_methods;
		_seen_cgi_pass = rhs._seen_cgi_pass;
		_seen_cgi_extension = rhs._seen_cgi_extension;
	}
	return *this;
}

// Destructor
LocationConfig::~LocationConfig() {}

// Setters
void LocationConfig::setRoot(const std::string& path) {
	if (_root_set)
		throw std::runtime_error("Duplicate 'root' directive in location block");
	_root = path;
	_root_set = true;
}

void LocationConfig::addIndex(const std::string& file) {
	if (!_seen_index.insert(file).second)
		throw std::runtime_error("Duplicate index file: " + file);
	_index.push_back(file);
}

void LocationConfig::setAutoIndex(bool value) {
	if (_autoindex_set)
		throw std::runtime_error("Duplicate 'autoindex' directive in location block");
	_autoindex = value;
	_autoindex_set = true;
}

void LocationConfig::addAllowedMethod(const std::string& method) {
	if (!_seen_methods.insert(method).second)
		throw std::runtime_error("Duplicate allowed method: " + method);
	_allowed_methods.push_back(method);
}

void LocationConfig::setReturn(const std::string& code, const std::string& value) {
	if (_return_set)
		throw std::runtime_error("Duplicate 'return' directive in location block");
	_return_code = code;
	_return_value = value;
	_return_set = true;
}

void LocationConfig::addCgiPass(const std::string& path) {
	if (!_seen_cgi_pass.insert(path).second)
		throw std::runtime_error("Duplicate cgi_pass: " + path);
	_cgi_pass.push_back(path);
}

void LocationConfig::addCgiExtension(const std::string& ext) {
	if (!_seen_cgi_extension.insert(ext).second)
		throw std::runtime_error("Duplicate cgi_extension: " + ext);
	if (!(ext == ".py" || ext == ".sh" || ext == ".php" || ext == ".rb" || ext == ".pl")) {
		throw std::runtime_error("Not supported cgi_extenstion: " + ext);
	}
	_cgi_extension.push_back(ext);
}

void LocationConfig::setClientMaxBodySize(size_t size) {
	if (_client_max_body_size_set)
		throw std::runtime_error("Duplicate 'client_max_body_size' directive in location block");
	if (size > (100 * 1024 * 1024))
		throw std::runtime_error("'client_max_body_size' cannot exceed 100M");
	_client_max_body_size = size;
	_client_max_body_size_set = true;
}

void LocationConfig::setUploadStore(const std::string& store) {
	if (_upload_store_set)
		throw std::runtime_error("Duplicate 'upload_store' directive in location block");
	_upload_store = store;
	_upload_store_set = true;
}

// Getters
const std::string& LocationConfig::getPath() const { return _path; }
const std::string& LocationConfig::getRoot() const { return _root; }
const std::vector<std::string>& LocationConfig::getIndex() const { return _index; }
bool LocationConfig::getAutoIndex() const { return _autoindex; }
const std::vector<std::string>& LocationConfig::getAllowedMethods() const { return _allowed_methods; }
const std::string& LocationConfig::getReturnCode() const { return _return_code; }
const std::string& LocationConfig::getReturnValue() const { return _return_value; }
const std::vector<std::string>& LocationConfig::getCgiPass() const { return _cgi_pass; }
const std::vector<std::string>& LocationConfig::getCgiExtension() const { return _cgi_extension; }
size_t LocationConfig::getClientMaxBodySize() const { return _client_max_body_size; }
const std::string& LocationConfig::getUploadStore() const { return _upload_store; }

// Presence checks
bool LocationConfig::hasRoot() const { return _root_set; }
bool LocationConfig::hasIndex() const { return !_index.empty(); }
bool LocationConfig::hasAutoIndex() const { return _autoindex_set; }
bool LocationConfig::hasClientMaxBodySize() const { return _client_max_body_size_set; }

// Inheritance resolution
void LocationConfig::inheritFrom(const LocationConfig& parent) {
	// Inherit root if not set
	if (!_root_set && parent._root_set) {
		_root = parent._root;
		_root_set = true;
	}
	
	// Inherit index if not set
	if (_index.empty() && !parent._index.empty()) {
		_index = parent._index;
		_seen_index = parent._seen_index;
	}
	
	// Inherit autoindex if not set
	if (!_autoindex_set && parent._autoindex_set) {
		_autoindex = parent._autoindex;
		_autoindex_set = true;
	}
	
	// Inherit client_max_body_size if not set
	if (!_client_max_body_size_set && parent._client_max_body_size_set) {
		_client_max_body_size = parent._client_max_body_size;
		_client_max_body_size_set = true;
	}
}