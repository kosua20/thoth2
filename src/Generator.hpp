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

	struct Page {
	public:
		fs::path location;
		std::string html;
		std::vector<std::pair<fs::path, fs::path>> files;
	};

	struct PageArticle : public Page {
	public:

		const Article* article = nullptr;

		std::string innerContent;
		std::string summary;
	};

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
	
	void renderArticlePage(const Article & article, PageArticle & page);
	
	std::string renderContent(const Article & article);
	
	void generateIndexPage(const std::vector<const PageArticle*>& pages, const std::string& title, const fs::path& relativePath, const fs::path& parentPath, Page& page);

	void generateRssFeed(const std::vector<const PageArticle*>& pages, Generator::Page& feed);

	void generateSitemap(const std::vector<PageArticle>& articlePages, const std::vector<Page>& otherPages, const Page& indexPage, Generator::Page& sitemap);
	
	bool savePage(const Page & page, const fs::path & outputDir, bool force) const;
	
	size_t saveArticlePages(const std::vector<const PageArticle*>& pages, const fs::path & output, bool force);

	static std::string populateSnippet(const Generator::PageArticle & page, const fs::path& path, const std::string& src);
	
	Template _template;
	const Settings & _settings;
	std::vector<Article> _articles;
	
	hoedown_renderer * _renderer;
	hoedown_buffer * _buffer;
};
