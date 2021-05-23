#pragma once

#include "Common.hpp"
#include "Articles.hpp"


struct hoedown_buffer;
struct hoedown_renderer;

enum Mode : uint {
	ARTICLES = 1,
	DRAFTS = 2,
	INDEX = 4,
	RESOURCES = 8,
	FORCE = 16,
	ALL = ARTICLES | DRAFTS | INDEX | RESOURCES
};

enum Action : uint {
	NONE = 0,
	SETUP = 1,
	GENERATE = 2,
	UPLOAD = 4,
	PASSWORD = 8,
	TEST = 16
};

class Generator {
public:
	
	Generator(const Settings & settings);
	
	void process(const std::vector<Article> & articles, uint mode);
	
	~Generator();
	
private:
	
	struct Template {
		std::string footer;
		std::string header;
		std::string indexItem;
		std::string article;
		std::string syntax;
	};
	
	struct Page {
		fs::path location;
		std::string html;
		std::string summary;
		std::string innerContent;
		std::vector<std::pair<fs::path, fs::path>> files;
	};
	
	void renderPage(const Article & article, Page & page);
	
	std::string renderContent(const Article & article);
	
	void generateIndexPages(std::vector<Page> & pages);
	
	bool savePage(const Page & page, const fs::path & outputDir, bool force) const;
	
	size_t savePages(const Article::Type & mode, const fs::path & output, bool force);
	
	static std::string summarize(const std::string & htmlText, const size_t length);
	
	Template _template;
	const Settings & _settings;
	std::vector<Article> _articles;
	std::vector<Page> _pages;
	
	hoedown_renderer * _renderer;
	hoedown_buffer * _buffer;
};
