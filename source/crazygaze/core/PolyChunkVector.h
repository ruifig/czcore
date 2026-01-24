#pragma once

#include "crazygaze/core/Common.h"
#include "crazygaze/core/Math.h"

#if defined(_MSVC_LANG)
__pragma(warning(push))
__pragma(warning(disable: 4324))  /* 'structname': structure was padded due to alignment specifier */
#endif

namespace cz
{

/**
 * A vector-like container that allows storing polymorphic types in chunks.
 * It has the following characteristics:
 * 
 * - A base type `T` is specified as a template parameter.
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
template<typename T, typename SizeType_ = uint32_t>
class PolyChunkVector
{
  protected:

	struct Chunk
	{
		CZ_DELETE_COPY_AND_MOVE(Chunk);
		explicit Chunk(size_t capacity)
		{
			num = counter++;
			mem = static_cast<uint8_t*>(malloc(capacity));
			cap = capacity;
			std::println(" Chunk() : {}", num);
		}

		~Chunk()
		{
			assert(usedCap == 0);
			std::println("~Chunk() : {}", num);
			free(mem);
		}
		uint8_t* mem;
		size_t usedCap = 0;
		size_t cap;
		Chunk* next = nullptr;

		// #CZGE : Remove these
		inline static int counter = 0;
		int num;
	};

  public:

	using SizeType = SizeType_;

	PolyChunkVector(size_t chunkCapacity = (sizeof(T) + sizeof(Header)) * 256)
	{
		std::println("sizeof(Header) = {}", sizeof(Header));
		getFreeChunk(chunkCapacity);
	}

	~PolyChunkVector()
	{
		clear();
		deleteAllChunks();
	}

	PolyChunkVector(const PolyChunkVector&) = delete;
	PolyChunkVector& operator=(const PolyChunkVector&) = delete;

	template<class Derived, typename... Args>
		requires std::is_base_of_v<T, Derived>
	Derived& emplace_back(Args&&... args)
	{
		static_assert(alignof(Derived) <= alignof(T), "Derived type is not properly aligned");

		size_t totalSize = sizeof(Header) + sizeof(Derived);

		// Check if we have enough space in the current chunk
		if ((m_tail->usedCap + totalSize) > m_tail->cap)
		{
			// Not enough space, get or allocate a new chunk
			getFreeChunk(std::max(totalSize, m_tail->cap));
		}

		// Construct the derived object in place
		Header* newHeader = reinterpret_cast<Header*>(m_tail->mem + m_tail->usedCap);
		newHeader->stride = static_cast<SizeType>(totalSize);
		Derived* obj = new (newHeader+1) Derived(std::forward<Args>(args)...);
		m_tail->usedCap += totalSize;
		m_numElements++;

		return *obj;
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
			while(pos < c->usedCap)
			{
				Header* h = reinterpret_cast<Header*>(c->mem + pos);
				T* obj = reinterpret_cast<T*>(h + 1);
				obj->~T();

				pos += h->stride;
			}

			c->usedCap = 0;
			c = c->next;
		}
		m_tail = m_head;
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
		Chunk* c = nullptr;
		size_t pos = 0;

		T& operator*() const
		{
			Header* h = reinterpret_cast<Header*>(c->mem + pos);
			T* obj = reinterpret_cast<T*>(h + 1);
			return *obj;
		}

		Iterator& operator++()
		{
			Header* h = reinterpret_cast<Header*>(c->mem + pos);
			pos += h->stride;
			if (pos >= c->usedCap)
			{
				// Move to next chunk
				c = c->next;
				pos = 0;
			}

			return *this;
		}

		bool operator==(const Iterator& other) const
		{
			return c == other.c && pos == other.pos;
		}

		bool operator!=(const Iterator& other) const
		{
			return !(*this == other);
		}
	};

	Iterator begin()
	{
		return Iterator{m_head, 0};
	}

	Iterator end()
	{
		return Iterator{nullptr, 0};
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
	}

	void getFreeChunk(size_t chunkCapacity)
	{ 
		chunkCapacity = roundUpToMultipleOf(std::max(sizeof(Header) + sizeof(T), chunkCapacity), alignof(T));

		// NOTE:
		// Since we can keep the chunks when clearing the container, it means m_tail is not necessarily the
		// last in the chain.
		if (m_tail && m_tail->next)
		{
			assert(m_tail->next->usedCap == 0);

			if (m_tail->next->cap >= chunkCapacity)
			{
				// If the next chunk is big enough, use it.
				m_tail = m_tail->next;
			}
			else
			{
				// If not, then deallocate it and replace with another one big enough, making sure not to break the chain
				Chunk* n = m_tail->next->next;
				delete m_tail->next;
				m_tail->next = new Chunk(chunkCapacity);
				m_tail->next->next = n; // Restore the link
				m_tail = m_tail->next;
			}
		}
		else
		{
			Chunk* c = new Chunk(chunkCapacity);
			if (m_tail)
			{
				m_tail->next = c;
				m_tail = c;
			}
			else
			{
				m_head = m_tail = c;
			}
		}
	}

	/**
	 * Stored before each element, so we have the information
	 * required to transverse the container.
	 */
	struct alignas(alignof(T)) Header
	{
		// Bytes from this header to the next header
		SizeType stride;
	};

	//
	// Since chunks are not necessarily released, it means that once we clear the container, we keep a chain
	// of chunks that are not in use.
	// Therefore, m_tail points to the chunk we are adding elements to, but not necessarily the last chunk in the chain.
	// The first allocated chunk
	Chunk* m_head = nullptr;
	Chunk* m_tail = nullptr;

	std::size_t m_numElements = 0;
};

} // namespace cz

#if defined(_MSVC_LANG)
__pragma(warning(pop))
#endif

