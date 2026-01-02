#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <stdexcept>

class LocationConfig {
	public:
		explicit LocationConfig(const std::string& path);

		// setters used by parser
		void setRoot(const std::string& path);
		void addIndex(const std::string& file);
		void setAutoIndex(bool value);
		void addAllowedMethod(const std::string& method);
		void setReturn(const std::string& value, const std::string& code);
		void setCgiPass(const std::string& path);
		void setClientMaxBodySize(size_t size);
		void setUploadStore(const std::string& store);

		// getters
		const std::string& getPath() const;
		const std::string& getRoot() const;
		const std::vector<std::string>& getIndex() const;
		bool getAutoIndex() const;
		const std::vector<std::string>& getAllowedMethods() const;
		const std::string& getReturnValue() const;
		const std::string& getReturnCode() const;
		const std::string& getUploadStore() const;
		const std::vector<std::string>& getCgiPass() const;
		size_t getClientMaxBodySize() const;

		// presence checks
		bool hasRoot() const;
		bool hasAutoIndex() const;
		bool hasReturn() const;
		bool hasClientMaxBodySize() const;

	private:
		std::string path;
		std::string root;
		std::vector<std::string> index;
		bool autoindex;
		std::vector<std::string> allowed_methods;
		std::string return_directive;
		std::string return_code;
		std::string upload_store;
		std::vector<std::string> cgi_pass;
		size_t client_max_body_size;

		// duplicate & presence
		bool root_set;
		bool autoindex_set;
		bool return_set;
		bool upload_store_set;
		bool client_max_body_size_set;

		std::set<std::string> seen_index;
		std::set<std::string> seen_methods;
		std::set<std::string> seen_cgi_pass;
};