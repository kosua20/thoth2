#pragma once

#include "Common.hpp"
#include "Settings.hpp"
#include <optional>
#include <ctime>

class Date {
public:
	
	Date(const std::string & date, const std::string & format);
	
	std::string str(const std::string & format = "", const std::string & locale = "") const;
	
	friend bool operator<(const Date & a, const Date & b);
	
private:
	
	std::tm _date;
	std::time_t _time;
	std::string _initialStr;
};

class Article {
public:
	
	enum Type {
		Draft, Public
	};
	
	Article(const std::string & title, const std::optional<Date> & date, const std::string & author, const std::string & content);
	
	const std::string & title() const { return _title; }
	
	const std::string & content() const { return _content; }
	
	const std::optional<Date> & date() const { return _date; }
	
	std::string dateStr() const;
	
	const std::string & author() const { return _author; };
	
	const fs::path & url() const { return _url; }
	
	const Type & type() const { return _type; }
	
	static bool isValid(const fs::path & fileName);
	
	static std::optional<Article> loadArticle(const fs::path & path, const Settings & settings);
	
	static std::vector<Article> loadArticles(const fs::path & dir, const Settings & settings);
	
private:
	
	std::string generateURL() const;
	
	/// The title of the article
	std::string _title;
	
    /// The content of the article, formatted in Markdown
	std::string _content;
    
    /// The date of publication of the article (if it is not a draft)
	std::optional<Date> _date;
    
    /// The author of the article
	std::string _author;
    
    /// Denotes if the article is a draft or a published article
	Type _type;
    
    /// A short summary composed of the beginning of the article text, stripped of its HTML components
	//std::string _summary;
	
	fs::path _url;
};
