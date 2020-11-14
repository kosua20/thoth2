#include "Settings.hpp"
#include "system/Keychain.hpp"
#include "system/TextUtilities.hpp"
#include <fstream>


Settings::Settings(const fs::path & path){
	_selfPath = path;
	_rootPath = path.parent_path();
	
	_templatePath = _rootPath / "template";
	_articlesPath = _rootPath / "articles";
	_outputPath = _rootPath / "output";
	_resourcesPath = _rootPath / "resources";
}

bool Settings::load(){
	Log::Info() << Log::Load << "Loading settings from " << _selfPath << "." << std::endl;
	
	std::ifstream file(_selfPath);
	if(!file.is_open()){
		Log::Error() << "Unable to load config file at path " << _selfPath <<"." << std::endl;
		return false;
	}
	std::string str;
	while(std::getline(file, str)){
		if(TextUtilities::hasPrefix(str, "#") || TextUtilities::hasPrefix(str, "_")){
			continue;
		}
		auto tokens = TextUtilities::split(str, ":", true);
		if(tokens.size() > 1){
			
			for(size_t i = 0; i < tokens.size(); ++i){
				tokens[i] = TextUtilities::trim(tokens[i], "\t\r\n ");
			}
			const auto & key = tokens[0];
			const auto & value = tokens[1];
			if(value.empty()){
				continue;
			}
			if(key == "templatePath"){
				_templatePath = value;
			} else if(key == "articlesPath"){
				_articlesPath = value;
			} else if(key == "outputPath"){
				_outputPath = value;
			} else if(key == "defaultAuthor"){
				_defaultAuthor = value;
			} else if(key == "dateStyle"){
				_dateStyle = value;
			} else if(key == "blogTitle"){
				_blogTitle = value;
			} else if(key == "imageWidth"){
				_imageWidth = value;
			} else if(key == "imagesLinks"){
				_imagesLinks = (value=="true") || (value=="True") || (value=="yes") || (value=="Yes") || (value=="1");
			} else if(key == "ftpAdress"){
				std::vector<std::string> values;
				values.insert(values.end(), tokens.begin()+1, tokens.end());
				std::string path = TextUtilities::join(values, ":");
				TextUtilities::replace(path, "sftp://", "");
				TextUtilities::replace(path, "ssh://", "");
				TextUtilities::replace(path, "ftp://", "");
				// Two cases
				const std::string::size_type colPos = path.find(":");
				const std::string::size_type slaPos = path.find("/");
				std::string domain, dir;
				if(colPos != std::string::npos){
					domain = path.substr(0, colPos);
					dir = path.substr(colPos+1);
				} else if(slaPos != std::string::npos){
					domain = path.substr(0, slaPos);
					dir = path.substr(slaPos);
				} else {
					domain = path;
					dir = "/";
				}
				_ftpDomain = domain;
				_ftpPath = dir;
			} else if(key == "ftpUsername"){
				_ftpUsername = value;
			} else if(key == "ftpPort"){
				_ftpPort = std::stoi(value);
			} else if(key == "ftpPassword"){
				_ftpPassword = value;
			} else if(key == "siteRoot"){
				_siteRoot = value;
			}
		}
	}
	file.close();
	return true;
}

void Settings::save(){
	std::ofstream file(_selfPath);
	if(!file.is_open()){
		Log::Error() << "Unable to write config file at path \"" << _selfPath << "\"." << std::endl;
	}
	file << str(true) << std::endl;
	file.close();
}

std::string Settings::str(bool includeHelp){
	std::stringstream str;
	if(includeHelp){
		str << "# {#Thoth} config file\n# The root path is deduced from the position of this config file\n";
	}
	if(includeHelp){
		str << "\n# The path to the template folder\n#\t(defaults to rootPath/template)\n";
	}
	str << "templatePath" << ":\t\t" << _templatePath.string() << "\n";
	
	if(includeHelp){
		str << "\n# The path to the articles folder containing the .md files\n#\t(defaults to rootPath/articles)\n";
	}
	str << "articlesPath" << ":\t\t" << _articlesPath.string() << "\n";
	
	if(includeHelp){
		str << "\n# The path where Thoth should output the generated content\n#\t(defaults to rootPath/output)\n";
	}
	str << "outputPath" << ":\t\t" << _outputPath.string() << "\n";
	
	if(includeHelp){
		str << "\n# The title of the blog\n#\t(defaults to \"A new blog\")\n";
	}
	str << "blogTitle" << ":\t\t" << _blogTitle << "\n";
	
	if(includeHelp){
		str << "\n# The default author name to use on each article page\n#\t(defaults to the current Mac user)\n";
	}
	str << "defaultAuthor" << ":\t\t" << _defaultAuthor << "\n";
	
	if(includeHelp){
		str << "\n# The date style used in each article (.md file)\n#\t(defaults to %m/%d/%Y, see std::get_time specification for available formats)\n";
	}
	str << "dateStyle" << ":\t\t" << _dateStyle << "\n";
	
	if(includeHelp){
		str << "\n# The default width for each image in articles html pages.\n#\t(defaults to 640)\n";
	}
	str << "imageWidth" << ":\t\t" << _imageWidth << "\n";
	
	if(includeHelp){
		str << "\n# Set to true if you want each image of an article to link directly to the corresponding file\n#\t(defaults to false)\n";
	}
	str << "imagesLinks" << ":\t\t" << (_imagesLinks ? "true" : "false") << "\n";
	
	if(includeHelp){
		str << "\n# The ftp address pointing to the exact folder where the output should be uploaded\n";
	}
	str << "ftpAdress" << ":\t\t";
	if(!_ftpDomain.empty() || !_ftpPath.empty()){
		str << _ftpDomain << ":" << _ftpPath.string();
	}
	str << "\n";
	
	if(includeHelp){
		str << "\n# The ftp username\n";
	}
	str << "ftpUsername" << ":\t\t" << _ftpUsername << "\n";
	
	if(includeHelp){
		str << "\n# The ftp password\n\tIf possible, leave the field empty and use the keychain wtih thoth --set-password " << _selfPath.string() << "\n";
	}
	
	str << "ftpPassword" << ":\t\t" << /*_ftpPassword <<*/ "\n";
	
	if(includeHelp){
		str << "\n# The ftp port to use\n#\t(defaults to 22)\n";
	}
	str << "ftpPort" << ":\t\t" << _ftpPort << "\n";
	
	if(includeHelp){
		str << "\n# The online URL of the blog, without http:// (for RSS generation)\n";
	}
	str << "siteRoot" << ":\t\t" << _siteRoot;
	
	return str.str();
}

std::string Settings::ftpPassword() const {
	//If no password was provided, try to load from the keychain.
	if(_ftpPassword.empty()){
		Log::Info() << Log::Password << "Fetching account password from keychain...";
		std::string pass;
		if(Keychain::getPassword(ftpDomain(), _ftpUsername, pass)){
			Log::Info() << " success." << std::endl;
			return pass;
		} else {
			Log::Info() << std::endl;
			Log::Error() << Log::Password << "Failure, define the password with thoth --set-password " << _selfPath.string() << ", or if keychain is not supported on your platform, by setting the ftpPassword field in the config file." << std::endl;
		}
	}
	return _ftpPassword;
}
