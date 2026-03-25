#pragma once

#include "Common.h"
#include "LinkedList.h"

/**
 * @file Handle.h
 * @brief Generational handle storage for stable, type-safe object references.
 *
 * This header implements an opaque handle system backed by a per-type storage.
 * A handle is a small value object that refers to an entry inside a central
 * storage array, rather than owning the object directly.
 *
 * ## Overview
 *
 * `Handle<T, DataType>` provides a lightweight reference to an object of type `T`
 * stored in a static `HandleStorage<T, DataType>`.
 *
 * Each handle value is composed of:
 * - an **index** into the storage array
 * - a **generation counter** used to detect stale handles
 *
 * When an object is destroyed, its slot is returned to a free list and may later
 * be reused. The generation counter changes on each allocation, so an old handle
 * to a reused slot will no longer validate successfully.
 *
 * This is commonly used when you want:
 * - cheap copyable IDs
 * - validation against use-after-free
 * - object lookup without exposing raw ownership
 * - a compact alternative to pointers for gameplay/runtime systems
 *
 * ## Main properties
 *
 * - **Typed handles**  
 *   A `Handle<Foo, uint32_t>` cannot be used as a handle to another type.
 *
 * - **Stale handle detection**  
 *   Reused storage slots are protected by a generation counter.
 *
 * - **O(1) create / destroy / lookup**  
 *   Allocation uses either append or a recycled free slot.
 *
 * - **Static per-type storage**  
 *   Each `Handle<T, DataType>` specialization owns one shared storage instance
 *   for all handles of that exact type/signature.
 *
 * - **Iterable live objects**  
 *   The storage can be iterated, skipping free slots automatically.
 *
 * ## Handle layout
 *
 * The underlying integer type (`DataType`) determines the handle size:
 *
 * - `uint32_t`  -> 16-bit index + 16-bit generation counter
 * - `uint64_t`  -> 32-bit index + 32-bit generation counter
 *
 * This lets you choose between smaller handles and larger capacity.
 *
 * ## Lifetime model
 *
 * A `Handle` is **not** an owning smart pointer.
 *
 * Destroying a handle object does **not** destroy the underlying `T`.
 * The user must explicitly call `release()` to free the referenced object.
 *
 * After `release()`:
 * - the handle becomes invalid
 * - any other copies of the same handle become stale
 * - future lookups through those stale handles will fail validation
 *
 * ## Validity rules
 *
 * A handle is valid only if:
 * - its stored index is inside the current storage bounds, and
 * - the generation counter matches the entry currently occupying that slot
 *
 * `tryGetObj()` returns `nullptr` if validation fails.
 * `getObj()` and `operator->()` assert on invalid access.
 *
 * ## Important caveats
 *
 * - **Not thread-safe**  
 *   Creation, destruction, and lookup are unsynchronized.
 *
 * - **Pointers/references are not stable across storage mutation**  
 *   Objects are stored inside a `std::vector`. Any pointer or reference obtained
 *   from a handle may be invalidated by later storage growth, destruction, reset,
 *   or slot reuse. Keep handles, not raw pointers, when long-lived access is needed.
 *
 * - **Manual release required**  
 *   This system separates object lifetime from handle object lifetime on purpose.
 *   That is powerful, but also a nice little trap if you forget to call `release()`.
 *
 * - **Counter wraparound is possible**  
 *   The generation counter is finite. In extremely long-running or high-churn
 *   scenarios, it can wrap. Pick a sufficiently large `DataType` if this matters.
 *
 * ## Intended usage
 *
 * Typical uses include:
 * - entity/object registries
 * - gameplay object references
 * - resource tables
 * - systems that need validation without raw pointer ownership
 *
 * Example:
 * @code
 * using ProjectileHandle = cz::Handle<Projectile, uint32_t>;
 *
 * ProjectileHandle h = ProjectileHandle::create(...);
 *
 * if (Projectile* p = h.tryGetObj())
 * {
 *     p->update();
 * }
 *
 * h.release();
 * @endcode
 *
 * ## Related internal types
 *
 * - `details::HandleMeta`  
 *   Packs/unpacks the index and generation counter into the underlying integer.
 *
 * - `details::HandleEntry<T>`  
 *   Stores either a live `T` or free-list metadata for a vacant slot.
 *
 * - `details::HandleStorage<T, DataType>`  
 *   Owns the storage array, free list, allocation logic, destruction logic,
 *   and iteration over live entries.
 *
 * - `details::BaseHandleStorage`  
 *   Base class used to register all storages globally for bulk reset.
 */

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

		class Iterator
		{
		  public:
			using iterator_category = std::forward_iterator_tag;
			using value_type = T;
			using difference_type = std::ptrdiff_t;
			using pointer = T*;
			using reference = T&;

			Iterator(HandleStorage* storage, size_t index)
				: m_storage(storage)
				, m_index(index)
			{
				skipFree();
			}

			reference operator*() const
			{
				return m_storage->data[m_index].getValue();
			}

			pointer operator->() const
			{
				return &m_storage->data[m_index].getValue();
			}

			Iterator& operator++()
			{
				++m_index;
				skipFree();
				return *this;
			}

			Iterator operator++(int)
			{
				Iterator tmp = *this;
				++(*this);
				return tmp;
			}

			bool operator==(const Iterator& other) const
			{
				return m_storage == other.m_storage && m_index == other.m_index;
			}

			bool operator!=(const Iterator& other) const
			{
				return !(*this == other);
			}

		  private:
			void skipFree()
			{
				while (m_index < m_storage->data.size() && m_storage->data[m_index].meta.bits.free)
					++m_index;
			}

			HandleStorage* m_storage = nullptr;
			size_t m_index = 0;
		};

		class ConstIterator
		{
		  public:
			using iterator_category = std::forward_iterator_tag;
			using value_type = const T;
			using difference_type = std::ptrdiff_t;
			using pointer = const T*;
			using reference = const T&;

			ConstIterator(const HandleStorage* storage, size_t index)
				: m_storage(storage)
				, m_index(index)
			{
				skipFree();
			}

			reference operator*() const
			{
				return m_storage->data[m_index].getValue();
			}

			pointer operator->() const
			{
				return &m_storage->data[m_index].getValue();
			}

			ConstIterator& operator++()
			{
				++m_index;
				skipFree();
				return *this;
			}

			ConstIterator operator++(int)
			{
				ConstIterator tmp = *this;
				++(*this);
				return tmp;
			}

			bool operator==(const ConstIterator& other) const
			{
				return m_storage == other.m_storage && m_index == other.m_index;
			}

			bool operator!=(const ConstIterator& other) const
			{
				return !(*this == other);
			}

		  private:
			void skipFree()
			{
				while (m_index < m_storage->data.size() && m_storage->data[m_index].meta.bits.free)
					++m_index;
			}

			const HandleStorage* m_storage = nullptr;
			size_t m_index = 0;
		};

		Iterator begin() { return Iterator(this, 0); }
		Iterator end() { return Iterator(this, data.size()); }
		ConstIterator begin() const { return ConstIterator(this, 0); }
		ConstIterator end() const { return ConstIterator(this, data.size()); }
		ConstIterator cbegin() const { return ConstIterator(this, 0); }
		ConstIterator cend() const { return ConstIterator(this, data.size()); }


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

