#include "system/System.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#ifdef _WIN32

WCHAR * widen(const std::string & str) {
	const int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	WCHAR * arr	= new WCHAR[size];
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, static_cast<LPWSTR>(arr), size);
	// \warn Will leak on Windows.
	return arr;
}

std::string narrow(WCHAR * str) {
	const int size = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
	std::string res(size - 1, 0);
	WideCharToMultiByte(CP_UTF8, 0, str, -1, &res[0], size, nullptr, nullptr);
	return res;
}

#else

const char * widen(const std::string & str) {
	return str.c_str();
}
std::string narrow(char * str) {
	return std::string(str);
}

#endif


void System::ping() {
	Log::Info() << '\a' << std::endl;
}

std::vector<fs::path> System::listItems(const fs::path & path, bool recursive, bool listDirectories){
	std::error_code ec;
	if(!System::itemExists(path) || !fs::is_directory(path, ec)) {
		return {};
	}

	std::vector<fs::path> files;
	try {
		fs::directory_iterator endIt;
		for(fs::directory_iterator it(path); it != endIt; ++it) {
			if(it->is_directory()){
				if(listDirectories){
					files.push_back(it->path());
				}
				if(recursive){
					auto subfiles = listItems(it->path(), true, listDirectories);
					files.insert(files.end(), subfiles.begin(), subfiles.end());
				}
			} else if(it->is_regular_file()){
				const std::string fileName = it->path().filename().string();
				if(fileName.size() > 0 && fileName.at(0) != '.'){
					files.push_back(it->path());
				}
			}
		}
	} catch(...) {
		Log::Error() << "Can't access or find directory." << std::endl;
	}

	std::sort(files.begin(), files.end());

	return files;
}

bool System::copyItem(const fs::path & src, const fs::path & dst, bool force){
	fs::copy_options options = fs::copy_options::recursive;
	if(force){
		options |= fs::copy_options::overwrite_existing;
	} else {
		options |= fs::copy_options::skip_existing;
	}
	std::error_code ec;
	fs::copy(src, dst, options, ec);
	return ec ? false : true;
}

bool System::createDirectory(const fs::path & path, bool force){
	if(force && fs::exists(path)){
		removeItem(path);
	}
	if(!fs::exists(path)){
		std::error_code ec;
		const bool res = fs::create_directories(path, ec);
		return ec ? false : res;
	}
	return false;
}

bool System::removeItem(const fs::path & path){
	std::error_code ec;
	const size_t count = fs::remove_all(path, ec);
	return ec ? false : (count > 0);
}

bool System::itemExists(const fs::path & path){
	std::error_code ec;
	const bool res = fs::exists(path, ec);
	return ec ? false : res;
}

bool System::isDirectory(const fs::path & path){
	std::error_code ec;
	const bool res = fs::is_directory(path, ec);
	return ec ? false : res;
}

bool System::isFile(const fs::path & path){
	std::error_code ec;
	const bool res = fs::is_regular_file(path, ec);
	return ec ? false : res;
}

std::string System::loadStringFromFile(const fs::path & path){
	std::ifstream file(widen(path.string()));
	if(file.bad() || file.fail()) {
		Log::Error() << "Unable to load file at path " << path << "." << std::endl;
		return "";
	}
	std::stringstream buffer;
	// Read the stream in a buffer.
	buffer << file.rdbuf();
	file.close();
	// Create a string based on the content of the buffer.
	std::string line = buffer.str();
	return line;
}

bool System::writeStringToFile(const std::string & str, const fs::path & path){
	std::ofstream file(widen(path.string()));
	if(file.bad() || file.fail()) {
		Log::Error() << "Unable to write to file at path " << path << "." << std::endl;
		return false;
	}
	file << str << std::endl;
	file.close();
	return true;
}

void System::setStdinPrintback(bool enable){
#ifdef _WIN32
	// \warn Untested.
	HANDLE hstdin = GetStdHandel(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hstdin, &mode);
	if(enable){
		mode |= ENABLE_ECHO_INPUT;
	} else {
		mode &= ~ENABLE_ECHO_INPUT;
	}
	SetConsoleMode(hstdin, mode);
#else
	struct termios tty;
	tcgetattr(STDIN_FILENO, &tty);
	if(enable){
		tty.c_lflag |= ECHO;
	} else {
		tty.c_lflag &= ~ECHO;
	}
	tcsetattr(STDIN_FILENO, TCSANOW, &tty);
#endif
}
