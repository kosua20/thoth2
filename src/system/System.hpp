#pragma once

#include "system/Config.hpp"
#include "Common.hpp"

#include <ghc/filesystem.hpp>
#include <thread>

namespace fs = ghc::filesystem;

/**
 \brief Performs system basic operations such as directory creation, timing, threading, file picking.
 \ingroup System
 */
class System {
public:

	/** Notify the user by sending a 'Bell' signal. */
	static void ping();

	static std::vector<fs::path> listItems(const fs::path & path, bool recursive, bool listDirectories);
	
	static bool copyItem(const fs::path & src, const fs::path & dst, bool force);
	
	static bool createDirectory(const fs::path & path, bool force = false);
	
	static bool removeItem(const fs::path & path);
	
	static bool itemExists(const fs::path & path);
	
	static bool isDirectory(const fs::path & path);
	
	static bool isFile(const fs::path & path);
	
	static std::string loadStringFromFile(const fs::path & path);
	
	static bool writeStringToFile(const std::string & str, const fs::path & path);
	
	static void setStdinPrintback(bool enable);
	
};
