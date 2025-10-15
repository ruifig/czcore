/********************************************************************
	CrazyGaze (http://www.crazygaze.com)
	Author : Rui Figueira
	Email  : rui@crazygaze.com
	
	purpose:
*********************************************************************/

#include "AsyncCommandQueue.h"
#include "Logging.h"

namespace cz
{

void AsyncCommandQueue::send(std::function<void()>&& f)
{
	CZ_CHECK(f);
	m_queue.push(std::move(f));
}

void AsyncCommandQueue::tickImpl(bool wait)
{
	// Instead of having a possible infinite loop (if we get commands faster than we can process them),
	// I'm checking how many items we have at the start then only consume those.
	size_t todo = m_queue.size();

	// If nothing to do, we wait until there is something
	if (todo==0 && wait)
	{
		std::function<void()> f = m_queue.waitAndPop();
		f();
		todo = m_queue.size();
	}

	while (todo--)
	{
		std::function<void()> f;
		CZ_VERIFY(m_queue.tryAndPop(f));
		f();
	}
}

//////////////////////////////////////////////////////////////////////////
//	AsyncCommandQueueExplicit
//////////////////////////////////////////////////////////////////////////
void AsyncCommandQueueExplicit::tick(bool wait)
{
	tickImpl(wait);
}

//////////////////////////////////////////////////////////////////////////
//	AsyncCommandQueueAutomatic
//////////////////////////////////////////////////////////////////////////
AsyncCommandQueueAutomatic::AsyncCommandQueueAutomatic()
{
	m_thread = std::thread(&AsyncCommandQueueAutomatic::run, this);
}

AsyncCommandQueueAutomatic::~AsyncCommandQueueAutomatic()
{
	m_queue.push( [this]() { m_finish = true; }) ;
	m_thread.join();
}

void AsyncCommandQueueAutomatic::run()
{
	while (!m_finish)
	{
		tickImpl(true);
	}
}

} // namespace cz

