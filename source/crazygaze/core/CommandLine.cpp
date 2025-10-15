#include "CommandLine.h"
#include "StringUtils.h"
#include "Logging.h"

namespace cz
{

bool CommandLine::init(int argc, char* argv[])
{
	Param* currParam = nullptr;

	int errorCount = 0;

	// Returns a pointer to the next specified character the end of the string if the character wasn't found
	auto findNext = [](const char* ptr, char ch) -> const char*
	{
		while (*ptr && *ptr != ch)
		{
			ptr++;
		}
		return ptr;
	};

	for (int i = 1; i < argc; i++)
	{
		const char* arg = argv[i];
		const char* ptr = arg;

		if (*ptr == '-')
		{
			ptr++;

			const char* end = findNext(ptr, '=');
			auto paramName = std::string_view(ptr, end);

			if (getParam(paramName))
			{
				errorCount++;
				CZ_LOG(Main, Error, "Duplicated parameter : '-{}'", paramName);
				continue;
			}

			Param param;
			param.name = std::string_view(ptr, end);

			if (*end == '=')
			{
				currParam = nullptr;

				// If an argument is in the form -ABC=Something, we consider "Something" as the only value
				end++;

				// -ABC= without a value is invalid
				if (*end == 0)
				{
					errorCount++;
					CZ_LOG(Main, Error, "Invalid parameter format: '{}'", arg);
				}
				else
				{
					param.values.emplace_back(std::string_view(end, strlen(end)));
					m_params.push_back(std::move(param));
				}
			}
			else
			{
				// A -ABC parameter without a = means any arguments until the next -XXX will be values for -ABC
				m_params.push_back(std::move(param));
				currParam = &m_params.back();
			}

			continue;
		}

		if (currParam == nullptr)  // values without having a current parameter are not allowed
		{
			errorCount++;
			CZ_LOG(Main, Error, "Unexpected parameter: '{}'", arg);
			continue;
		}

		currParam->values.push_back(arg);
	}

	return errorCount == 0 ? true : false;
}

bool CommandLine::has(std::string_view name, bool ignoreCase) const
{
	return getParam(name, ignoreCase) == nullptr ? false : true;
}

const CommandLine::Param* CommandLine::getParam(std::string_view name, bool ignoreCase) const
{
	if (ignoreCase)
	{
		for (auto&& p : m_params)
		{
			if (asciiStrEqualsCi(p.name, name))
			{
				return &p;
			}
		}

	}
	else
	{
		for (auto&& p : m_params)
		{
			if (p.name == name)
			{
				return &p;
			}
		}
	}

	return nullptr;
}

} // namespace cz 


