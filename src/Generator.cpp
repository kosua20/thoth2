#include "Generator.hpp"
#include "system/TextUtilities.hpp"
#include "system/System.hpp"

#include <hoedown/html.h>
#include <hoedown/document.h>
#include <map>

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
	// Init renderer based on options.
	// Treat image title as width.
	_renderer = hoedown_html_renderer_new(hoedown_html_flags(0), 16, 1);
	_buffer = hoedown_buffer_new(100);
	
}


void Generator::process(const std::vector<Article> & articles, uint mode){
	_articles = articles;
	// Convert the markdown representation to html for each article.
	Log::Info() << Log::Generation << "Processing pages... ";
	_pages.resize(_articles.size());
	for(size_t aid = 0; aid < _articles.size(); ++aid){
		renderPage(_articles[aid], _pages[aid]);
	}
	Log::Info() << "done." << std::endl;
	
	const bool force = bool(mode & FORCE);
	
	if(mode & ARTICLES){
		Log::Info() << Log::Generation << "Creating article pages... ";
		System::createDirectory(_settings.outputPath() / "articles", force);
		const size_t count = savePages(Article::Public, _settings.outputPath(), force);
		Log::Info() << count << " new created pages." << std::endl;
	}
	if(mode & DRAFTS){
		Log::Info() << Log::Generation << "Creating drafts pages... ";
		System::createDirectory(_settings.outputPath() / "drafts", force);
		const size_t count = savePages(Article::Draft, _settings.outputPath(), force);
		Log::Info() << count << " new created pages." << std::endl;
	}
	
	if(mode & (INDEX | ARTICLES | DRAFTS)){
		Log::Info() << Log::Generation << "Generating index pages:" << std::endl;
		std::vector<Page> indexes;
		generateIndexPages(indexes);
		for(const auto & index : indexes){
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

void Generator::renderPage(const Article & article, Page & page){
	const std::string dir = article.type() == Article::Public ? "articles" : "drafts";
	page.location = fs::path(dir) / article.url();
	page.location.replace_extension("html");
	
	const std::string content = renderContent(article);
	page.summary = summarize(content, 400);
	page.innerContent = content;
	
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
			const fs::path locPath = article.url() / srcPath.filename();
			const fs::path dstPath = fs::path(dir) / locPath;
			page.files.push_back({srcPath, dstPath});
			TextUtilities::replace(page.innerContent, srcLink, locPath.generic_string());
		}
		srcPos = content.find("src=\"", endPos);
	}

	//Generate complete html.
	std::string html(_template.article);
	TextUtilities::replace(html, "{#TITLE}", article.title());
	TextUtilities::replace(html, "{#DATE}", article.dateStr());
	TextUtilities::replace(html, "{#AUTHOR}", article.author());
	TextUtilities::replace(html, "{#BLOG_TITLE}", _settings.blogTitle());
	TextUtilities::replace(html, "{#LINK}", page.location.generic_string());
	TextUtilities::replace(html, "{#SUMMARY}", page.summary);
	TextUtilities::replace(html, "{#AUTHOR}", article.author());
	if(!_template.syntax.empty() && content.find("<pre>") != std::string::npos){
		TextUtilities::replace(html, "</head>", "\n" + _template.syntax + "\n</head>");
	}

	TextUtilities::replace(html, "{#CONTENT}", page.innerContent);
	page.html = html;
	
}

std::string Generator::renderContent(const Article & article){
	// Interpret settings for the renderer.
	const int options = HOEDOWN_EXT_TABLES | HOEDOWN_EXT_FENCED_CODE | HOEDOWN_EXT_FOOTNOTES |  HOEDOWN_EXT_AUTOLINK | HOEDOWN_EXT_STRIKETHROUGH | HOEDOWN_EXT_UNDERLINE | HOEDOWN_EXT_QUOTE | HOEDOWN_EXT_SUPERSCRIPT;
	// Allocate buffer for the media_width string (to support %, px, etc.).
	const auto & opts = _settings.imageWidth();
	const std::unique_ptr<std::uint8_t[]> mediaOpts = std::make_unique<std::uint8_t[]>(opts.size());
	std::copy(opts.begin(), opts.end(), mediaOpts.get());
	
	hoedown_document * doc = hoedown_document_new(_renderer, static_cast<hoedown_extensions>(options), 16, mediaOpts.get(), opts.size(), int(_settings.imagesLinks()));
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

bool Generator::savePage(const Page & page, const fs::path & outputDir, bool force) const {
	const fs::path outputFile = outputDir / page.location;
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

size_t Generator::savePages(const Article::Type & type, const fs::path & output, bool force){
	size_t count = 0;
	// Save pages.
	for(size_t aid = 0; aid < _articles.size(); ++aid){
		if(_articles[aid].type() == type){
			const bool done = savePage(_pages[aid], output, force);
			count += size_t(done);
		}
	}
	return count;
}

void Generator::generateIndexPages(std::vector<Generator::Page> & pages){
	pages.clear();
	pages.reserve(3);
	
	std::map<Article::Type, std::string> indexHtml;
	indexHtml[Article::Public] = std::string(_template.footer);
	indexHtml[Article::Draft] = std::string(_template.footer);
	const std::map<Article::Type, fs::path> indexName = {
		{ Article::Draft, "index-drafts.html" },
		{ Article::Public, "index.html" }
	};
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

	const unsigned int minRssId = (std::max)(0, int(_articles.size()) - int(_settings.rssCount()));

	for(size_t aid = 0; aid < _articles.size(); ++aid){
		const Article & article = _articles[aid];
		const Page & page = _pages[aid];
		
		std::string html(_template.indexItem);
		TextUtilities::replace(html, "{#TITLE}", article.title());
		TextUtilities::replace(html, "{#DATE}", article.dateStr());
		TextUtilities::replace(html, "{#AUTHOR}", article.author());
		TextUtilities::replace(html, "{#LINK}", page.location.generic_string());
		TextUtilities::replace(html, "{#SUMMARY}", page.summary);
		html.append("\n");
		
		std::string & currIndex = indexHtml.at(article.type());
		currIndex.insert(currIndex.begin(), html.begin(), html.end());
		
		if(article.type() == Article::Public && aid >= minRssId){
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
	}
	
	feedXml.append("</channel>\n</rss>");
	for(auto & index : indexHtml){
		index.second.insert(index.second.begin(), _template.header.begin(),  _template.header.end());
		TextUtilities::replace(index.second, "{#BLOG_TITLE}", _settings.blogTitle());
		pages.emplace_back();
		pages.back().html = index.second;
		pages.back().location = indexName.at(index.first);
	}
	
	pages.emplace_back();
	pages.back().html = feedXml;
	pages.back().location = fs::path("feed.xml");
}

Generator::~Generator(){
	hoedown_buffer_free(_buffer);
	hoedown_html_renderer_free(_renderer);
}

std::string Generator::summarize(const std::string & htmlText, const size_t length){
	// Just remove all "<...>" for now.
	std::string summary = htmlText;
	std::string::size_type openPos = summary.find("<");
	while(openPos != std::string::npos){
		// Find the next closing bracket.
		std::string::size_type closePos = summary.find(">", openPos+1);
		// Remove everything between the two.
		const std::string newString = summary.substr(0, openPos) + summary.substr(closePos+1);
		summary = newString;
		openPos = summary.find("<");
	}
	TextUtilities::replace(summary, "\n", "");
	TextUtilities::replace(summary, "\r", "");
	TextUtilities::replace(summary, "\t", " ");
	summary = summary.substr(0, std::min(length, summary.size()));
	summary = TextUtilities::trim(summary, " \t");
	summary.append("...");
	return summary;
}
