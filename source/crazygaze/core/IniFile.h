#pragma once

#include "Common.h"
#include "PlatformUtils.h"

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
		const Entry* tryGetEntry(const char* key) const;

		/**
		 * Looks for for an entry. If it doesn't exist, it creates one.
		 */
		Entry& getEntry(const char* key);

		template<typename T>
		void setValue(const char* key, const T& value)
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
		bool getValue(const char* key, T& dst) const
		{
			const Entry* entry = tryGetEntry(key);
			if (!entry)
			{
				return false;
			}
			
			std::istringstream is{ entry->value };
			T val;
			char c;
			if ((is >> std::boolalpha >> val) && !(is >> c))
			{
				dst = val;
				return true;
			}
			else
			{
				return false;
			}
		}

		inline bool getValue(const char* key, std::string& dst)
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
	const Section* tryGetSection(const char* name) const;

	/**
	 * Finds a section.
	 * If it doesn't exist, then it is created
	 */
	Section& getSection(const char* name);

	template<typename T>
	bool getValue(const char* section, const char* key, T& dst) const
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
	void setValue(const char* section, const char* key, const T& value)
	{
		getSection(section).setValue(key, value);
	}

	std::vector<Section> sections;

private:
	bool openImpl(const fs::path& path, bool logOpenError);
};

} // namespace cz

