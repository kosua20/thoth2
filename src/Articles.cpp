#include "Articles.hpp"
#include "system/TextUtilities.hpp"
#include "system/System.hpp"
#include <iomanip>

Date::Date(){}

Date::Date(const std::string & date, const std::string & format) {
	_date = {};
	_initialStr = date;
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
	localtime_r(&date._time, &date._date);
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
	std::string str = _type == Public ? _date.value().str() : "DRAFT";
	str.append("_");
	str.append(_title);
	str = TextUtilities::lowercase(str);
	TextUtilities::replace(str, "/", "-");
	TextUtilities::replace(str, " ", "_");
	
	const std::string ulrCompliant = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
	
	// Urgh.
	for(size_t cid = 0; cid < str.size(); ++cid){
		if(ulrCompliant.find(str[cid]) == std::string::npos){
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
	return std::optional<Article>(Article(title, date, author, body));
	
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
