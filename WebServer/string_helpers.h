#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

inline std::string trim(const std::string &str)
{
	if (str.empty())
		return {};

	auto first_index = str.find_first_not_of(" ");
	if (first_index == str.npos)
		return {};

	std::string new_str = str;
	new_str             = str.substr(first_index);
	new_str             = str.substr(0, str.find_last_not_of(" ") + 1);

	return new_str;
}

inline std::vector<std::string> split(const std::string &str, std::string_view delimiter)
{
	if (str.empty())
		return {};

	std::vector<std::string> parts;
	size_t index_start = 0, index_head = 0;
	while ((index_start = str.find_first_not_of(delimiter, index_head)) != str.npos)
	{
		index_head = str.find(delimiter, index_start);
		parts.push_back(str.substr(index_start, index_head == str.npos ? str.npos : index_head - index_start));
	}

	return parts;
}

inline std::string random_string(size_t length)
{
	auto randchar = []() -> char
	{
		const char charset[]   = "0123456789"
		                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		                         "abcdefghijklmnopqrstuvwxyz";
		const size_t max_index = (sizeof(charset) - 1);
		return charset[rand() % max_index];
	};
	std::string str(length, 0);
	std::generate_n(str.begin(), length, randchar);
	return str;
}