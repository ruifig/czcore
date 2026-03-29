#pragma once

#include "crazygaze/core/Common.h"
#include "crazygaze/core/Math.h"

#if defined(_MSVC_LANG)
__pragma(warning(push))
__pragma(warning(disable: 4324))  /* 'structname': structure was padded due to alignment specifier */
#endif

// Set this to 1 to clear memory to 0xAA on allocation and 0xCC on clearing
#define POLYCHUNKVECTOR_CLEARMEM 0

namespace cz
{

/**
 * A vector-like container that allows storing polymorphic types in chunks.
 * It has the following characteristics:
 * 
 * - A base type `T` is specified as a template parameter.
 * - Respects the alignment of `T`.
 *   It then allows storing any type derived from `T`.
 * - Types can be polymorphic
 * - Once created, objects are never moved. This means that pointers/references to objects remain valid until the container is destroyed or cleared.
 *   This is implemented by allocating memory in chunks. When once chunk is exhausted, it allocates another chunk.
 * - Objects can't be removed. The only way to remove objects is to clear the entire container.
 * - Allows inserting oob (out of band) data alongside the objects.
 *   - This is useful for storing variable size data alongside objects, e.g., strings or arrays, which can improve cache locality.
 *
 * The best use case for this container is to create cache friend command queues.
 *
 * When constructed, no memory is allocated until the first object is added.
 * To do a "reserve" similar to what std containers do, use `clear` with the `resetToSingleChunk` parameter after construction.
 */
template<typename T, typename SizeType_ = size_t>
class PolyChunkVector
{
  protected:

	/**
	 * Stored before each element, so we have the information
	 * required to transverse the container.
	 */
	struct alignas(alignof(T)) Header
	{
		// Bytes from this header to the next header
		SizeType_ stride;
	};

  public:
	using SizeType = SizeType_;

	// What is the initial chunk capacity, if elements are pushed before any clear with resetToSingleChunk
	constexpr static SizeType InitialChunkCapacity = (sizeof(Header) + sizeof(T)) * 1024;

  protected:

	struct Chunk
	{
		CZ_DELETE_COPY_AND_MOVE(Chunk);
		explicit Chunk(size_t capacity)
		{
			mem = static_cast<uint8_t*>(malloc(capacity));
			cap = capacity;

			#if POLYCHUNKVECTOR_CLEARMEM
				memset(mem, 0xAA, capacity);
			#endif
		}

		~Chunk()
		{
			assert(usedCap == 0);
			free(mem);
		}
		uint8_t* mem;
		size_t cap;
		size_t usedCap = 0;

		// This is used for when we insert OOB data when the chunk is still empty.
		// When that happens, we set this to true, and insert an header so we can track the stride,
		// but that header will be skipped when iterating elements.
		bool skipFirstHeader = false;

		Chunk* next = nullptr;
	};

  public:

	PolyChunkVector() = default;

	~PolyChunkVector()
	{
		clear();
		deleteAllChunks();
	}

	CZ_DELETE_COPY(PolyChunkVector);

	PolyChunkVector(PolyChunkVector&& other)
	{
		swap(*this, other);
	}

	PolyChunkVector& operator=(PolyChunkVector&& other)
	{
		clear();
		deleteAllChunks();
		swap(*this, other);
		return *this;
	}

	friend void swap(PolyChunkVector& a, PolyChunkVector& b)
	{
		std::swap(a.m_head       , b.m_head);
		std::swap(a.m_tail       , b.m_tail);
		std::swap(a.m_lastHeader , b.m_lastHeader);
		std::swap(a.m_numElements, b.m_numElements);
	}

	template<class Derived, typename... Args>
		requires std::is_base_of_v<T, Derived>
	Derived& emplace_back(Args&&... args)
	{
		static_assert(alignof(Derived) <= alignof(T), "Derived type is not properly aligned");

		void* ptr = getSpace(sizeof(Derived));
		assert(isMultipleOf(reinterpret_cast<size_t>(ptr), alignof(T)));
		Derived* obj = new (ptr) Derived(std::forward<Args>(args)...);
		m_numElements++;

		return *obj;
	}

	/**
	 * Reserves out-of-band data space alongside the objects.
	 * This is used to store things such as strings, or any data of variable size.
	 *
	 * @return The pointer to where the data can be copied
	 */
	void* reserveOOB(const size_t size)
	{
		if (size== 0)
			return nullptr;

		size_t alignedSize = roundUpToMultipleOf(size, alignof(T));
		void *ptr;

		// If there is a m_lastHeader, try to append the OOB data to it if it fits
		if (m_lastHeader && ((m_tail->usedCap + alignedSize ) <= m_tail->cap))
		{
			ptr = m_tail->mem + m_tail->usedCap;
			m_lastHeader->stride += static_cast<SizeType>(alignedSize);
			m_tail->usedCap += alignedSize;
		}
		else
		{
			ptr = getSpace(alignedSize);
			m_tail->skipFirstHeader = true;
		}

		assert(isMultipleOf(size_t(ptr), alignof(T)));
		return ptr;
	}

	/**
	 * Pushes out-of-band data alongside the objects.
	 * This is useful for storing variable size data alongside objects, e.g., strings or arrays, which can improve cache locality.
	 *
	 * @param data Pointer to the data to copy.
	 * @param size Size of the data to copy.
	 * @return Pointer to where the data was copied to. The returned pointer has the same alignment of T
	 */
	void* pushOOB(const void* data, const size_t size)
	{
		void* ptr = reserveOOB(size);
		if (!ptr)
			return nullptr;
		memcpy(ptr, data, size);
		return ptr;
	}

	/**
	 * Pushes an out-of-band string_view
	 *
	 * It stores the string data alongside the objects, and returns a string_view pointing to the stored data.
	 * A null-terminator is added, to make it easier to use the data as a C-string if needed.
	 *
	 * @param str The string to push
	 * @return A string_view pointing to the stored string data.
	 */
	std::string_view pushOOBString(std::string_view str)
	{
		char* ptr = reinterpret_cast<char*>(reserveOOB(str.size() + 1));
		memcpy(ptr, str.data(), str.size());
		// .data() does not necessarily include a null-terminator, so we need to add one seperately
		ptr[str.size()] = 0;
		return std::string_view{ptr, str.size()};
	}

	/*
	 * Similar to the one that takes a string_view.
	 * A null-terminator is added, to make it easier to use the data as a C-string if needed.
	 */
	std::string_view pushOOBString(const std::string& str)
	{
		char* ptr = reinterpret_cast<char*>(reserveOOB(str.size() + 1));
		// c_str() already includes the null-terminator, so no need for adding it seperately.
		memcpy(ptr, str.c_str(), str.size() + 1);
		return std::string_view{ptr, str.size()};
	}

	/**
	 * Pushes an out-of-band C-style string
	 * It stores the null-terminator as well.
	 *
	 * @param str The string to push
	 * @return A pointer to the null-terminated string
	 */
	const char* pushOOBString(const char* str)
	{
		return static_cast<char*>(pushOOB(str, strlen(str) +1));
	}

	/**
	 * Transverses the chunks and returns the used and total capacity of the container.
	 * first - used capacity across all chunks
	 * second - total capacity across all chunks
	 */
	std::pair<size_t, size_t> calcCapacity() const
	{
		size_t used = 0;
		size_t total = 0;
		for (const Chunk* chunk = m_head; chunk != nullptr; chunk = chunk->next)
		{
			used += chunk->usedCap;
			total += chunk->cap;
		}

		return {used, total};
	}

	/**
	 * Returns the number of elements stored in the container.
	 */
	size_t size() const
	{
		return m_numElements;
	}

	/**
	 * Clears the container, calling destructors of all stored objects.
	 *
	 * @param resetToSingleChunk
	 *	If non-zero, after clearing the container, it deallocates all chunks creates a single chunk with this value as it's capacity.
	 *	This can be useful for e.g a game that clear the container every frame. E.g:
	 *		- The game starts with some default chunk capacity
	 *		- As objects are added, the container grows and allocated more chunks.
	 *		- The next frame, the game resets the container with `clear(calcUsedCapacity())`, which
	 *		  means that for the next frame one chunk will probably be big enough to hold all objects.
	 */
	void clear(size_t resetToOneChunk = 0)
	{
		Chunk* c = m_head;
		while(c)
		{
			size_t pos = 0;
			if (c->skipFirstHeader)
			{
				Header* h = reinterpret_cast<Header*>(c->mem);
				pos += h->stride;
			}

			while(pos < c->usedCap)
			{
				Header* h = reinterpret_cast<Header*>(c->mem + pos);
				T* obj = reinterpret_cast<T*>(h + 1);
				obj->~T();

				pos += h->stride;
			}

			c->usedCap = 0;
			c->skipFirstHeader = false;
			#if POLYCHUNKVECTOR_CLEARMEM
				memset(c->mem, 0xCC, c->cap);
			#endif
			c = c->next;
		}
		m_tail = m_head;
		m_lastHeader = nullptr;
		m_numElements = 0;

		if (resetToOneChunk)
		{
			// If we only have 1 chunk, and its already >=the requested size, then nothing to do
			if (m_tail && m_tail->next == nullptr && m_tail->cap >= resetToOneChunk)
				return;

			deleteAllChunks();
			getFreeChunk(resetToOneChunk);
		}
	}

	struct Iterator
	{
		explicit Iterator(Chunk* c, size_t pos)
			: m_c(c)
			, m_pos(pos)
		{
		}

		Iterator(const Iterator&) = default;
		Iterator(Iterator&&) = default;
		Iterator& operator=(const Iterator&) = default;
		Iterator& operator=(Iterator&&) = default;

		T& operator*() const
		{
			Header* h = header();
			T* obj = reinterpret_cast<T*>(h + 1);
			return *obj;
		}

		Iterator& operator++()
		{
			assert(m_c);
			m_pos += header()->stride;
			findValid();
			return *this;
		}

		bool operator==(const Iterator& other) const
		{
			return m_c == other.m_c && m_pos == other.m_pos;
		}

		bool operator!=(const Iterator& other) const
		{
			return !(*this == other);
		}

		private:

		Chunk* m_c = nullptr;
		size_t m_pos = 0;

		friend PolyChunkVector;

		Header* header() const
		{
			return reinterpret_cast<Header*>(m_c->mem + m_pos);
		}

		void findValid()
		{
			//
			// Valid positions are:
			// m_c == nullptr : end of chain (aka end() iterator)
			// (m_pos < m_c->usedCap && m_pos > 0)
			// (m_pos < m_c->usedCap && m_pos == 0 && !m_c->skipFirstHeader)

			while(true)
			{
				if (m_c == nullptr)
				{
					assert(m_pos == 0);
					return;
				}

				if (m_pos < m_c->usedCap)
				{
					if (m_pos > 0 || (m_pos == 0 && !m_c->skipFirstHeader))
						return;

					m_pos += header()->stride;
				}

				// Note that this can't be an "else" because we can change m_pos in the previous if and we need to check
				// if it's past usedCap
				if (m_pos >= m_c->usedCap)
				{
					m_c = m_c->next;
					m_pos = 0;
				}
			}
		}
	};

	Iterator begin() const
	{
		Iterator it{m_head, 0};
		// Since the chunk chain can have holes with empty chunks, we need to advance to the first valid element
		it.findValid();
		return it;
	}

	Iterator end() const
	{
		return Iterator{nullptr, 0};
	}

  protected:

	/**
	 * Finds space for size bytes in the container, allocating new chunks if necessary.
	 *
	 * @param size Number of bytes to allocate (excluding the header size).
	 * @return Pointer to the allocated space (after the header).
	 */
	void* getSpace(size_t size)
	{
		size_t totalSize = sizeof(Header) + size;

		if (m_tail == nullptr) // case where we don't have any chunks yet
		{
			getFreeChunk(std::max(totalSize, InitialChunkCapacity));
		}
		else if ((m_tail->usedCap + totalSize) > m_tail->cap) // case where we have chunks but not enough space in the current tail
		{
			// We allocate a new chunk with at least the same capacity as the last one
			getFreeChunk(std::max(totalSize, m_tail->cap));
		}

		m_lastHeader = reinterpret_cast<Header*>(m_tail->mem + m_tail->usedCap);
		m_lastHeader->stride = static_cast<SizeType>(totalSize);
		m_tail->usedCap += totalSize;
		return (m_lastHeader+1);
	}


	/**
	 * Deletes all allocated chunks.
	 */
	void deleteAllChunks()
	{
		assert(m_numElements == 0);

		while (m_head)
		{
			Chunk* n = m_head->next;
			delete m_head;
			m_head = n;
		}

		m_tail = nullptr;
		m_lastHeader = nullptr;
	}

	void getFreeChunk(size_t chunkCapacity)
	{ 
		chunkCapacity = roundUpToMultipleOf(std::max(sizeof(Header) + sizeof(T), chunkCapacity), alignof(T));

		m_lastHeader = nullptr;

		// Case for when there is no chunks yet.
		if (!m_tail)
		{
			assert(m_numElements == 0);
			Chunk* c = new Chunk(chunkCapacity);
			m_head = m_tail = c;
			return;
		}

		// Try to find any existing chunk that has enough capacity
		while(m_tail->next)
		{
			m_tail = m_tail->next;
			// By definition any chunks we are transversing should be empty
			assert(m_tail->usedCap == 0);

			// Found an empty chunk that is big enough
			if (m_tail->cap >= chunkCapacity)
				return;
		}

		// There was no chunk big enough, so allocate one at the end of the chain
		m_tail->next = new Chunk(chunkCapacity);
		m_tail = m_tail->next;
	}

	//
	// Since chunks are not necessarily released, it means that once we clear the container, we keep a chain
	// of chunks that are not in use.
	// Therefore, m_tail points to the chunk we are adding elements to, but not necessarily the last chunk in the chain.
	// The first allocated chunk
	Chunk* m_head = nullptr;
	Chunk* m_tail = nullptr;

	// The last header inserted.
	// We used this so we can insert OOB data after it.
	Header* m_lastHeader = nullptr;

	std::size_t m_numElements = 0;
};


/**
 * Container to store commands in a cache friendly way, for later execution.
 *
 * The base type needs to have a virtual method `exec(...)`, which will be called when executing the commands.
 *
 * This utilizes PolyChunkVector so it avoids heat allocations.
 *
 * When implementing this, I compared performance with a similar implementation based on std::vector<std::function<...>>,
 * where enough space is reserved ahead of time to avoid reallocations as much as possible, I got the following results:
 * 
 * - Memory usage is about 40% less. This will depend on the size of the capture state of the lambdas being stored,
 *   since std::function uses a Small Buffer optimization. So, if the capture state is very small, lots of space is wasted with
 *   the std implementation.
 * - This is about 2x faster than the std version for small capture states
 * - For a capture state that needs to capture a string, the std version needs to duplicate std::string (expensive),
 *   while this version allows storing the string data alongside the command with `pushOOBString`, and so this version is about
 *   4-5x faster.
 *
 * Obviously results will depend on the scenario, but I did not find a situation where this implementation was slower than the
 * std one.
 *
 */

class CommandVector
{
  protected:

	struct Cmd
	{
		virtual ~Cmd() = default;
		virtual void operator()() = 0;
	};

	template<typename F>
	requires std::invocable<F>
	struct CmdWrapper : public Cmd
	{
		CmdWrapper(F&& f)
			: payload(std::forward<F>(f))
		{
		}
		
		void operator()() override
		{
			payload();
		}

		F payload;
	};

	PolyChunkVector<Cmd> m_cmds;

  public:

	/**
	 * @param chunkCapacity
	 *	Initial individual chunk capacity in bytes. Ideally, this should be big enough to hold many commands to reduce the number
	 *  of allocations.
	 *
	 * To have an idea of the ideal size, run your application, then use `calcCapacity` after executing a typical workload, to see
	 * how much capacity is used. Then use that value as the `chunkCapacity`, which means it will create one chunk big enough to
	 * hold all commands in a typical workload.
	 */
	CommandVector(size_t chunkCapacity = 0)
	{
		if (chunkCapacity)
			m_cmds.clear(chunkCapacity);
	}

	/**
	 * Pushes a lambda
	 */
	template<typename F>
	requires std::invocable<F>
	void push(F&& f)
	{
		m_cmds.emplace_back<CmdWrapper<F>>(std::forward<F>(f));
	}

	/**
	 * Returns the used capacity in bytes.
	 *
	 * If the workload is about the same per frame, this can be fed to `clear`, to cause the next frame to allocate just one chunk big
	 * enough to hold all commands.
	 */
	size_t calcCapacity() const
	{
		return m_cmds.calcCapacity().first;
	}

	/** 
	 * Execute all commands.
	 * Note that the commands are NOT removed from the containers. Use `clear` to do that.
	 *
	 * @return The number of commands executed.
	 */
	size_t executeAll()
	{
		for (Cmd& cmd : m_cmds)
		{
			cmd();
		}
		return m_cmds.size();
	}

	/**
	 * Clears the container, calling destructors of all stored objects.
	 *
	 * @param resetToSingleChunk
	 *	If non-zero, after clearing the container, it deallocates all chunks creates a single chunk with this value as it's capacity.
	 *	This can be useful for e.g a game that clear the container every frame. E.g:
	 *		- The game starts with some default chunk capacity
	 *		- As objects are added, the container grows and allocated more chunks.
	 *		- The next frame, the game resets the container with `clear(calcUsedCapacity())`, which
	 *		  means that for the next frame one chunk will probably be big enough to hold all objects.
	 */
	void clear(size_t resetToOneChunk = 0)
	{
		m_cmds.clear(resetToOneChunk);
	}

	/**
	 * Returns the number of elements in the container
	 */
	size_t size() const
	{
		return m_cmds.size();
	}
};

} // namespace cz

#if defined(_MSVC_LANG)
__pragma(warning(pop))
#endif

