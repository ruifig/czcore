#pragma once

#include "Common.h"

namespace cz
{

//
// Multiple producer, multiple consumer thread safe queue
//
template<typename T>
class SharedQueue
{
private:
	std::queue<T> m_queue;
	mutable std::mutex m_mtx;
	std::condition_variable m_data_cond;

	SharedQueue& operator=(const SharedQueue&) = delete;
	SharedQueue(const SharedQueue& other) = delete;

public:
	SharedQueue(){}


	/**
	 * Queues an new item, constructing it in-place.
	 * 
	 * @returns
	 *	The number of items in the queue (including the new one), at the time the new item was inserted.
	 *	This is useful for scenario where the producer needs to kick start the consumer because the producer is only running
	 *  when there are items in the queue.
	 */
	template<typename... Args>
	size_t emplace(Args&&... args)
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		m_queue.emplace(std::forward<Args>(args)...);
		auto size = m_queue.size();
		m_data_cond.notify_one();
		return size;
	}

	/**
	 * Queues an new item
	 * 
	 * @returns
	 *	The number of items in the queue (including the new one), at the time the new item was inserted.
	 *	This is useful for scenario where the producer needs to kick start the consumer because the producer is only running
	 *  when there are items in the queue.
	 */
	template<typename Arg>
	size_t push(Arg&& item){
		std::lock_guard<std::mutex> lock(m_mtx);
		m_queue.push(std::forward<Arg>(item));
		auto size = m_queue.size();
		m_data_cond.notify_one();
		return size;
	}

	/**
	 * Tries to pop an item from the queue. It does not block waiting for items.
	 * @return Returns true if an Items was retrieved
	 */
	bool tryAndPop(T& poppedItem){
		std::lock_guard<std::mutex> lock(m_mtx);
		if (m_queue.empty()){
			return false;
		}
		poppedItem = std::move(m_queue.front());
		m_queue.pop();
		return true;
	}

	/**
	 * Gets a pointer to the item at the front of the queue or null if the queue is empty.
	 * Note that this doesn't remove the item from the queue, so it should not be used by design with multiple producers.
	 * One scenario this is useful is when a consumer only wants to remove items when the work for that item is complete, and NOT
	 * when it starts working on that item. This scenario only works for single consumer approaches.
	 */
	T* peek()
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		if (m_queue.empty()){
			return nullptr;
		}
		return &m_queue.front();
	}

	/**
	 * Retrieves all items into the supplied queue.
	 * This should be more efficient than retrieving one item at a time when a thread wants to process as many items as there
	 * are currently in the queue. Example:
	 * std::queue<Foo> local;
	 * if (q.popAll(local)) {
	 *     ... process items in local ...
	 * }
	 *
	 * @param dest Queue used as destination queue.
	 * @return True if the destination queue has any items, either because they were pushed or because the queue already had items
	 */
	bool popAll(std::queue<T>& dest)
	{
		std::lock_guard<std::mutex> lock(m_mtx);

		if (dest.size())
		{
			while(m_queue.size())
			{
				dest.push(std::move(m_queue.front()));
				m_queue.pop();
			}
		}
		else
		{
			std::swap(dest, m_queue);
		}

		return dest.size()!=0;
	}

	/**
	 * Retrieves an item, blocking if necessary to wait for items.
	 *
	 * @return The item popped from the queue
	 */
	T waitAndPop()
	{
		std::unique_lock<std::mutex> lock(m_mtx);
		m_data_cond.wait(lock, [this] { return !m_queue.empty();});
		T item = std::move(m_queue.front());
		m_queue.pop();
		return item;
	}

	/**
	 * Retrieves an item, blocking if necessary for the specified duration until items arrive
	 *
	 * @param poppedItem Where to receive the popped item if the function returned true
	 * @param timeoutMs Time to wait in milliseconds for an item to arrive
	 *
	 * @return True if an item was popped (poppedItem will contain the item), or false if the operation timed out
	 *
	 */
	bool waitAndPop(T& poppedItem, int64_t timeoutMs){
		std::unique_lock<std::mutex> lock(m_mtx);
		if (!m_data_cond.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] { return !m_queue.empty();}))
			return false;

		poppedItem = std::move(m_queue.front());
		m_queue.pop();
		return true;
	}

	/** Checks if the queue is empty */
	bool empty() const{
		std::lock_guard<std::mutex> lock(m_mtx);
		return m_queue.empty();
	}

	/**
	 * Returns how many items there are in the queue.
	 * Be careful how you use this. If you need to know the size of the queue after inserting an item, then the return value of
	 * emplace() and push().
	 * 
	 * This is because if you queue an item and check the size of the queue in separate steps, a consumer or another producer can
	 * add or remove items from the queue in between both calls.
	 */
	unsigned size() const{
		std::lock_guard<std::mutex> lock(m_mtx);
		return static_cast<unsigned>(m_queue.size());
	}
};
using WorkQueue = SharedQueue<std::function<void()>>;

} // namespace cz

