
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <sys/stat.h>
#include "system/SSHSFTP.hpp"
#include "system/TextUtilities.hpp"

#ifdef _WIN32
#include <fcntl.h>
#define strncasecmp _strnicmp
#endif

/* Platform independent file modes. */
/* File mode */
#define S_IRWXU_TH   0000700   /* [XSI] RWX mask for owner */
#define S_IRUSR_TH   0000400   /* [XSI] R for owner */
#define S_IWUSR_TH   0000200   /* [XSI] W for owner */
#define S_IXUSR_TH   0000100   /* [XSI] X for owner */
#define S_IRWXG_TH   0000070   /* [XSI] RWX mask for group */
#define S_IRGRP_TH   0000040   /* [XSI] R for group */
#define S_IWGRP_TH   0000020   /* [XSI] W for group */
#define S_IXGRP_TH   0000010   /* [XSI] X for group */
#define S_IRWXO_TH   0000007   /* [XSI] RWX mask for other */
#define S_IROTH_TH   0000004   /* [XSI] R for other */
#define S_IWOTH_TH   0000002   /* [XSI] W for other */
#define S_IXOTH_TH   0000001   /* [XSI] X for other */

#define CREATE_AUTH

Server::Server(const std::string & domain, const std::string & user, const int port){

	ssh_init();
	_ssh = ssh_new();
	if(!_ssh) {
		Log::Error() << Log::Server << "Unable to create SSH connection." << std::endl;
		return;
	}
	ssh_options_set(_ssh, SSH_OPTIONS_USER, user.c_str());
	ssh_options_set(_ssh, SSH_OPTIONS_HOST, domain.c_str());
	ssh_options_set(_ssh, SSH_OPTIONS_PORT, &port);
	ssh_options_set(_ssh, SSH_OPTIONS_LOG_VERBOSITY, &_verbosity);
	if(ssh_connect(_ssh)){
		Log::Error() << Log::Server << "Unable to connect via SSH: " << ssh_get_error(_ssh) << std::endl;
		disconnect();
		return;
	}
	
	if(verifyHost() < 0){
		Log::Error() << Log::Server << "Unable to verify host." << std::endl;
		disconnect();
		return;
	}
	
}
	
bool Server::authenticate(const std::string & password){
	if(!_ssh){
		Log::Error() << Log::Server << "No SSH session running." << std::endl;
		return false;
	}
	if(ssh_userauth_password(_ssh, NULL, password.c_str()) != SSH_AUTH_SUCCESS) {
		Log::Error() << Log::Server << "SSH authentication failed." << std::endl;
		disconnect();
		return false;
	}
	char *banner = ssh_get_issue_banner(_ssh);
	if(banner) {
		if(_verbosity > 0){
			Log::Info() << Log::Server << banner << std::endl;
		}
		SSH_STRING_FREE_CHAR(banner);
	}
	
	// SFTP.
	_sftp = sftp_new(_ssh);
	if(!_sftp) {
		Log::Error() << Log::Server << "Unable to create SFTP session: " << ssh_get_error(_ssh) <<  std::endl;
		disconnect();
		return false;
    }
	if(sftp_init(_sftp) < 0) {
        Log::Error() << Log::Server << "Unable to init SFTP session: " << ssh_get_error(_ssh) <<  std::endl;
		disconnect();
		return false;
    }
	_connected = true;
	return true;
}

void Server::disconnect(){
	if(_sftp){
		sftp_free(_sftp);
		_sftp = nullptr;
	}
	if(_ssh){
		ssh_disconnect(_ssh);
		ssh_free(_ssh);
		_ssh = nullptr;
	}
	_connected = false;
}

bool Server::copyItem(const fs::path & src, const fs::path & dst, bool force){
	if(!_connected){
		Log::Error() << Log::Server << "No SFTP session running." << std::endl;
		return false;
	}
	const std::string nameStr = src.filename();
	if(TextUtilities::hasPrefix(nameStr, ".")){
		return true;
	}
	if(!System::itemExists(src)){
		return false;
	}
	if(itemExists(dst) && force){
		removeItem(dst);
	}
	bool res = true;

	if(System::isDirectory(src)){
		res = createDirectory(dst);
		const auto files = System::listItems(src, false, true);
		for(const auto & file : files){
			const bool res2 = copyItem(file, dst / file.filename(), force);
			res = res && res2;
		}
	} else if(System::isFile(src)){
		// If the file still exist after potential force deletion, skip.
		if(itemExists(dst)){
			return true;
		}

		mode_t mode = S_IRUSR_TH | S_IWUSR_TH | S_IRGRP_TH | S_IROTH_TH;
		sftp_file dstFile = sftp_open(_sftp, dst.c_str(), O_WRONLY | O_CREAT, mode);
		if(!dstFile){
			res = false;
		} else {
			std::ifstream srcFile(System::widen(src.string()), std::ios::in | std::ios::binary);
			std::vector<char> buffer(4096);
			while(srcFile.read(buffer.data(), buffer.size())){
				const ssize_t len = ssize_t(buffer.size());
				if(sftp_write(dstFile, buffer.data(), len) != len){
					res = false;
					break;
				}
			}
			if(!srcFile){
				const ssize_t len = ssize_t(srcFile.gcount());
				if(sftp_write(dstFile, buffer.data(), len) != len){
					res = false;
				}
			}
			
			srcFile.close();
			sftp_close(dstFile);
		}
	}
	return res;
}

bool Server::createDirectory(const fs::path & path, bool force){
	if(!_connected){
		Log::Error() << Log::Server << "No SFTP session running." << std::endl;
		return false;
	}
	if(force && itemExists(path)){
		// Delete the directory first.
		Server::removeItem(path);
	}
	// Re-check, if the directory still exists after a potential forced deletion, just skip.
	if(itemExists(path)){
		return true;
	}

	mode_t mode = S_IRWXU_TH | S_IRGRP_TH | S_IXGRP_TH | S_IROTH_TH | S_IXOTH_TH;
	const int res =  sftp_mkdir(_sftp, path.c_str(), mode);
	return res == 0;
}

bool Server::removeItem(const fs::path & path){
	if(!_connected){
		Log::Error() << Log::Server << "No SFTP session running." << std::endl;
		return false;
	}
	const std::string nameStr = path.filename();
	if(nameStr == "." || nameStr == ".."){
		return true;
	}
	sftp_attributes item = sftp_stat(_sftp, path.c_str());
	if(!item){
		return false;
	}
	
	if(item->type == SSH_FILEXFER_TYPE_DIRECTORY){
		// Iterate over directory elements and recursively remove them.
		const sftp_dir dir = sftp_opendir(_sftp, path.c_str());
		if(!dir){
			sftp_attributes_free(item);
			return false;
		}
		bool res = true;
		sftp_attributes file;
		while((file = sftp_readdir(_sftp, dir))){
			const std::string name(file->name);
			res = res && removeItem(path / name);
			sftp_attributes_free(file);
		}
		if(sftp_dir_eof(dir) == 0 || sftp_closedir(dir) == SSH_ERROR){
			sftp_attributes_free(item);
			return false;
		}
		const bool resDir = (sftp_rmdir(_sftp, path.c_str()) == 0);
		sftp_attributes_free(item);
		return res && resDir;
	} else if(item->type == SSH_FILEXFER_TYPE_REGULAR || item->type == SSH_FILEXFER_TYPE_SYMLINK){
		sftp_attributes_free(item);
		return sftp_unlink(_sftp, path.c_str()) == 0;
	}
	sftp_attributes_free(item);
	return false;
}

bool Server::itemExists(const fs::path & path){
	if(!_connected){
		Log::Error() << Log::Server << "No SFTP session running." << std::endl;
		return false;
	}
	sftp_attributes item = sftp_stat(_sftp, path.c_str());
	if(item){
		sftp_attributes_free(item);
		return true;
	}
	return false;
}

int Server::verifyHost(){
	if(!_ssh){
		return 0;
	}
    ssh_key srv_pubkey;
    if(ssh_get_server_publickey(_ssh, &srv_pubkey) < 0) {
        return -1;
    }
	unsigned char *hash = NULL;
    size_t hlen;
    const int rc = ssh_get_publickey_hash(srv_pubkey, SSH_PUBLICKEY_HASH_SHA256, &hash, &hlen);
    ssh_key_free(srv_pubkey);
    if (rc < 0) {
        return -1;
    }

    const enum ssh_known_hosts_e state = ssh_session_is_known_server(_ssh);
    switch(state) {
		case SSH_KNOWN_HOSTS_CHANGED:
			Log::Warning() << Log::Server << "Host key for server changed : server's one is now :" << std::endl;
			ssh_print_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hlen);
			ssh_clean_pubkey_hash(&hash);
			Log::Warning() << Log::Server << "For security reason, connection will be stopped." << std::endl;
			return -1;
		case SSH_KNOWN_HOSTS_OTHER:
			Log::Warning() << Log::Server << "The host key for this server was not found but an other type of key exists. An attacker might change the default server key to confuse your client into thinking the key does not exist\nWe advise you to rerun the client with -d or -r for more safety." << std::endl;
			return -1;
		case SSH_KNOWN_HOSTS_NOT_FOUND:
			Log::Warning() << Log::Server << "Could not find known host file. If you accept the host key here, the file will be automatically created." << std::endl;
			/* fallback to SSH_SERVER_NOT_KNOWN behavior */
		case SSH_SERVER_NOT_KNOWN:
			Log::Warning() << Log::Server << "The server is unknown. Do you trust the host key (yes/no)?" << std::endl;
			ssh_print_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hlen);
			
			char buf[10];
			if(fgets(buf, sizeof(buf), stdin) == NULL) {
				ssh_clean_pubkey_hash(&hash);
				return -1;
			}
			if(strncasecmp(buf,"yes",3)!=0){
				ssh_clean_pubkey_hash(&hash);
				return -1;
			}
			Log::Warning() << Log::Server << "This new key will be written on disk for further usage. do you agree ?" << std::endl;
			if(fgets(buf, sizeof(buf), stdin) == NULL) {
				ssh_clean_pubkey_hash(&hash);
				return -1;
			}
			if(strncasecmp(buf,"yes",3)==0){
				if(ssh_session_update_known_hosts(_ssh) != SSH_OK) {
					ssh_clean_pubkey_hash(&hash);
					Log::Error() << Log::Server << "SSH error " << strerror(errno) << std::endl;
					return -1;
				}
			}

			break;
		case SSH_KNOWN_HOSTS_ERROR:
			ssh_clean_pubkey_hash(&hash);
			Log::Error() << Log::Server << "SSH error " << ssh_get_error(_ssh) << std::endl;
			return -1;
		case SSH_KNOWN_HOSTS_OK:
			break; /* ok */
    }

    ssh_clean_pubkey_hash(&hash);
    return 0;
}
