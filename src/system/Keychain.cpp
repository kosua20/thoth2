#include "system/Keychain.hpp"
#include "system/TextUtilities.hpp"

#ifdef _MACOS
#include <Security/Security.h>
#endif

#ifdef _WIN32
#include <wincred.h>
#endif

bool Keychain::getPassword(const fs::path & server, const std::string & user, std::string & password){
		
#if defined(_MACOS)
	UInt32 passLength;
	char * passBuffer;
	const std::string serverName = server.string();
	OSStatus stat = SecKeychainFindInternetPassword(nullptr, serverName.size(), serverName.c_str(), 0, nullptr, user.size(), user.c_str(), 0, nullptr, 0, kSecProtocolTypeSSH, kSecAuthenticationTypeDefault, &passLength, (void**)&passBuffer, nullptr);
	if(stat == 0){
		password = std::string(passBuffer, passLength);
		return true;
	}
#elif defined(_WIN32)
	const std::string serverName = "Thoth_" + user + "@" + server.string();
	const std::wstring targetName = System::widen(serverName);
	PCREDENTIALW credential;
	BOOL stat = CredReadW(targetName.c_str(), CRED_TYPE_GENERIC, 0, &credential);
	if(stat){
		password = std::string((const char *)credential->CredentialBlob, (size_t)credential->CredentialBlobSize);
		CredFree(credential);
		return true;
	}
#else
	Log::Error() << Log::Password << "No keychain available on your platform." << std::endl;
#endif
	return false;
}


bool Keychain::setPassword(const fs::path & server, const std::string & user, const std::string & password){

#if defined(_MACOS)
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
#elif defined(_WIN32)
	const std::string serverName = "Thoth_" + user + "@" + server.string();
	const std::wstring targetName = System::widen(serverName);
	
	CREDENTIALW credsToAdd = {};
	credsToAdd.Flags = 0;
	credsToAdd.Type = CRED_TYPE_GENERIC;
	credsToAdd.TargetName = (LPWSTR)targetName.c_str();
	credsToAdd.CredentialBlob = (LPBYTE)password.c_str();
	credsToAdd.CredentialBlobSize = password.size();
	credsToAdd.Persist = CRED_PERSIST_LOCAL_MACHINE;
	// This will overwrite the credential if it already exists.
	BOOL stat = CredWrite(&credsToAdd, 0);
	return stat;
#else
	Log::Error() << Log::Password << "No keychain available on your platform." << std::endl;
#endif
	return false;
}
