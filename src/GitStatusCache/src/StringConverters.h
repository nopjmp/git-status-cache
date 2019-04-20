#pragma once

inline std::string ConvertToUtf8(const std::wstring& unicode)
{
	if (unicode.empty()) return std::string();
	int needed = WideCharToMultiByte(CP_UTF8, 0, unicode.c_str(), (int)unicode.size(), NULL, 0, NULL, FALSE);
	std::string str(needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, unicode.c_str(), (int)unicode.size(), &str[0], needed, NULL, FALSE);
	return str;
}

inline std::wstring ConvertToUnicode(const std::string& utf8String)
{
	if (utf8String.empty()) return std::wstring();
	int needed = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), (int)utf8String.size(), NULL, 0);
	std::wstring wstr(needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), (int)utf8String.size(), &wstr[0], needed);
	return wstr;
}