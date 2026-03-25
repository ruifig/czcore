#pragma once

#include "Common.h"

namespace cz
{

namespace details
{

	/**
	 * What holds a handle's data.
	 * 
	 */
	template<typename BitsType>
	union HandleMeta;

	template<>
	union HandleMeta<uint64_t>
	{
		struct
		{
			uint32_t idx;
			uint32_t counter; 
		} bits;
		uint64_t all = 0;
	};

	template<>
	union HandleMeta<uint32_t>
	{
		struct
		{
			uint16_t idx;
			uint16_t counter; 
		} bits;
		uint32_t all = 0;
	};

	static_assert(sizeof(HandleMeta<uint64_t>) == sizeof(uint64_t));
	static_assert(sizeof(HandleMeta<uint32_t>) == sizeof(uint32_t));


	template <typename T>
	struct HandleEntry
	{
		struct Meta
		{
			union
			{
				struct
				{
					uint64_t extra : 63;

					// NOTE: Intentionally using the term `free` instead of `used`, so that when `free` is 0, it means we can treat the entire thing as the handle value,
					// which speeds up comparisons (no need to bit fiddling to validate the handle).
					//
					// With that in mind we have:
					// - If `free` is 0, then `meta` has handle's value, and we cal just compare `all` against the handle value.
					// - If `free` is 1, then `meta` has the index to the next free slot
					uint64_t free : 1;
				} bits;

				uint64_t all = std::numeric_limits<uint64_t>::max();
			};
		} meta;

		static_assert(sizeof(Meta) == sizeof(uint64_t));

		HandleEntry() = default;

		HandleEntry(const T& v)
		{
			meta.bits.free = false;
			std::construct_at(getPtr(), v);
		}

		HandleEntry(T&& v)
		{
			meta.bits.free = false;
			std::construct_at(getPtr(), std::move(v));
		}

		~HandleEntry()
		{
			if (!meta.bits.free)
				std::destroy_at(getPtr());
		}

		HandleEntry(const HandleEntry& other)
		{
			copyFrom(other);
		}

		HandleEntry(HandleEntry&& other)
		{
			moveFrom(std::move(other));
		}

		HandleEntry& operator=(const HandleEntry& other)
		{
			if (this != &other)
				copyFrom(other);
			return *this;
		}

		HandleEntry& operator=(HandleEntry&& other)
		{
			if (this != &other)
				moveFrom(std::move(other));
			return *this;
		}

		uint8_t alignas(alignof(T)) buf[sizeof(T)] = "empty...";

		T& getValue()
		{
			CZ_CHECK(!meta.bits.free);
			return reinterpret_cast<T&>(buf);
		}

		const T& getValue() const
		{
			CZ_CHECK(!meta.bits.free);
			return reinterpret_cast<const T&>(buf);
		}

	  protected:


		T* getPtr()
		{
			return reinterpret_cast<T*>(buf);
		}

		void copyFrom(const HandleEntry& other)
		{
			if (!meta.bits.free)
			{
				meta = other.meta;
				if (!other.meta.bits.free)
					getValue() = other.getValue();
				else
					std::destroy_at(getPtr());
			}
			else
			{
				meta = other.meta;
				if (!other.meta.bits.free)
					std::construct_at(getPtr(), other.getValue());
				else
				{
					// nothing to do
				}
			}
		}

		void moveFrom(HandleEntry&& other)
		{
			if (!meta.bits.free)
			{
				meta = other.meta;
				if (!other.meta.bits.free)
				{
					getValue() = std::move(other.getValue());
					std::destroy_at(other.getPtr());
					other.meta.bits.free = true;
				}
				else
				{
					std::destroy_at(getPtr());
				}
			}
			else
			{
				meta= other.meta;
				if (!other.meta.bits.free)
				{
					std::construct_at(getPtr(), std::move(other.getValue()));
					std::destroy_at(other.getPtr());
					other.meta.bits.free = true;
				}
				else
				{
					// nothing to do
				}
			}
		}
	};

	class BaseHandleStorage : public DoublyLinked<BaseHandleStorage>
	{
	  public:
		BaseHandleStorage()
		{
			ms_all.pushBack(this);
		}
		virtual ~BaseHandleStorage()
		{
			ms_all.remove(this);
		}

		virtual void reset() = 0;

		static inline DoublyLinkedList<BaseHandleStorage> ms_all;
	};

	/**
	 * Storage for handles of type T
	 */
	template<typename T, typename HT>
	class HandleStorage : public BaseHandleStorage
	{
	  public:

		using HMeta = HandleMeta<HT>;
		HandleStorage()
		{
			reset();
		}
		~HandleStorage()
		{
		}

		static constexpr uint32_t invalidIndex = std::numeric_limits<uint32_t>::max();

		void reset() override
		{
			data = std::vector<HandleEntry<T>>();
			nextFree = invalidIndex;
			counter = 0;
		}

		template<typename... Args>
		HT create(Args&&... args)
		{
			HandleEntry<T>* e;
			HMeta hmeta;
			hmeta.bits.counter = static_cast<decltype(hmeta.bits.counter)>(++counter);

			if (nextFree == invalidIndex)
			{
				hmeta.bits.idx = static_cast<decltype(hmeta.bits.idx)>(data.size());
				e = &data.emplace_back(T(std::forward<Args>(args)...));
			}
			else
			{
				hmeta.bits.idx = static_cast<decltype(hmeta.bits.idx)>(nextFree);
				e = &data[nextFree];
				CZ_CHECK(e->meta.bits.free == 1);
				nextFree = static_cast<decltype(nextFree)>(e->meta.bits.extra);
				*e = HandleEntry<T>(T(std::forward<Args>(args)...));
			}

			e->meta.all = hmeta.all;

			 return hmeta.all;
		}

		void destroy(HMeta meta)
		{
			CZ_CHECK(meta.all && (meta.bits.idx < data.size()));

			details::HandleEntry<T>& e = data[meta.bits.idx];
			CZ_CHECK(e.meta.all == meta.all);

			e = {};
			e.meta.bits.extra = nextFree;
			nextFree = meta.bits.idx;
		}

		std::vector<HandleEntry<T>> data;
		uint32_t nextFree;
		uint32_t counter;
	};

} // namespace details

template<typename T, typename DataType>
struct Handle
{
	static inline details::HandleStorage<T, DataType> storage;

	details::HandleMeta<DataType> meta;

	template<typename... Args>
	static Handle<T, DataType> create(Args&&... args)
	{
		Handle<T, DataType> res;
		res.meta.all = storage.create(std::forward<Args>(args)...);
		return res;
	}

	bool isValid() const
	{
		return tryGetObj() ? true : false;
	}

	T& getObj()
	{
		return getObjImpl();
	}

	const T& getObj() const
	{
		return getObjImpl();
	}

	T* tryGetObj()
	{
		return tryGetObjImpl();
	}

	const T* tryGetObj() const
	{
		return tryGetObjImpl();
	}

	T* operator->()
	{
		return &getObj();
	}

	const T* operator->() const
	{
		return &getObj();
	}

	void release()
	{
		if (meta.all == 0)
			return;

		storage.destroy(meta);
		meta.all = 0;
	}

  protected:

	T& getObjImpl() const
	{
		const T* p = tryGetObj();
		CZ_CHECK(p);
		return *p;
	}

	T* tryGetObjImpl() const
	{
		if (meta.all && (meta.bits.idx < storage.data.size()))
		{
			details::HandleEntry<T>& e = storage.data[meta.bits.idx];
			return e.meta.all == meta.all ? &e.getValue() : nullptr;
		}
		else
		{
			return nullptr;
		}
	}
};

} // namespace cz

