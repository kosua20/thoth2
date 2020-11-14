#include "system/Keychain.hpp"
#include "system/TextUtilities.hpp"

#ifdef _MACOS
#include <Security/Security.h>
#endif



bool Keychain::getPassword(const fs::path & server, const std::string & user, std::string & password){
		
#ifdef _MACOS
	UInt32 passLength;
	char * passBuffer;
	const std::string serverName = server.string();
	OSStatus stat = SecKeychainFindInternetPassword(nullptr, serverName.size(), serverName.c_str(), 0, nullptr, user.size(), user.c_str(), 0, nullptr, 0, kSecProtocolTypeSSH, kSecAuthenticationTypeDefault, &passLength, (void**)&passBuffer, nullptr);
	if(stat == 0){
		password = std::string(passBuffer, passLength);
		return true;
	}
#else
	Log::Error() << Log::Password << "No keychain available on your platform." << std::endl;
#endif
	return false;
}


bool Keychain::setPassword(const fs::path & server, const std::string & user, const std::string & password){
	
#ifdef _MACOS
	SecKeychainItemRef item;
	const std::string serverName = server.string();
	OSStatus stat = SecKeychainFindInternetPassword(nullptr, serverName.size(), serverName.c_str(), 0, nullptr, user.size(), user.c_str(), 0, nullptr, 0, kSecProtocolTypeSSH, kSecAuthenticationTypeDefault, 0, nullptr, &item);
	if(stat == 0){
		// If the item already exists, modify it.
		stat = SecKeychainItemModifyAttributesAndData(item, nullptr, password.size(), password.c_str());
	} else {
		// Else create it.
		stat = SecKeychainAddInternetPassword(nullptr, serverName.size(), serverName.c_str(), 0, nullptr, user.size(), user.c_str(), 0, nullptr, 0, kSecProtocolTypeSSH, kSecAuthenticationTypeDefault, password.size(), password.c_str(), nullptr);
	}
	return stat == 0;
#else
	Log::Error() << Log::Password << "No keychain available on your platform." << std::endl;
#endif
	return false;
}
