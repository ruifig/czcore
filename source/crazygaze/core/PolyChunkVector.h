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
 */
template<typename T, typename SizeType_ = size_t>
class PolyChunkVector
{
  public:
	using SizeType = SizeType_;

  protected:

	/**
	 * Stored before each element, so we have the information
	 * required to transverse the container.
	 */
	struct alignas(alignof(T)) Header
	{
		// Bytes from this header to the next header
		SizeType stride;
	};

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

	PolyChunkVector(size_t chunkCapacity = (sizeof(T) + sizeof(Header)) * 256)
	{
		getFreeChunk(chunkCapacity);
	}

	~PolyChunkVector()
	{
		clear();
		deleteAllChunks();
	}

	PolyChunkVector(const PolyChunkVector&) = delete;
	PolyChunkVector& operator=(const PolyChunkVector&) = delete;

	/**
	 * Finds space for size bytes in the container, allocating new chunks if necessary.
	 *
	 * @param size Number of bytes to allocate (excluding the header size).
	 * @return Pointer to the allocated space (after the header).
	 */
	void* getSpace(size_t size)
	{
		size_t totalSize = sizeof(Header) + size;
		// Check if we have enough space in the current chunk
		if ((m_tail->usedCap + totalSize) > m_tail->cap)
		{
			// Not enough space, get or allocate a new chunk
			getFreeChunk(std::max(totalSize, m_tail->cap));
		}
		m_lastHeader = reinterpret_cast<Header*>(m_tail->mem + m_tail->usedCap);
		m_lastHeader->stride = static_cast<SizeType>(totalSize);
		m_tail->usedCap += totalSize;
		return (m_lastHeader+1);
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
	 * Pushes out-of-band data alongside the objects.
	 * This is useful for storing variable size data alongside objects, e.g., strings or arrays, which can improve cache locality.
	 *
	 * @param data Pointer to the data to copy.
	 * @param size Size of the data to copy.
	 * @return Pointer to where the data was copied to. The returned pointer has the same alignment of T
	 */
	void* pushOOB(const void* data, const size_t size)
	{
		void *ptr;
		if (size== 0)
			return nullptr;

		size_t alignedSize = roundUpToMultipleOf(size, alignof(T));

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

		memcpy(ptr, data, size);
		assert(isMultipleOf(size_t(ptr), alignof(T)));
		return ptr;
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
			// If we only have 1 chunk, and it's the requested size, then nothing to do
			if (m_tail->next == nullptr && m_tail->cap == resetToOneChunk)
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

		#if 0
		void nextHeader()
		{
			m_pos += header()->stride;
		}

		// Skips until it finds an element (or the end of the chain)
		void skipInvalid()
		{
			auto isInvalidItPos = [this]()-> bool
			{
				if (!m_c) // end of chain
				{
					assert(m_pos = 0);
					return false;
				}

				if (m_pos < m_c->usedCap)
				{
					// If we are at the start of a chunk with skipFirstHeader, then it's not an element
					if (m_pos == 0 && m_c->skipFirstHeader)
						return true;
					else
						return false;
				}
				else
					return true;
			};

			while(m_c)
			{
				if (m_pos == 0 && m_c->skipFirstHeader)
					nextHeader();
			}

			// Skip empty chunks until we find the next item (or the end of the chain)
			while (m_c && m_pos >= m_c->usedCap)
			{
				// Move to next chunk
				m_c = m_c->next;
				m_pos = 0;
				// If the chunk has skipFirstHeader set, we need to skip the first header, because we have OOB
				// right at the start
				if (m_c && m_c->skipFirstHeader)
				{
					nextHeader();
				}
			}
		}
		#else
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
		#endif
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

	/**
	 * Pushes an out-of-band string_view
	 *
	 * It stores the string data alongside the objects, and returns a string_view pointing to the stored data.
	 *
	 * @param str The string to push
	 * @return A string_view pointing to the stored string data.
	 */
	std::string_view pushOOBString(std::string_view str)
	{
		return std::string_view(static_cast<char*>(pushOOB(str.data(), str.size())), str.size());
	}

	/**
	 * Pushes an out-of-band null-terminated string.
	 *
	 * It stores the string data alongside the objects, and returns a string_view pointing to the stored data.
	 *
	 * @param str The string to push
	 * @return A string_view pointing to the stored string data.
	 */
	const char* pushOOBString(const char* str)
	{
		return static_cast<char*>(pushOOB(str, strlen(str) +1));
	}

  protected:

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

} // namespace cz

#if defined(_MSVC_LANG)
__pragma(warning(pop))
#endif

