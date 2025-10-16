#pragma once

#include "Common.h"
#include "PlatformUtils.h"
#include "StringUtils.h"

namespace cz
{

struct IniFile
{
	struct Entry
	{
		std::string name;
		std::string value;
	};

	struct Section
	{
		/**
		 * Looks for an existing entry. If it doesn't exist, it returns nullptr
		 */
		const Entry* tryGetEntry(std::string_view key) const;

		/**
		 * Looks for for an entry. If it doesn't exist, it creates one.
		 */
		Entry& getEntry(std::string_view key);

		template<typename T>
		void setValue(std::string_view key, const T& value)
		{
			Entry& entry = getEntry(key);

			if constexpr (std::is_same_v<T, bool>)
			{
				entry.value = value ? "true" : "false";
			}
			else
			{
				entry.value = std::to_string(value);
			}
		}

		template<typename T>
		bool getValue(std::string_view key, T& dst) const
		{
			const Entry* entry = tryGetEntry(key);
			if (!entry)
			{
				return false;
			}
			
			if constexpr(std::is_same_v<T, std::string>)
			{
				dst = entry->value;
				return true;
			}
			else if constexpr(std::is_same_v<T, bool>)
			{
				if (entry->value == "0" ||  asciiStrEqualsCi(entry->value, "false"))
				{
					dst = false;
					return true;
				}
				else if (entry->value == "1" ||  asciiStrEqualsCi(entry->value, "true"))
				{
					dst = true;
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				std::istringstream is{ entry->value };
				T val;
				char c;
				if ((is >> val) && !(is >> c))
				{
					dst = val;
					return true;
				}
				else
				{
					return false;
				}
			}
		}

		inline bool getValue(std::string_view key, std::string& dst)
		{
			const Entry* entry = tryGetEntry(key);
			if (!entry)
			{
				return false;
			}

			dst = entry->value;
			return true;
		}

		std::string name;
		std::vector<Entry> entries;
	};

	IniFile() {}

	bool open(const fs::path& path);
	bool try_open(const fs::path& path);
	bool save(const fs::path& path);

	/**
	 * Finds a section.
	 * Returns nullptr if it doesn't exist
	 */
	const Section* tryGetSection(std::string_view name) const;

	/**
	 * Finds a section.
	 * If it doesn't exist, then it is created
	 */
	Section& getSection(std::string_view name);

	template<typename T>
	bool getValue(std::string_view section, std::string_view key, T& dst) const
	{
		if (const Section* s = tryGetSection(section))
		{
			return s->getValue(key, dst);
		}
		else
		{
			return false;
		}
	}

	template<typename T>
	void setValue(std::string_view section, std::string_view key, const T& value)
	{
		getSection(section).setValue(key, value);
	}

	std::vector<Section> sections;

private:
	bool openImpl(const fs::path& path, bool logOpenError);
};

} // namespace cz

