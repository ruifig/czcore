#pragma once

#include "Common.h"
#include "Logging.h"

namespace cz
{

class BaseSingleton
{
  public:
	BaseSingleton() = default;
	virtual ~BaseSingleton() = default;
};

template<typename T>
class Singleton : public BaseSingleton
{
  public: 
	Singleton()
	{
		CZ_CHECK(ms_instance == nullptr);
		ms_instance = static_cast<T*>(this);
	}

	virtual ~Singleton()
	{
		CZ_CHECK(ms_instance);
		ms_instance = nullptr;
	}

	Singleton(const Singleton&) = delete;
	Singleton(Singleton&&) = delete;
	Singleton& operator=(const Singleton&) = delete;
	Singleton& operator=(Singleton&&) = delete;

	static T& get()
	{
		CZ_CHECK(ms_instance);
		return *ms_instance;
	}

	/**
	 * Allows the caller to check if the singleton exists
	 */
	static T* tryGet()
	{
		return ms_instance;
	}

	inline static T* ms_instance = nullptr;
};

} // namespace cz


