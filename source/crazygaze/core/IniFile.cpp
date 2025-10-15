#include "IniFile.h"
#include "File.h"
#include "StringUtils.h"
#include "Logging.h"

namespace cz
{


//////////////////////////////////////////////////////////////////////////
// Section
//////////////////////////////////////////////////////////////////////////

const cz::IniFile::Entry* IniFile::Section::tryGetEntry(const char* key) const
{
	for (auto&& e : entries)
	{
		if (e.name == key)
		{
			return &e;
		}
	}

	return nullptr;
}

IniFile::Entry& IniFile::Section::getEntry(const char* key)
{
	for(auto&& e : entries)
	{
		if (e.name == key)
			return e;
	}

	entries.push_back({key, ""});
	return entries.back();
}


//////////////////////////////////////////////////////////////////////////
// IniFile
//////////////////////////////////////////////////////////////////////////

bool IniFile::open(const fs::path& path)
{
	return openImpl(path, true);
}

bool IniFile::try_open(const fs::path& path)
{
	return openImpl(path, false);
}

#if 0
namespace details
{

	bool isDigit(char ch)
	{
		return ch >= '0' && ch <= '9';
	}

	template<typename T>
	bool tryNumber(const std::string& str, IniFile::Value& dst)
	{
		std::istringstream iss(str);
		T val;

		// noskipws considers leading whitespace invalid
		iss >> std::noskipws >> val;
		// Check the entire string was consumed and if either failbit or badbit is set
		if (iss.eof() && !iss.fail())
		{
			dst = val;
			return true;
		}
		else
		{
			return false;
		}
	}

	#if 0
	bool tryInt(const std::string& str, IniFile::Value& dst)
	{
	#if 0
		const char* ptr = str;
		if (!ptr)
		{
			return {};
		}

		if (*ptr=='-' || *ptr=='+')
		{
			ptr++;
		}

		while(*ptr)
		{
			if (!isDigit())
			{
				return {};
			}
			*ptr++;
		}

		return atoi(str);
		#endif

		return tryNumber<int>(str);
	}

	std::optional<float> tryFloat(const std::string& str)
	{
		return tryNumber<float>(str);
	}
	#endif

} // namespace details

#endif


bool IniFile::openImpl(const fs::path& path, bool logOpenError)
{
	File::Buffer data = File::readAll(path, logOpenError);
	if (!data)
	{
		return false;
	}

	std::vector<std::string> lines = stringSplitIntoLinesVector(data.to_string_view(), false);

	for(auto&& line : lines)
	{
		line = trim(line);
		std::string::iterator it;

		if (line.c_str()[0] == ';' || line.c_str()[0] == '#')
		{
			// Its a comment
		}
		else if (line.c_str()[0] == '[')  // its a section
		{
			std::string sectionName(line.begin() + 1, line.end() - 1);
			sections.push_back({});
			sections.back().name = trim(sectionName);
		}
		else if ((it = std::find(line.begin(), line.end(), '=')) != line.end())	 // its a value
		{
			if (sections.size())
			{
				std::string key(line.begin(), it);
				key = trim(key);
				std::string value(it + 1, line.end());
				value = trim(value);

				Entry& entry = sections.back().getEntry(key.c_str());

				// If it starts and finished with a " or ', we remove them if there aren't any other " or ' in the middle
				if (
					((*value.begin() == '"')  && (std::find(value.begin()+1, value.end(), '"')  == (value.end() - 1))) ||
					((*value.begin() == '\'') && (std::find(value.begin()+1, value.end(), '\'') == (value.end() - 1)))
				)
				{
					value = std::string(value.begin() + 1, value.end() - 1);
				}

				entry.value = value;
			}
			else
			{
				CZ_LOG(Main, Warning, "Value with no section in INI File '{}' : '{}'", narrow(path.c_str()), line);
			}
		}
		else if (line == "")
		{
		}
		else
		{
			CZ_LOG(Main, Warning, "Invalid line in INI File '{}' : '{}'", narrow(path.c_str()), line);
		}
	}

	return true;
}

bool IniFile::save(const fs::path& path)
{
	auto file = File::open(path, File::Mode::Write);
	if (!file)
	{
		return false;
	}

	#if 0
	auto writeString = [&file](const std::string& str)
	{
		file->write(str.c_str(), str.size());
	};

	auto lineBreak = [&file]()
	{
		static const char* linebreak = "\r\n";
		file->write(linebreak, strlen(linebreak));
	};

	for(auto&& section : m_sections)
	{
		if (section->m_name.size())
		{
			writeString(std::format("{}\r\n", section->m_name));
		}
	}
	#endif

	std::ostringstream os;
	for(auto&& section : sections)
	{
		os << '[' << section.name << ']' << "\r\n";
		for(auto&& entry : section.entries)
		{
			os << entry.name << '=' << entry.value << "\r\n";
		}
	}

	file->write(os.view().data(), os.view().size());

	return true;
}

const IniFile::Section* IniFile::tryGetSection(const char* name) const
{
	for (auto&& s : sections)
	{
		if (s.name == name)
		{
			return &s;
		}
	}

	return nullptr;
}

IniFile::Section& IniFile::getSection(const char* name)
{
	for (auto&& s : sections)
	{
		if (s.name == name)
		{
			return s;
		}
	}

	sections.push_back({});
	sections.back().name = name;
	return sections.back();
}

} // namespace cz


