
#include "Common.hpp"
#include "Settings.hpp"
#include "system/System.hpp"


struct ssh_session_struct;
struct sftp_session_struct;
typedef struct ssh_session_struct* ssh_session;
typedef struct sftp_session_struct* sftp_session;

class Server {
	
public:

	struct Stats {
		unsigned int uploadedFiles = 0;
		unsigned int createdDirs = 0;
	};
	
	Server(const std::string & domain, const std::string & user, const int port);
	
	bool authenticate(const std::string & password);
	
	void disconnect();
	
	bool copyItem(const fs::path & src, const fs::path & dst, bool force);
	
	bool createDirectory(const fs::path & path, bool force = false);
	
	bool removeItem(const fs::path & path);
	
	bool itemExists(const fs::path & path, uint64_t& size);

	bool itemExists(const fs::path & path);

	const Stats& stats() const { return _stats; }

	void resetStats();
	
private:
	
	int verifyHost();

	Stats _stats;
	ssh_session _ssh = 0;
	sftp_session _sftp = 0;
	int _verbosity = 0;
	bool _connected = false;
};


