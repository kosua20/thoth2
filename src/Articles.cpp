#include "Articles.hpp"
#include "system/TextUtilities.hpp"
#include "system/System.hpp"
#include <iomanip>
#include <ctime>

Date::Date(){}

Date::Date(const std::string & date, const std::string & format) {
	_date = {};
	_initialStr = TextUtilities::trim(date, " \t\n\r");
	std::istringstream str(date);
	str >> std::get_time(&_date, format.c_str());
	if(str.fail()) {
		Log::Error() << "Parsing failed for date \"" << date << "\" using format " << format << "." << std::endl;
		return;
	}
	_date.tm_hour = 10;
	_date.tm_min = 0;
	_date.tm_sec = 0;
	_time = std::mktime(&_date);
}

size_t Date::year() const {
	return (size_t)(std::max)(0, _date.tm_year + 1900);
}

size_t Date::month() const {
	return (size_t)(_date.tm_mon);
}

std::string Date::str(const std::string & format, const std::string & locale) const {
	if(format.empty()){
		return _initialStr;
	}
	std::stringstream str;
	if(!locale.empty()){
		str.imbue(std::locale(locale));
	}
	str << std::put_time(&_date, format.c_str());
	return str.str();
}

bool operator<(const Date & a, const Date & b){
	return std::difftime(b._time, a._time) > 0;
}

Date Date::currentDate(){
	Date date;
	date._initialStr = "";
	date._time = std::time(0);
	std::tm* dateTmp = std::localtime(&date._time);
	memcpy( &date._date, dateTmp, sizeof( std::tm ) );
	return date;
}


Article::Article(const std::string & title, const std::optional<Date> & date, const std::string & author, const std::string & content){
	_title = title;
	_date = date;
	_author = author;
	_content = content;
	_type = date ? Public : Draft;
	_url = fs::path(generateURL());
}

std::string Article::generateURL() const {
	//std::string str = _type == Public ? _date.value().str() : "DRAFT";
	//str.append("_");
	std::string pageName = TextUtilities::lowercase(_title);
	TextUtilities::replace(pageName, "/", "_");
	TextUtilities::replace(pageName, " ", "_");
	std::string str = _type == Public ? _date.value().str("%Y/%m") : "";
	str.append("/");
	str.append(pageName);
	
	const std::string urlCompliant = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_/";
	
	// Urgh.
	for(size_t cid = 0; cid < str.size(); ++cid){
		if(urlCompliant.find(str[cid]) == std::string::npos){
			str[cid] = '@';
		}
	}
	TextUtilities::replace(str, "@", "");
	return str;
}

std::string Article::dateStr() const {
	return _date ? _date.value().str() : "DRAFT";
}

bool Article::isValid(const fs::path & file){
	if(file.extension().string() != ".md"){
		return false;
	}
	const std::string filename = file.filename().string();
	return !TextUtilities::hasPrefix(filename, "_")
		&& !TextUtilities::hasPrefix(filename, "#");
}


std::optional<Article> Article::loadArticle(const fs::path & path, const Settings & settings){
	const std::string content = System::loadStringFromFile(path);
	if(content.empty()){
		return std::optional<Article>();
	}
	
	const std::string::size_type firstDouble = content.find("\n\n");
	if(firstDouble == std::string::npos){
		return std::optional<Article>();
	}
	const std::string header = content.substr(0, firstDouble);
	const std::string body = content.substr(firstDouble + 2);
	if(header.empty() || body.empty()){
		return std::optional<Article>();
	}
	
	const std::vector<std::string> headerTokens = TextUtilities::split(header, "\n", false);
	std::string title = headerTokens[0];
	// ?
	TextUtilities::replace(title, "##", "");
	title = TextUtilities::trim(title, "#");
	std::optional<Date> date;
	if(headerTokens.size() > 1){
		const std::string dateStr = TextUtilities::lowercase(headerTokens[1]);
		if(dateStr != "draft"){
			date.emplace(dateStr, settings.dateStyle());
		}
	}
	std::string author = settings.defaultAuthor();
	if(headerTokens.size() > 2 && !headerTokens[2].empty()){
		author = headerTokens[2];
	}
	auto article = std::optional<Article>(Article(title, date, author, body));

	// Keywords
	if(headerTokens.size() > 3 && !headerTokens[3].empty()){
		auto keywords = TextUtilities::split(headerTokens[3], ",", true);
		for(const std::string& rawKeyword : keywords){
			std::string keyword = TextUtilities::trim(rawKeyword, " ,#");
			keyword = TextUtilities::lowercase(keyword);
			article.value().addKeyword(keyword);
		}
		std::sort(article.value()._keywords.begin(), article.value()._keywords.end());
	}
	return article;
	
}


std::vector<Article> Article::loadArticles(const fs::path & dir, const Settings & settings){

	// Load articles.
	std::vector<Article> articles;
	const auto files = System::listItems(dir, false, false);
	for(const auto & file: files){
		if(!Article::isValid(file)){
			continue;
		}
		const auto article = Article::loadArticle(file, settings);
		if(article){
			articles.push_back(article.value());
		}
	}
	// Sort articles.
	std::sort(articles.begin(), articles.end(),[](const Article & a, const Article & b){
		if(a.date() && b.date()){
			return a.date().value() < b.date().value();
		} else if(a.date()){
			return true;
		}
		return false;
	});
	return articles;
}

void Article::addKeyword(const std::string& keyword){
	_keywords.push_back(keyword);
}
