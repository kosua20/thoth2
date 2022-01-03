#include "system/Keychain.hpp"
#include "system/TextUtilities.hpp"

#ifdef _MACOS
#include <Security/Security.h>
#endif

#ifdef _WIN32
#include <wincred.h>
#endif

#ifdef __linux__
#include <libsecret/secret.h>

// Declare secret scheme.
const SecretSchema * getSecretSchema(){
	static SecretSchema schema;
	schema.name = "fr.simonrodriguez.thoth2";
	schema.flags = SECRET_SCHEMA_NONE;
	schema.attributes[0].name = "domain";
	schema.attributes[0].type = SECRET_SCHEMA_ATTRIBUTE_STRING;
	schema.attributes[1].name = "user";
	schema.attributes[1].type = SECRET_SCHEMA_ATTRIBUTE_STRING;
	schema.attributes[2].name = "NULL";
	schema.attributes[2].type = SecretSchemaAttributeType(0);
	return &schema;
}
#endif

bool Keychain::getPassword(const std::string & server, const std::string & user, std::string & password){
		
#if defined(_MACOS)
	UInt32 passLength;
	char * passBuffer;
	const std::string serverName = server;
	OSStatus stat = SecKeychainFindInternetPassword(nullptr, serverName.size(), serverName.c_str(), 0, nullptr, user.size(), user.c_str(), 0, nullptr, 0, kSecProtocolTypeSSH, kSecAuthenticationTypeDefault, &passLength, (void**)&passBuffer, nullptr);
	if(stat == 0){
		password = std::string(passBuffer, passLength);
		return true;
	}
#elif defined(_WIN32)
	const std::string serverName = "Thoth_" + user + "@" + server;
	const std::wstring targetName = System::widen(serverName);
	PCREDENTIALW credential;
	BOOL stat = CredReadW(targetName.c_str(), CRED_TYPE_GENERIC, 0, &credential);
	if(stat){
		password = std::string((const char *)credential->CredentialBlob, (size_t)credential->CredentialBlobSize);
		CredFree(credential);
		return true;
	}
#elif defined(__linux__)
	GError* stat = nullptr;
	gchar *passBuffer = secret_password_lookup_sync(getSecretSchema(), NULL, &stat, 
		"domain", server.c_str(), "user", user.c_str(), NULL);
	if(stat != nullptr){
		g_error_free(stat);
		return false;
	}
	if(passBuffer == nullptr){
		return false;
	}
	password = std::string(passBuffer);
	secret_password_free(passBuffer);
	return true;

#else
	Log::Error() << Log::Password << "No keychain available on your platform." << std::endl;
#endif
	return false;
}


bool Keychain::setPassword(const std::string & server, const std::string & user, const std::string & password){

#if defined(_MACOS)
	SecKeychainItemRef item;
	const std::string serverName = server;
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
	const std::string serverName = "Thoth_" + user + "@" + server;
	const std::wstring targetName = System::widen(serverName);
	
	CREDENTIALW credsToAdd = {};
	credsToAdd.Flags = 0;
	credsToAdd.Type = CRED_TYPE_GENERIC;
	credsToAdd.TargetName = (LPWSTR)targetName.c_str();
	credsToAdd.CredentialBlob = (LPBYTE)password.c_str();
	credsToAdd.CredentialBlobSize = (DWORD)password.size();
	credsToAdd.Persist = CRED_PERSIST_LOCAL_MACHINE;
	// This will overwrite the credential if it already exists.
	BOOL stat = CredWrite(&credsToAdd, 0);
	return stat;
#elif defined(__linux__)
	GError* stat = nullptr;
	secret_password_store_sync(getSecretSchema(), SECRET_COLLECTION_DEFAULT, "Thoth configuration", password.c_str(), NULL, &stat, 
		"domain", server.c_str(), "user", user.c_str(), NULL);
	if(stat != nullptr){
		g_error_free(stat);
		return false;
	}
	return true;
#else
	Log::Error() << Log::Password << "No keychain available on your platform." << std::endl;
#endif
	return false;
}
