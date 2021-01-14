#pragma once

#include "Common.hpp"

#include "system/System.hpp"

class Keychain {
public:
	
	static bool getPassword(const std::string & server, const std::string & user, std::string & password);
	
	static bool setPassword(const std::string & server, const std::string & user, const std::string & password);
	
private:
	
};
