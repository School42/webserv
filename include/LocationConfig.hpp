#pragma once
#include <string>
#include <vector>
#include <set>
#include <stdexcept>

class LocationConfig {
public:
	// Orthodox Canonical Form
	explicit LocationConfig(const std::string& path);
	LocationConfig(const LocationConfig& other);
	LocationConfig& operator=(const LocationConfig& rhs);
	~LocationConfig();
	
	// Setters (used by parser)
	void setRoot(const std::string& path);
	void addIndex(const std::string& file);
	void setAutoIndex(bool value);
	void addAllowedMethod(const std::string& method);
	void setReturn(const std::string& code, const std::string& value);
	void addCgiPass(const std::string& path);
	void addCgiExtension(const std::string& ext);
	void setClientMaxBodySize(size_t size);
	void setUploadStore(const std::string& store);
	
	// Getters
	const std::string& getPath() const;
	const std::string& getRoot() const;
	const std::vector<std::string>& getIndex() const;
	bool getAutoIndex() const;
	const std::vector<std::string>& getAllowedMethods() const;
	const std::string& getReturnCode() const;
	const std::string& getReturnValue() const;
	const std::vector<std::string>& getCgiPass() const;
	const std::vector<std::string>& getCgiExtension() const;
	size_t getClientMaxBodySize() const;
	const std::string& getUploadStore() const;
	
	// Presence checks (for inheritance resolution)
	bool hasRoot() const;
	bool hasIndex() const;
	bool hasAutoIndex() const;
	bool hasClientMaxBodySize() const;
	
	// Inheritance resolution (called by parser after parsing)
	void inheritFrom(const LocationConfig& parent);

private:
	// Path (required, immutable)
	std::string _path;
	
	// Directives (can be inherited from server or set explicitly)
	std::string _root;
	std::vector<std::string> _index;
	bool _autoindex;
	size_t _client_max_body_size;
	
	// Location-only directives
	std::vector<std::string> _allowed_methods;
	std::string _return_code;
	std::string _return_value;
	std::vector<std::string> _cgi_pass;
	std::vector<std::string> _cgi_extension;
	std::string _upload_store;
	
	// Flags to track what has been explicitly set
	bool _root_set;
	bool _autoindex_set;
	bool _client_max_body_size_set;
	bool _return_set;
	bool _upload_store_set;
	
	// Duplicate detection sets
	std::set<std::string> _seen_index;
	std::set<std::string> _seen_methods;
	std::set<std::string> _seen_cgi_pass;
	std::set<std::string> _seen_cgi_extension;
};