#pragma once

#include "Common.hpp"
#include "system/System.hpp"

class Settings {
public:
	
	Settings(const fs::path & configFilePath);
	
	bool load();
	
	void save();
	
	std::string str(bool includeHelp);
	
	std::string ftpPassword() const;
	
	const fs::path & rootPath() const {
		return _rootPath;
	}

	const fs::path & resourcesPath() const {
		return _resourcesPath;
	}

	const fs::path & selfPath() const {
		return _selfPath;
	}

	const fs::path & templatePath() const {
		return _templatePath;
	}

	const fs::path & articlesPath() const {
		return _articlesPath;
	}

	const fs::path & outputPath() const {
		return _outputPath;
	}

	const std::string & defaultAuthor() const {
		return _defaultAuthor;
	}

	const std::string & dateStyle() const {
		return _dateStyle;
	}

	const std::string & blogTitle() const {
		return _blogTitle;
	}

	const std::string & tocTitle() const {
		return _tocTitle;
	}

	const std::string & imageWidth() const {
		return _imageWidth;
	}
	
	const std::string & ftpDomain() const {
		return _ftpDomain;
	}
	
	const fs::path & ftpPath() const {
		return _ftpPath;
	};

	const std::string & ftpUsername() const {
		return _ftpUsername;
	}

	const std::string & siteRoot() const {
		return _siteRoot;
	}

	const std::string & externalLink() const {
		return _externalLink;
	}

	int ftpPort() const {
		return _ftpPort;
	}

	unsigned int rssCount() const {
		return _rssCount;
	}

	unsigned int summaryLength() const {
		return _summaryLength;
	}

	bool imagesLinks() const {
		return _imagesLinks;
	}

	bool calendarIndexPages() const {
		return _calendarIndexPages;
	}

	bool perCategoryLink() const {
		return _perCategoryLink;
	}

private:
	
    /// The path to the main directory (computed).
	fs::path _rootPath;
    /// The path to the resources directory (computed).
	fs::path _resourcesPath;
	
	/// The path to the config file from which this Config struct was loaded.
	fs::path _selfPath;
    /// The path to the template directory.
	fs::path _templatePath;
    /// The path to the articles directory.
	fs::path _articlesPath;
    /// The path to the output directory.
	fs::path _outputPath;
    /// The default author name to use when generating articles.
	std::string _defaultAuthor = "";
    /// The format of the date used in the articles header.
	std::string _dateStyle = "%m/%d/%Y";
    /// The title of the blog.
	std::string _blogTitle = "A new blog";
	/// Title of the table of contents
	std::string _tocTitle = "Table of contents";
    /// The default image width to use.
	std::string _imageWidth = "640";
	/// The SFTP server domain
	std::string _ftpDomain;
	/// Path to the blog directory on the SFTP server
	fs::path _ftpPath;
    /// The username to use to connect to the SFTP server.
	std::string _ftpUsername = "";
    /// The password to use to connect to the SFTP server (if stored in the configuration).
	std::string _ftpPassword = "";
    /// The URL of the site.
	std::string _siteRoot = "";
	/// Link to use as parent link on the main page
	std::string _externalLink = "";
    /// The port to use to connect to the SFTP server.
	int _ftpPort = 22;
	/// Number of posts to display in the RSS feed.
	unsigned int _rssCount = 10;
	/// Number of characters of the article summaries on the index page
	unsigned int _summaryLength = 400;
    /// Denotes if images in the generated HTML files should link to the raw image file.
	bool _imagesLinks = false;
	/// Should index pages be generated for each year.
	bool _calendarIndexPages = false;
	/// Each category keyword links to the category page (instead of the overall categories list)
	bool _perCategoryLink = true;
};
