#pragma once

#include "Common.h"
#include "Singleton.h"
#include "StringUtils.h"

namespace cz
{

/**
 * Processes command line parameters
 * 
 * Processing is done as follow:
 *
 * - Flags/options are prefixed with `-`. E.g:
 *		game.exe -fullscreen
 * - Flags/options can have a `=`, which gives it a single value. E.g:
 *		game.exe -resx=1024 -resy=768
 * - Flags/options can have multiple values, by adding anything after that doesn't have a `=`. e.g
 *		game.exe -process file1 file2
 *		Values are separated by spaces.
 *
 * If a value (in either the =XXX or not) starts with a `"`, the end of that value will be on the next `"`. This allows specifying
 * values that contain spaces.
 * 
 */
class CommandLine : public Singleton<CommandLine>
{
  public:

	struct Param
	{
		std::string name;
		std::vector<std::string> values;
	};

	/**
	 * used internally only
	 *
	 * Returns true if there are no unexpected things in the command line. It returns false if there are things that don't follow
	 * the expected format.
	 * Not that even if it returns false, the caller can still proceed if it wants, but any command line parameters that were
	 * not parsed will not be available to query.
	 */
	bool init(int argc, char* argv[]);

	/**
	 * Returns the first parameter. Makes it possible to use range based for loops
	 * If there are no parameters, dereferencing this is undefined behavior.
	 */
	const Param* begin() const
	{
		return m_params.data();
	}

	/**
	 * Returns one past the last parameter. Makes it possible to use range based for loops
	 * WARNING: Do not dereference this. It points to invalid memory.
	 */
	const Param* end() const
	{
		return m_params.data() + m_params.size();
	}

	/**
	 * Checks if the specified parameter exists, regardless if it has values or not.
	 */
	bool has(std::string_view name, bool ignoreCase = true) const;

	/**
	 * Returns a parameter, or nullptr if not found
	 */
	const Param* getParam(std::string_view name, bool ignoreCase = true) const;

	/**
	 * Retrieves a parameter's first value.
	 * - If the parameter is not set, or is set but doesn't have any values, it returns false
	 * - If the value cannot be converted to the desired type, it returns false
	 * 
	 * @param dst On exit, it contains the parameters first value
	 */
	template<typename T>
	bool getValue(const char* name, T& dst) const
	{
		const Param* p = getParam(name);
		if (!p || p->values.size() == 0)
		{
			return false;
		}

		const std::string pval = p->values[0];

		if constexpr(std::is_same_v<bool, T>)
		{
			if (pval == "0" || asciiStrEqualsCi(pval, "false"))
			{
				dst = false;
				return true;
			}
			else if (pval == "1" || asciiStrEqualsCi(pval, "true"))
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
			std::istringstream is{ pval};
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

	/**
	 * Specialization of getValue for strings
	 */
	bool getValue(const char* name, std::string& dst)
	{
		const Param* p = getParam(name);
		if (!p || p->values.size() == 0)
		{
			return false;
		}

		dst = p->values[0];
		return true;
	}

	/**
	 * Gets a parameter's first value
	 * If the parameter is not set, has no values, or the value cannot be converted to the desired type, it returns the specified
	 * defaultValue
	 */
	template<typename T>
	T getValueOrDefault(const char* name, T defaultValue)
	{
		T dst;
		if (getValue(name, dst))
		{
			return dst;
		}
		else
		{
			return defaultValue;
		}
	}

  private:

	std::vector<Param> m_params;
};

} // namespace cz

