#include "system/TextUtilities.hpp"

#include <xxhash/xxhash.h>

std::string TextUtilities::trim(const std::string & str, const std::string & del) {
	const size_t firstNotDel = str.find_first_not_of(del);
	if(firstNotDel == std::string::npos) {
		return "";
	}
	const size_t lastNotDel = str.find_last_not_of(del);
	return str.substr(firstNotDel, lastNotDel - firstNotDel + 1);
}

std::string TextUtilities::removeExtension(std::string & str) {
	const std::string::size_type pos = str.find_last_of('.');
	if(pos == std::string::npos) {
		return "";
	}
	const std::string ext(str.substr(pos));
	str.erase(str.begin() + pos, str.end());
	return ext;
}

std::string TextUtilities::parentDirectory(const std::string & str){
	if(str == "/" || str == "\\"){
		return str;
	}
	const std::string::size_type lastSep = str.find_last_of("/\\");
	if(lastSep == std::string::npos){
		return "";
	}
	const size_t shift = ((lastSep + 1) == str.size()) ? 1 : 0;
	const std::string str1 = str.substr(0, int(str.size()) - shift);
	const std::string::size_type lastSep1 = str1.find_last_of("/\\");
	if(lastSep1 == std::string::npos){
		return "";
	}
	return str1.substr(0, lastSep1);
}

void TextUtilities::replace(std::string & source, const std::string & fromString, const std::string & toString) {
	std::string::size_type nextPos = 0;
	const size_t fromSize		   = fromString.size();
	const size_t toSize			   = toString.size();
	while((nextPos = source.find(fromString, nextPos)) != std::string::npos) {
		source.replace(nextPos, fromSize, toString);
		nextPos += toSize;
	}
}

bool TextUtilities::hasPrefix(const std::string & source, const std::string & prefix) {
	if(prefix.empty() || source.empty()) {
		return false;
	}
	if(prefix.size() > source.size()) {
		return false;
	}
	const std::string sourcePrefix = source.substr(0, prefix.size());
	return sourcePrefix == prefix;
}

bool TextUtilities::hasSuffix(const std::string & source, const std::string & suffix) {
	if(suffix.empty() || source.empty()) {
		return false;
	}
	if(suffix.size() > source.size()) {
		return false;
	}
	const std::string sourceSuffix = source.substr(source.size() - suffix.size(), suffix.size());
	return sourceSuffix == suffix;
}

std::string TextUtilities::join(const std::vector<std::string> & tokens, const std::string & delimiter){
	std::string accum;
	for(size_t i = 0; i < tokens.size(); ++i){
		accum.append(tokens[i]);
		if(i != (tokens.size() - 1)){
			accum.append(delimiter);
		}
	}
	return accum;
}

std::vector<std::string> TextUtilities::split(const std::string & str, const std::string & delimiter, bool skipEmpty){
	std::string subdelimiter = " ";
	if(delimiter.empty()){
		Log::Warning() << "Delimiter is empty, using space as a delimiter." << std::endl;
	} else {
		subdelimiter = delimiter.substr(0,1);
	}
	if(delimiter.size() > 1){
		Log::Warning() << "Only the first character of the delimiter will be used (" << delimiter[0] << ")." << std::endl;
	}
	std::stringstream sstr(str);
	std::string value;
	std::vector<std::string> tokens;
	while(std::getline(sstr, value, subdelimiter[0])) {
		if(!skipEmpty || !value.empty()) {
			tokens.emplace_back(value);
		}
	}
	return tokens;
}


std::string TextUtilities::lowercase(const std::string & src){
	std::string dst(src);
	std::transform(src.begin(), src.end(), dst.begin(),
				   [](unsigned char c){
		return std::tolower(c);
	});
	return dst;
}

uint64_t TextUtilities::hash(const std::string& str){
	return uint64_t(XXH3_64bits(str.data(), str.size()));
}

std::string TextUtilities::summarize(const std::string & htmlText, const size_t length){
	if(length == 0){
		return "";
	}

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

	openPos = summary.find("[");
	while(openPos != std::string::npos){
		// Find the next closing bracket.
		std::string::size_type closePos = summary.find("]", openPos+1);
		// Remove everything between the two.
		const std::string newString = summary.substr(0, openPos) + summary.substr(closePos+1);
		summary = newString;
		openPos = summary.find("[");
	}

	TextUtilities::replace(summary, " .", ".");
	TextUtilities::replace(summary, "…", "...");
	TextUtilities::replace(summary, "\n", " ");
	TextUtilities::replace(summary, "\r", "");
	TextUtilities::replace(summary, "\t", " ");

	size_t targetLength = std::min(summary.size(), length);

	// Try to cut between two words.
	std::string::size_type pos = summary.find_last_of(" ", targetLength-1);
	if(pos != std::string::npos){
		targetLength = pos;
	}

	summary = summary.substr(0, targetLength);

	summary = TextUtilities::trim(summary, " \t");
	summary.append("…");
	return summary;
}

std::string TextUtilities::sanitizeUrl(const std::string& name) {
	std::string str = TextUtilities::lowercase(name);
	TextUtilities::replace(str, "/", "_");
	TextUtilities::replace(str, " ", "_");

	const std::string urlCompliant = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"; // remove "/"
	for(size_t cid = 0; cid < str.size(); ++cid){
		if(urlCompliant.find(str[cid]) == std::string::npos){
			str[cid] = '@';
		}
	}
	TextUtilities::replace(str, "@", "");
	return str;
}

std::string TextUtilities::uppercaseFirst(const std::string& str) {
	if(str.empty()){
		return "";
	}
	std::string str1 = str;
	const std::string letters = "abcdefghijklmnopqrstuvwxyz";
	const std::string letterCaps = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	for(unsigned int lid = 0; lid < letters.size(); ++lid){
		if(str1[0] == letters[lid]){
			str1[0] = letterCaps[lid];
			break;
		}
	}
	return str1;
}

bool TextUtilities::hasUppercase(const std::string& str) {
	const std::string letterCaps = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	return str.find_first_of(letterCaps) != std::string::npos;
}
