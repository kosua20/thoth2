#include "Generator.hpp"
#include "system/TextUtilities.hpp"
#include "system/System.hpp"

#include <hoedown/html.h>
#include <hoedown/document.h>
#include <map>
#include <array>

#ifdef __linux__
const std::string EN_US_LOCALE = "en_US.UTF8";
#else
const std::string EN_US_LOCALE = "en_US";
#endif

Generator::Generator(const Settings & settings) : _settings(settings) {
	// Create markdown generator based on settings.
	
	// Initialize output directory.
	System::createDirectory(settings.outputPath());
	
	// Start by checking and updating the template.
	const auto templateFiles = System::listItems(settings.templatePath(), false, false);
	for(const auto & file : templateFiles){
		const fs::path dstPath = settings.outputPath() / file.filename();
		// Copy item.
		System::copyItem(file, dstPath, true);
	}
	System::removeItem(settings.outputPath() / "article.html");
	System::removeItem(settings.outputPath() / "syntax.html");
	System::removeItem(settings.outputPath() / "categories.html");
	
	// Load template data.
	if(!System::itemExists(settings.templatePath() / "article.html") || !System::itemExists(settings.templatePath() / "index.html")){
		Log::Error() << "Unable to find the template files article.html and index.html at path " << settings.templatePath() << "." << std::endl;
		return;
	}
	_template.article = System::loadStringFromFile(settings.templatePath() / "article.html");
	const std::string indexHtml   = System::loadStringFromFile(settings.templatePath() / "index.html");
	const std::string::size_type insertPos = indexHtml.find("{#ARTICLE_BEGIN}");
	const std::string::size_type endPos = indexHtml.find("{#ARTICLE_END}");
	_template.indexItem = indexHtml.substr(insertPos + 16, endPos - (insertPos + 16));
	_template.header = indexHtml.substr(0, insertPos);
	_template.footer = indexHtml.substr(endPos + 14);
	if(System::itemExists(settings.templatePath() / "syntax.html")){
		_template.syntax  = System::loadStringFromFile(settings.templatePath() / "syntax.html");
	}

	//Category listing template.
	const std::string categHtml   = System::loadStringFromFile(settings.templatePath() / "categories.html");
	const std::string::size_type insertPosCateg = categHtml.find("{#CATEGORY_BEGIN}");
	const std::string::size_type endPosCateg = categHtml.find("{#CATEGORY_END}");
	const std::string::size_type insertPosNestedCateg = categHtml.find("{#ARTICLE_BEGIN}");
	const std::string::size_type endPosNestedCateg = categHtml.find("{#ARTICLE_END}");
	_template.headerCategory = categHtml.substr(0, insertPosCateg);
	_template.footerCategory = categHtml.substr(endPosCateg + 15);
	_template.itemHeaderCategory = categHtml.substr(insertPosCateg + 17, insertPosNestedCateg - (insertPosCateg + 17));
	_template.itemFooterCategory = categHtml.substr(endPosNestedCateg + 14, endPosCateg - (endPosNestedCateg + 14));
	_template.itemArticleCategory = categHtml.substr(insertPosNestedCateg + 16, endPosNestedCateg - (insertPosNestedCateg + 16));

	_buffer = hoedown_buffer_new(100);
}


void Generator::process(const std::vector<Article> & articles, uint mode){
	_articles = articles;

	std::vector<PageArticle> articlePages(_articles.size());
	std::vector<Page> rootPages;
	std::vector<Page> otherPages;

	// Generate all categories.
	Categories categories;
	for(const auto& article : articles){
		for(const Article::Keyword& keyword : article.keywords()){
			if(categories.count(keyword.id) == 0){
				categories[keyword.id].name = keyword.name;
				categories[keyword.id].location = fs::path("categories/" + keyword.id + ".html");
			}
		}
	}

	// Convert the markdown representation to html for each article.
	Log::Info() << Log::Generation << "Processing pages... ";
	for(size_t aid = 0; aid < _articles.size(); ++aid){
		renderArticlePage(_articles[aid], articlePages[aid], categories);
	}
	Log::Info() << "done." << std::endl;

	// Sort generated article pages.
	std::vector<const PageArticle*> publishedPages;
	std::vector<const PageArticle*> draftPages;

	for(const PageArticle& page : articlePages){
		if(page.article->type() == Article::Type::Public){
			publishedPages.push_back(&page);
		} else {
			draftPages.push_back(&page);
		}
	}

	const bool force = bool(mode & FORCE);
	
	if(mode & ARTICLES){
		Log::Info() << Log::Generation << "Creating article pages... ";
		System::createDirectory(_settings.outputPath() / "articles", force);
		const size_t count = saveArticlePages(publishedPages, _settings.outputPath(), force);
		Log::Info() << count << " new created pages." << std::endl;
	}
	if(mode & DRAFTS){
		Log::Info() << Log::Generation << "Creating drafts pages... ";
		System::createDirectory(_settings.outputPath() / "drafts", force);
		const size_t count = saveArticlePages(draftPages, _settings.outputPath(), force);
		Log::Info() << count << " new created pages." << std::endl;
	}

	// Generate calendar pages if requested.
	// We always generate them as they have to appear in the sitemap.
	// We could just generate the locations instead if we are not saving the articles.
	if(_settings.calendarIndexPages()){

		// Sort published pages by year and month.
		std::map<size_t, std::map<size_t, std::vector<const PageArticle*>>> calendar;
		for(const PageArticle* page: publishedPages){
			const Article& article = *(page->article);
			const Date& date = article.date().value();
			calendar[date.year()][date.month()].push_back(page);
		}

		otherPages.reserve(calendar.size());

		for(const auto& year : calendar){
			std::vector<const PageArticle*> yearArticles;
			for(const auto& month : year.second){
				yearArticles.insert(yearArticles.end(), month.second.begin(), month.second.end());
				// TODO: we could generate per-month index pages.
				/*
				const PageArticle& refPage = *(month.second[0]);
				const std::string monthStr = refPage.article->date().value().str("%Y - %B");
				const std::string title = " Month: " + monthStr;
				 otherPages.emplace_back();
				generateIndexPage(month.second, title, "../../../", "../../../index.html", otherPages.back());
				 otherPages.back().location = refPage.location.parent_path() / "index.html";
				*/

			}

			const PageArticle& refPage = *(yearArticles[0]);
			const std::string yearStr = refPage.article->date().value().str("%Y");
			const std::string title = "Year: " + yearStr;
			otherPages.emplace_back();
			generateIndexPage(yearArticles, title, "../..", "../../index.html", otherPages.back());
			otherPages.back().location = refPage.location.parent_path().parent_path() / "index.html";
		}

	}

	// Categories.

	// Find all categories that are public, and accumulate pages.
	std::map<std::string, std::vector<const PageArticle*>> categoryArticles;

	for(const PageArticle* page: publishedPages){
		const Article& article = *(page->article);
		for(const Article::Keyword& keyword : article.keywords()){
			categoryArticles[keyword.id].push_back(page);
		}
	}
	for(const auto& categoryKV : categoryArticles){
		const Category& category = categories.at(categoryKV.first);

		const std::string title = "Category: " + category.name;
		otherPages.emplace_back();
		generateIndexPage(categoryKV.second, title, "..", "index.html", otherPages.back());
		otherPages.back().location = category.location;
	}

	// Save all category and calendar pages.
	if(mode & ARTICLES){
		Log::Info() << Log::Generation << "Creating category/calendar pages... " << std::flush;
		size_t count = 0;
		for(const auto & page : otherPages){
			const bool res = savePage(page, _settings.outputPath(), force);
			count += size_t(res);
		}
		Log::Info() << count << " new created pages." << std::endl;
	}

	// Index pages.
	if(mode & (INDEX | ARTICLES | DRAFTS)){
		Log::Info() << Log::Generation << "Generating index pages:" << std::endl;

		// Four main pages: 2 index pages, a RSS feed, a sitemap.
		rootPages.resize(5);

		rootPages[0].location = fs::path("index.html");
		generateIndexPage(publishedPages, _settings.blogTitle(), ".", _settings.externalLink(), rootPages[0]);

		rootPages[1].location = fs::path("index-drafts.html");
		generateIndexPage(draftPages, _settings.blogTitle() + " - Drafts", ".", _settings.externalLink(), rootPages[1]);

		rootPages[2].location = fs::path("categories/index.html");
		generateCategoriesPage(categoryArticles, categories, _settings.blogTitle() + " - Categories", "..", "../index.html", rootPages[2]);

		rootPages[3].location = fs::path("feed.xml");
		generateRssFeed(publishedPages, rootPages[3]);

		rootPages[4].location = fs::path("sitemap.xml");
		generateSitemap(publishedPages, otherPages, {&rootPages[0], &rootPages[2]}, rootPages[4]);

		// Save all general pages.
		for(const auto & index : rootPages){
			savePage(index, _settings.outputPath(), true);
			Log::Info() << Log::Generation << " * " << (_settings.outputPath() / index.location) << "." << std::endl;
		}
	}
	
	if(mode & RESOURCES){
		Log::Info() << Log::Generation << "Copying resources... ";
		// force if needed
		const auto resources = System::listItems(_settings.resourcesPath(), false, true);
		for(const auto & file : resources){
			const fs::path dstPath = _settings.outputPath() / file.filename();
			// Copy item.
			System::copyItem(file, dstPath, force);
		}
		Log::Info() << "done." << std::endl;
	}
	
}

void Generator::renderArticlePage(const Article & article, Generator::PageArticle & page, const Categories& categories){
	page.article = &article;

	const bool isPublic = article.type() == Article::Public;
	const std::string baseDir = isPublic ? "articles" : "drafts";
	const fs::path baseDirPath(baseDir);
	const fs::path sharedUrl = baseDirPath / article.url();
	page.location = sharedUrl;
	page.location.replace_extension("html");

	const std::string content = renderContent(article);
	page.summary = TextUtilities::summarize(content, _settings.summaryLength() );
	page.innerContent = content;
	page.tableOfContent = renderTableOfContent(article);

	// Look for local links.
	page.files.clear();
	std::string::size_type srcPos = content.find("src=\"");
	std::string::size_type endPos = std::string::npos;
	
	while(srcPos != std::string::npos){
		endPos = content.find("\"", srcPos + 5);
		const std::string srcLink = content.substr(srcPos + 5, endPos - (srcPos + 5));
		// There is a weird issue with referenced images where a space is present between the closing bracket and the file path, that turns into a "%09".
		std::string link = srcLink;
		if(TextUtilities::hasPrefix(srcLink, "%09")){
			link = srcLink.substr(3);
		}
		if(!TextUtilities::hasPrefix(link, "http") && !TextUtilities::hasPrefix(link, "www.")){
			const fs::path srcPath = _settings.articlesPath() / link;
			const fs::path relPath = sharedUrl.stem() / srcPath.filename();
			const fs::path dstPath = sharedUrl / srcPath.filename();
			page.files.push_back({srcPath, dstPath});
			TextUtilities::replace(page.innerContent, srcLink, relPath.generic_string());
		}
		srcPos = content.find("src=\"", endPos);
	}
	const std::string relativeToRoot = "../../../";

	//Prepare keywords string.
	std::string keywordsStr;
	const auto& keywords = article.keywords();
	const size_t keyCount = keywords.size();
	for(size_t kid = 0; kid < keyCount; ++kid){
		const Article::Keyword& keyword = keywords[kid];
		// Link wrt root.
		keywordsStr.append(std::string("<a href=\"") + relativeToRoot);
		if(_settings.perCategoryLink()){
			keywordsStr.append(categories.at(keyword.id).location.generic_string());
		} else {
			// Hardcoded to the categories global list.
			keywordsStr.append("categories/index.html#" + keyword.id);
		}
		keywordsStr.append("\" class=\"keyword\">");
		// Apply lowercase for uniformity.
		keywordsStr.append(TextUtilities::lowercase(keyword.name));
		keywordsStr.append("</a>");
		if(kid != (keyCount-1)){
			keywordsStr.append(" ");
		}
	}

	//Generate complete html.
	std::string html(_template.article);
	TextUtilities::replace(html, "{#TITLE}", article.title());
	TextUtilities::replace(html, "{#DATE}", article.dateStr());
	TextUtilities::replace(html, "{#AUTHOR}", article.author());
	TextUtilities::replace(html, "{#KEYWORDS}", keywordsStr);
	TextUtilities::replace(html, "{#BLOG_TITLE}", _settings.blogTitle());
	TextUtilities::replace(html, "{#LINK}", page.location.generic_string());
	TextUtilities::replace(html, "{#SUMMARY}", page.summary);
	TextUtilities::replace(html, "{#AUTHOR}", article.author());
	TextUtilities::replace(html, "{#ROOT_LINK}", _settings.siteRoot());

	if(!_template.syntax.empty() && content.find("<pre>") != std::string::npos){
		TextUtilities::replace(html, "</head>", "\n" + _template.syntax + "\n</head>");
	}

	TextUtilities::replace(html, "{#CONTENT}", page.innerContent);
	page.html = html;
	
}

std::string Generator::populateSnippet(const Generator::PageArticle & page, const fs::path& path, const std::string& src){
	const Article& article = *page.article;

	std::string html(src);
	TextUtilities::replace(html, "{#TITLE}", article.title());
	TextUtilities::replace(html, "{#DATE}", article.dateStr());
	TextUtilities::replace(html, "{#AUTHOR}", article.author());
	const fs::path finalPath = (path / page.location);
	TextUtilities::replace(html, "{#LINK}", finalPath.generic_string());
	TextUtilities::replace(html, "{#SUMMARY}", page.summary);
	return html;
}

std::string Generator::renderContentInternal(const Article & article, hoedown_renderer* renderer){
	// Interpret settings for the renderer.
	const int options = HOEDOWN_EXT_TABLES | HOEDOWN_EXT_FENCED_CODE | HOEDOWN_EXT_FOOTNOTES |  HOEDOWN_EXT_AUTOLINK | HOEDOWN_EXT_STRIKETHROUGH | HOEDOWN_EXT_UNDERLINE | HOEDOWN_EXT_QUOTE | HOEDOWN_EXT_SUPERSCRIPT;
	// Allocate buffer for the media_width string (to support %, px, etc.).
	const auto & opts = _settings.imageWidth();
	const std::unique_ptr<std::uint8_t[]> mediaOpts = std::make_unique<std::uint8_t[]>(opts.size());
	std::copy(opts.begin(), opts.end(), mediaOpts.get());

	hoedown_document * doc = hoedown_document_new(renderer, static_cast<hoedown_extensions>(options), 16, mediaOpts.get(), opts.size(), int(_settings.imagesLinks()));
	const std::string & content = article.content();
	const size_t contentSize = content.size();

	// Make sure the buffer is large enough.
	if(_buffer->size < contentSize){
		hoedown_buffer_grow(_buffer, contentSize);
	}
	const std::unique_ptr<std::uint8_t[]> input = std::make_unique<std::uint8_t[]>(contentSize);
	std::copy(content.begin(), content.end(), input.get());
	hoedown_document_render(doc, _buffer, input.get(), contentSize);
	// Convert back.
	std::string result(_buffer->size, '\0');
	std::copy(_buffer->data, _buffer->data + _buffer->size, result.begin());
	
	hoedown_buffer_reset(_buffer);
	return result;
}

std::string Generator::renderContent(const Article & article){
	// Init renderer based on options.
	// Treat image title as width.
	hoedown_renderer* renderer = hoedown_html_renderer_new(hoedown_html_flags(0), 16, 1);
	// Interpret settings for the renderer.
	const std::string content = renderContentInternal(article, renderer);
	hoedown_html_renderer_free(renderer);
	return content;
}

std::string Generator::renderTableOfContent(const Article & article){
	// Init renderer based on options.
	// Use only two nesting levels in ToC.
	hoedown_renderer* renderer = hoedown_html_toc_renderer_new(3);
	// Interpret settings for the renderer.
	const std::string toc = renderContentInternal(article, renderer);
	hoedown_html_renderer_free(renderer);
	return toc;
}

bool Generator::savePage(const Page & page, const fs::path & outputDir, bool force) const {
	const fs::path outputFile = outputDir / page.location;
	System::createDirectory(outputFile.parent_path(), false);
	
	const bool fileExists = System::itemExists(outputFile);
	bool fileHasChanged = true;
	// If the file already exists, maybe its content hasn't changed.
	if(fileExists){
		const uint64_t newHash = TextUtilities::hash(page.html);
		// TODO: cache old hashes in a list on disk if too slow.
		const uint64_t oldHash = TextUtilities::hash(System::loadStringFromFile(outputFile));
		fileHasChanged = newHash != oldHash;
	}

	bool wrote = false;
	if(force || fileHasChanged){
		wrote = System::writeStringToFile(page.html, outputFile);
	}
	// Also copy related data.
	if(!page.files.empty()){
		// Assume they all go in the same directory.
	   const fs::path dirPath = (outputDir / page.files.front().second).parent_path();
	   System::createDirectory(dirPath, force);
	   for(const auto & file : page.files){
		   System::copyItem(file.first, outputDir / file.second, force);
	   }
	}
	return wrote;
}

size_t Generator::saveArticlePages(const std::vector<const PageArticle*>& pages, const fs::path & output, bool force){
	size_t count = 0;
	// Save pages.
	for(size_t aid = 0; aid < pages.size(); ++aid){
		const bool done = savePage(*pages[aid], output, force);
		count += size_t(done);
	}
	return count;
}

void Generator::generateIndexPage(const std::vector<const PageArticle*>& pages, const std::string& title, const fs::path& relativePath, const std::string& parentPath, Generator::Page& page){

	std::string html(_template.footer);
	for(size_t aid = 0; aid < pages.size(); ++aid){
		const PageArticle& page = *(pages[aid]);
		const std::string content = populateSnippet(page, relativePath, _template.indexItem);
		html.insert(html.begin(), content.begin(), content.end());
	}

	html.insert(html.begin(), _template.header.begin(),  _template.header.end());
	TextUtilities::replace(html, "{#BLOG_TITLE}", title);
	TextUtilities::replace(html, "{#AUTHOR}", _settings.defaultAuthor());
	TextUtilities::replace(html, "{#ROOT_LINK}", _settings.siteRoot());
	TextUtilities::replace(html, "{#RELATIVE_ROOT_LINK}", relativePath.generic_string());
	TextUtilities::replace(html, "{#PARENT_LINK}", parentPath);

	page.html = html;
}

void Generator::generateCategoriesPage(const std::map<std::string, std::vector<const PageArticle*>>& categoryArticles, const Categories& categories, const std::string& title, const fs::path& relativePath, const fs::path& parentPath, Generator::Page& page){

	// Sort categories.
	std::vector<std::string> keywordIDs;
	keywordIDs.reserve(categoryArticles.size());
	for(const auto& categ : categoryArticles){
		keywordIDs.emplace_back(categ.first);
	}
	std::sort(keywordIDs.begin(), keywordIDs.end());

	std::string html = _template.headerCategory;
	for(const std::string& categoryID : keywordIDs){

		const std::vector<const PageArticle*>& pages = categoryArticles.at(categoryID);
		const Category& infos = categories.at(categoryID);

		std::string htmlCat(_template.itemFooterCategory);
		for(size_t aid = 0; aid < pages.size(); ++aid){
			const PageArticle& page = *(pages[aid]);
			const std::string content = Generator::populateSnippet(page, relativePath, _template.itemArticleCategory);
			htmlCat.insert(htmlCat.begin(), content.begin(), content.end());
		}

		htmlCat.insert(htmlCat.begin(), _template.itemHeaderCategory.begin(),  _template.itemHeaderCategory.end());

		TextUtilities::replace(htmlCat, "{#CATEGORY_TITLE}", infos.name);
		TextUtilities::replace(htmlCat, "{#CATEGORY_ID}", categoryID);
		const fs::path relativePagePath = relativePath / infos.location;
		TextUtilities::replace(htmlCat, "{#CATEGORY_LINK}", relativePagePath.generic_string());

		html.append(htmlCat);
	}
	html.append(_template.footerCategory);
	TextUtilities::replace(html, "{#BLOG_TITLE}", title);
	TextUtilities::replace(html, "{#AUTHOR}", _settings.defaultAuthor());
	TextUtilities::replace(html, "{#ROOT_LINK}", _settings.siteRoot());
	TextUtilities::replace(html, "{#RELATIVE_ROOT_LINK}", relativePath.generic_string());
	TextUtilities::replace(html, "{#PARENT_LINK}", parentPath.generic_string());

	page.html = html;
}

void Generator::generateRssFeed(const std::vector<const PageArticle*>& pages, Generator::Page& feed){

	const std::string format = "%a, %d %b %Y %H:%M:%S %z";
	const std::string currentTime = Date::currentDate().str(format, EN_US_LOCALE);

	std::string feedXml("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
	feedXml.reserve(4096);
	feedXml.append("<rss version=\"2.0\"");
	feedXml.append("\n\txmlns:atom=\"http://www.w3.org/2005/Atom\"");
	feedXml.append("\n\txmlns:content=\"http://purl.org/rss/1.0/modules/content/\"");
	feedXml.append("\n\txmlns:dc=\"http://purl.org/dc/elements/1.1/\"");
	feedXml.append(" >\n");
	feedXml.append("<channel>\n");
	feedXml.append("\t<title>" + _settings.blogTitle() + "</title>\n");
	feedXml.append("\t<atom:link href=\"http://" + _settings.siteRoot() + "/feed.xml\" rel=\"self\" type=\"application/rss+xml\" />\n");
	feedXml.append("\t<link>http://" + _settings.siteRoot() + "</link>\n");
	feedXml.append("\t<description>" + _settings.blogTitle() + ", a blog");
	if(!_settings.defaultAuthor().empty()){
		feedXml.append(" by " + _settings.defaultAuthor());
	}
	feedXml.append(".</description>\n");
	feedXml.append("\t<language>en-US</language>\n");
	feedXml.append("\t<lastBuildDate>" + currentTime + "</lastBuildDate>\n");

	const size_t pageCount = pages.size();
	const size_t minRssId = (std::max)(long(0), long(pageCount) - long(_settings.rssCount()));

	for(size_t pid = minRssId; pid < pageCount; ++pid){

		const PageArticle& page = *(pages[pid]);
		const Article& article = *(page.article);

		const std::string dateStr = article.date().value().str(format, EN_US_LOCALE);
		const std::string url = "http://" + _settings.siteRoot() + "/" + page.location.generic_string();
		const std::string urlParent = "http://" + _settings.siteRoot() + "/" + page.location.parent_path().generic_string() + "/";
		feedXml.append("\t<item>\n");
		feedXml.append("\t\t<title>" + article.title() + "</title>\n");
		feedXml.append("\t\t<pubDate>" + dateStr + "</pubDate>\n");
		feedXml.append("\t\t<link>" + url + "</link>\n");
		feedXml.append("\t\t<dc:creator>" + article.author() + "</dc:creator>\n");
		feedXml.append("\t\t<description>" + page.summary + "</description>\n");
		feedXml.append("\t\t<guid>" + url + "</guid>\n");

		// Make all links absolute.
		std::string content = page.innerContent;
		std::string::size_type srcPos = content.find("src=\"");
		std::string::size_type endPos = std::string::npos;

		while(srcPos != std::string::npos){
			endPos = content.find("\"", srcPos + 5);
			const std::string link = content.substr(srcPos + 5, endPos - (srcPos + 5));
			if(!TextUtilities::hasPrefix(link, "http") && !TextUtilities::hasPrefix(link, "www.")){
				content.insert(srcPos + 5, urlParent);
			}
			srcPos = content.find("src=\"", endPos);
		}

		feedXml.append("\t\t<content:encoded><![CDATA[" + content + "]]></content:encoded>\n");
		feedXml.append("\t</item>\n");
	}

	feedXml.append("</channel>\n</rss>");
	feed.html = feedXml;
}

void Generator::generateSitemap(const std::vector<const PageArticle*>& articlePages, const std::vector<Page>& otherPages, const std::vector<const Page*>& indexPages, Generator::Page& sitemap){

	const std::string format = "%Y-%m-%d";
	const std::string currentDate = Date::currentDate().str(format, EN_US_LOCALE);

	std::string sitemapXml("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	sitemapXml.reserve(4096);
	sitemapXml.append("<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n");

	// Index page has the highest priority.
	for(const Page* indexPage : indexPages){
		const std::string url = "http://" + _settings.siteRoot() + "/" + indexPage->location.generic_string();
		sitemapXml.append("\t<url>\n");
		sitemapXml.append("\t\t<loc>" + url + "</loc>\n");
		sitemapXml.append("\t\t<changefreq>monthly</changefreq>\n");
		sitemapXml.append("\t\t<lastmod>" + currentDate + "</lastmod>\n");
		sitemapXml.append("\t\t<priority>1.0</priority>\n");
		sitemapXml.append("\t</url>\n");
	}
	
	// Articles.
	for(const PageArticle* page : articlePages){
		const std::string url = "http://" + _settings.siteRoot() + "/" + page->location.generic_string();
		const std::string date = page->article->date().value().str(format, EN_US_LOCALE);

		sitemapXml.append("\t<url>\n");
		sitemapXml.append("\t\t<loc>" + url + "</loc>\n");
		sitemapXml.append("\t\t<changefreq>yearly</changefreq>\n");
		sitemapXml.append("\t\t<lastmod>" + date + "</lastmod>\n");
		sitemapXml.append("\t\t<priority>0.6</priority>\n");
		sitemapXml.append("\t</url>\n");
	}

	// Other pages are lower priority.
	for(const Page& page : otherPages){
		const std::string url = "http://" + _settings.siteRoot() + "/" + page.location.generic_string();

		sitemapXml.append("\t<url>\n");
		sitemapXml.append("\t\t<loc>" + url + "</loc>\n");
		sitemapXml.append("\t\t<changefreq>yearly</changefreq>\n");
		sitemapXml.append("\t\t<priority>0.1</priority>\n");
		sitemapXml.append("\t</url>\n");
	}

	sitemapXml.append("</urlset>\n");
	sitemap.html = sitemapXml;
}

Generator::~Generator(){
	hoedown_buffer_free(_buffer);
}

std::string::size_type findFirstSentenceEnd(const std::string& src, size_t start){
	if(start + 1 >= src.size()){
		return src.size();
	}
	const std::string sentenceEnds = ".!?";
	std::string::size_type pos = src.find_first_of(sentenceEnds, start);
	if(pos == std::string::npos){
		return src.size();
	}
	// Early exit for last character.
	if(pos + 1 == src.size()){
		return pos;
	}
	// Check if this is really the end of a sentence.
	if(src[pos+1] != ' ' && src[pos+1] != '\n' && src[pos+1] != '\r' && src[pos+1] != '\t'){
		// Keep searching.
		return findFirstSentenceEnd(src, pos+1);
	}
	return pos;
}



