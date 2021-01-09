#include "Common.hpp"
#include "Settings.hpp"
#include "Strings.hpp"
#include "Articles.hpp"
#include "Generator.hpp"

#include "system/Config.hpp"
#include "system/System.hpp"
#include "system/TextUtilities.hpp"
#include "system/SSHSFTP.hpp"
#include "system/Keychain.hpp"

#include <ctime>
#include <iomanip>
#include <chrono>
#include <iostream>

class ThothConfig : public Config {
public:
	
	explicit ThothConfig(const std::vector<std::string> & argv) : Config(argv) {
	
		// Process arguments.
		for(const auto & arg : arguments()) {
			// Config path.
			if((arg.key == "path" || arg.key == "p") && !arg.values.empty()){
				path = arg.values[0];
			}
			// Setup actions.
			if(arg.key == "setup"){
				action = SETUP;
			}
			if((arg.key == "set-password" || arg.key == "sp")){
				action = PASSWORD;
			}
			if(arg.key == "test"){
				action = TEST;
			}
			// Generation actions.
			if(arg.key == "generate"){
				action = GENERATE;
			}
			if(arg.key == "upload"){
				action = UPLOAD;
			}
			if(arg.key == "scribe"){
				action = GENERATE | UPLOAD;
			}
			// Modifiers.
			if(arg.key == "drafts-only" || arg.key == "d") {
				mode &= ~ARTICLES;
				mode &= ~RESOURCES;
				mode |= DRAFTS;
			}
			if(arg.key == "resources-only" || arg.key == "r") {
				mode &= ~ARTICLES;
				mode &= ~DRAFTS;
				mode &= ~INDEX;
				mode |= RESOURCES;
			}
			if(arg.key == "index-only" || arg.key == "i") {
				mode &= ~ARTICLES;
				mode &= ~DRAFTS;
				mode &= ~RESOURCES;
				mode |= INDEX;
			}
			if(arg.key == "force" || arg.key == "f") {
				mode |= FORCE;
			}
			// Infos.
			if(arg.key == "version" || arg.key == "v") {
				version = true;
				action = NONE;
			}
			if(arg.key == "license") {
				license = true;
				action = NONE;
			}
			if(arg.key == "ibis") {
				bonus = true;
				action = NONE;
			}
			
		}
		
		registerArgument("path", "p", "Path to the blog configuration file, non-relative.", "path to config");
		
		registerSection("Setup");
		
		registerArgument("setup", "", "Creates the configuration file and folders (articles, template, output, resources) specified by --path.");
		registerArgument("set-password", "sp", "Set or udpate the password in the system keychain for the configuration (specified by --path).");
		registerArgument("test", "", "Check the validity of the configuration (specified by --path) and attempts to connect to the SFTP server.");
		
		registerSection("Process");
		registerArgument("generate", "", "Generates the site (specified by --path). All existing files are kept. Drafts are updated. New articles are added. Index is rebuilt.");
		registerArgument("upload", "", "Upload to the SFTP server the content of the blog (specified by --path) that is not already present.");
		registerArgument("scribe", "", "Combines \"generate\" and \"upload\" with the corresponding config and options.");
		
		registerSection("Modifiers");
		registerArgument("index-only", "i", "Update index pages only.");
		registerArgument("drafts-only", "d", "Process drafts only.");
		registerArgument("resources-only", "r", "Update resources only.");
		registerArgument("force", "f", "Force generation/upload of all blog files");
		
		registerSection("Infos");
		registerArgument("version", "v", "Displays the current Thoth version.");
		registerArgument("license", "", "Display the license message.");
		
	}
	
	std::string path = "";
	
	// Tasks.
	uint action = GENERATE | UPLOAD;
	
	// Modifiers.
	uint mode = ALL;
	
	// Messages.
	bool version = false;
	bool license = false;
	bool bonus = false;
	
};

bool queryAndSetPassword(const Settings & settings){
	std::string pass;
	Log::Info() << Log::Password << "Please type your SFTP password for " << settings.ftpUsername() << "@" << settings.ftpDomain() << " below:" << std::endl;
	// Secure cin output log.
	System::setStdinPrintback(false);
	std::getline(std::cin, pass);
	System::setStdinPrintback(true);
	return Keychain::setPassword(settings.ftpDomain(), settings.ftpUsername(), pass);
}

void upload(const uint mode, const Settings & settings, Server & server){
	const bool force = bool(mode & FORCE);
	const fs::path src = settings.outputPath();
	const fs::path dst = settings.ftpPath();

	// We could copy the root and nothing else, but in case of forced upload it could erase other user data.
	
	if(mode & INDEX){
		Log::Info() << Log::Upload << "Uploading index pages...";
		// Index pages are always forced to update.
		const bool st0 = server.copyItem(src / "index.html", dst / "index.html", true);
		const bool st1 = server.copyItem(src / "index-drafts.html", dst / "index-drafts.html", true);
		const bool st2 = server.copyItem(src / "feed.xml", dst / "feed.xml", true);
		if(st0 && st1 && st2){
			Log::Info() << " done." << std::endl;
		} else {
			Log::Info() << " fail." << std::endl;
		}
	}
	
	if(mode & ARTICLES){
		Log::Info() << Log::Upload << "Uploading article pages...";
		const bool st0 = server.copyItem(src / "articles", dst / "articles", force);
		if(st0){
			Log::Info() << " done." << std::endl;
		} else {
			Log::Info() << " fail." << std::endl;
		}
	}
	
	if(mode & DRAFTS){
		Log::Info() << Log::Upload << "Uploading draft pages...";
		const bool st0 = server.copyItem(src / "drafts", dst / "drafts", force);
		if(st0){
			Log::Info() << " done." << std::endl;
		} else {
			Log::Info() << " fail." << std::endl;
		}
	}
	
	if(mode & RESOURCES){
		const std::vector<std::string> nonResources = { "index.html", "index-drafts.html", "feed.xml", "articles", "drafts"};
		
		Log::Info() << Log::Upload << "Uploading resources...";
		const auto files = System::listItems(src, false, true);
		bool st = true;
		for(const auto & file : files){
			const std::string filename = file.filename().string();
			if(std::find(nonResources.begin(), nonResources.end(), filename) == nonResources.end()){
				const bool st0 = server.copyItem(file, dst / file.filename(), force);
				st = st && st0;
			}
		}
		if(st){
			Log::Info() << " done." << std::endl;
		} else {
			Log::Info() << " fail." << std::endl;
		}
	}
}


int main(int argc, char** argv){
	
	ThothConfig config(std::vector<std::string>(argv, argv+argc));
	if(config.version){
		Log::Info() << versionMessage << std::endl;
		return 0;
	} else if(config.license){
		Log::Info() << licenseMessage << std::endl;
		return 0;
	} else if(config.bonus){
		Log::Info() << bonusMessage << std::endl;
		return 0;
	} else if(config.showHelp(config.path.empty())){
		return 0;
	}
	
	Settings settings(fs::path(config.path));
	
	// If setup, start by creating the configuration and hierarchy.
	if(config.action & SETUP){
		// Try to load to check if the file already exists.
		if(System::itemExists(settings.selfPath())){
			Log::Error() << Log::Config << "Unable to setup, config file already exists at path " << settings.selfPath() << "." << std::endl;
			return 2;
		}
		System::createDirectory(settings.rootPath());
		System::createDirectory(settings.articlesPath());
		System::createDirectory(settings.templatePath());
		System::createDirectory(settings.outputPath());
		System::createDirectory(settings.resourcesPath());
		settings.save();
		Log::Info() << "You can now fill in the informations in " << settings.selfPath() << " then use thoth --set-password " << settings.selfPath().string() << " to securely store the SFTP password in the system keychain. You should also put the template files (index.html, article.html, and optionally syntax.html) in " << settings.selfPath().string() << "/template/." << std::endl;
		return 0;
	}
	
	// Now we can try and load the parameters.
	if(!settings.load()){
		return 1;
	}
	
	if(config.action & PASSWORD){
		// Ask for the password.
		const bool res = queryAndSetPassword(settings);
		if(!res){
			Log::Error() << Log::Password << "Unable to save password to system keychain, fill the ftpPassword field in the config file as a fallback." << std::endl;
			return 4;
		}
		Log::Info() << Log::Password << "Saved! You can now test the connection using thoth --test " << settings.selfPath().string() << std::endl;
		return 0;
	}
	
	if(config.action & TEST){
		Log::Info() << Log::Config << "Configuration contains: " << std::endl << settings.str(false) << std::endl;
		Log::Info() << Log::Server << "Attempting to connect to " << settings.ftpUsername() << "@" << settings.ftpDomain() << ":" << settings.ftpPath().string() << std::endl;
		// Test connection to SFTP.
		Server server(settings.ftpDomain(), settings.ftpUsername(), settings.ftpPort());
		const bool res = server.authenticate(settings.ftpPassword());
		if(res){
			Log::Info() <<  Log::Server << "Authentication successful." << std::endl;
		}
		server.disconnect();
		return 0;
	}
	
	if(config.action & GENERATE){
		Log::Info() << Log::Load << "Loading articles... ";
		const auto articles = Article::loadArticles(settings.articlesPath(), settings);
		Log::Info() << articles.size() << " found." << std::endl;
		
		Generator generator(settings);
		generator.process(articles, config.mode);
	}
	
	if(config.action & UPLOAD){
		Log::Info() << Log::Upload << "Connecting to " << settings.ftpUsername() << "@" << settings.ftpDomain() << ":" << settings.ftpPath().string() << "." << std::endl;
		Server server(settings.ftpDomain(), settings.ftpUsername(), settings.ftpPort());
		const bool res = server.authenticate(settings.ftpPassword());
		if(!res){
			Log::Error() <<  Log::Server << "Error establishing a secure connection." << std::endl;
			server.disconnect();
			return 6;
		}
		upload(config.mode, settings, server);
		server.disconnect();
	}
	
	
	return 0;
}
