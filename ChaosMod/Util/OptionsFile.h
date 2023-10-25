#pragma once

#include "Logging.h"
#include "TryParse.h"
#include "Util/File.h"

#include <iostream>
#include <string>
#include <unordered_map>

class OptionsFile
{
  private:
	const char *m_szFileName;
	std::unordered_map<std::string, std::string> m_dictOptions;

  public:
	OptionsFile(const char *szFileName) : m_szFileName(szFileName)
	{
		Reset();
	}

	void Reset()
	{
		m_dictOptions.clear();

		bool bExists = true;

		if (!DoesFileExist(m_szFileName))
		{
			LOG("Config file " << m_szFileName << " not found!");
			return;
		}

		auto file = fopen(m_szFileName, "r");
		fseek(file, 0, SEEK_END);
		auto file_size = ftell(file);
		rewind(file);

		std::string buffer;
		buffer.resize(file_size);
		fread(buffer.data(), sizeof(char), file_size, file);

		while (!buffer.empty())
		{
			auto endl   = buffer.find('\n');
			auto line   = buffer.substr(0, endl);

			auto equate = line.find('=');
			// skip if line is a comment or empty
			if ((line.find('#') == line.npos || line.find_first_not_of(' ') != line.npos
			         ? line[line.find_first_not_of(' ')] != '#'
			         : false)
			    && equate != line.npos)
			{
				auto trim = [](std::string str) -> std::string
				{
					if (str.find_first_not_of(' ') == str.npos)
						return nullptr;
					str = str.substr(str.find_first_not_of(' '));
					str = str.substr(0, str.find_last_not_of(' ') + 1);
					return str;
				};
				auto key   = trim(line.substr(0, equate));
				auto value = trim(line.substr(equate + 1));

				if (!key.empty() && !value.empty())
					m_dictOptions[key] = value;
			}

			if (endl == buffer.npos)
				break;
			buffer = buffer.substr(endl + 1);
		}
	}

	template <typename T> inline T ReadValue(const std::string &szKey, T defaultValue) const
	{
		const auto &szValue = ReadValueString(szKey);

		if (!szValue.empty())
		{
			T result;
			if (Util::TryParse<T>(szValue, result))
			{
				return result;
			}
		}

		return defaultValue;
	}

	inline std::string ReadValueString(const std::string &szKey, const std::string &szDefaultValue = {}) const
	{
		const auto &result = m_dictOptions.find(szKey);

		if (result != m_dictOptions.end())
		{
			return result->second;
		}
		else
		{
			return szDefaultValue;
		}
	}
};