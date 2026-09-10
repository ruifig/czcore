/********************************************************************
	CrazyGaze (http://www.crazygaze.com)
	Author : Rui Figueira
	Email  : rui@crazygaze.com
	
	purpose:
	Allows adding key:value pair markers to the current callstack.
	This allows for example to check if an instance/function is already being executed
	Very similar and inspired by boost::asio::detail::call_stack,
	with the addition that we can iterate the call stack with a range based for
	Implementation based on http://www.crazygaze.com/blog/2016/03/11/callstack-markers-boostasiodetailcall_stack/
*********************************************************************/

#pragma once

namespace cz
{

template<typename Key, typename Value = unsigned char>
class CallstackMarker
{
public:
	class Iterator;

	class Context
	{
	public:
		Context(const Context&) = delete;
		Context& operator=(const Context&) = delete;
		explicit Context(Key* k)
			: m_key(k)
			, m_next(CallstackMarker<Key,Value>::ms_data.top)
		{
			m_val = reinterpret_cast<unsigned char*>(this);
			CallstackMarker<Key, Value>::ms_data.top = this;
		}

		Context(Key* k, Value& v)
			: m_key(k)
			, m_val(&v)
			, m_next(CallstackMarker<Key, Value>::ms_data.top)
		{
			CallstackMarker<Key, Value>::ms_data.top = this;
		}

		~Context()
		{
			CallstackMarker<Key,Value>::ms_data.top = m_next;
		}

		Key* getKey()
		{
			return m_key;
		}

		Value* getValue()
		{
			return m_val;
		}
	private:
		friend class CallstackMarker<Key, Value>;
		friend class CallstackMarker<Key, Value>::Iterator;
		Key* m_key;
		Value* m_val;
		Context* m_next;
	};

	class Iterator
	{
	public:
		Iterator(Context* ctx) : m_ctx(ctx) {}
		Iterator& operator++()
		{
			if (m_ctx)
				m_ctx = m_ctx->m_next;
			return *this;
		}

		bool operator != (const Iterator& other)
		{
			return m_ctx != other.m_ctx;
		}

		Context* operator*()
		{
			return m_ctx;
		}

	private:
		Context* m_ctx;
	};

	/**
	 * Typically, an application will use scoped Context instances, but in some scenarios it
	 * requires inserting a context from one function and popping it from another. Those
	 * cases can't be solved with a scoped context. 
	 * This function allows explicitly push/pop of heap allocated contexts
	 */
	void oob_push(Key* k)
	{
		ms_data.oob.emplace_back(new Context(k));
	}
	void oob_push(Key* k, Value& v)
	{
		ms_data.oob.emplace_back(new Context(k, v));
	}

	/**
	 * Pops an out of band context.
	 * The reason it returns the popped context is so that the caller can verify context stack correctness
	 * if appropriate.
	 */ 
	std::unique_ptr<Context> oob_pop()
	{
		CZ_CHECK(ms_data.oob.size());
		auto ctx = std::move(ms_data.oob.back());
		ms_data.oob.pop_back();
		return ctx;
	}

	/**
	 * Determine if the specified owner is on the stack
	 *
	 * @return The address of the value if present, nullptr if not present
	 */
	static Value* contains(const Key* k)
	{
		Context* elem = ms_data.top;
		while(elem)
		{
			if (elem->m_key == k)
				return elem->m_val;
			elem = elem->m_next;
		}
		return nullptr;
	}

	static Iterator begin()
	{
		return Iterator(ms_data.top);
	}

	static Iterator end()
	{
		return Iterator(nullptr);
	}

private:
	static inline thread_local struct
	{
		Context* top = nullptr;

		// Typically, an application will use scoped Context instances, but in some scenarios,
		// an application might want to insert a context from one function, then pop it from another.
		// This vector allows that
		std::vector<std::unique_ptr<Context>> oob;
	} ms_data;
};

} // namespace cz

