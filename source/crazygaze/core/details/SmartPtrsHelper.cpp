#include "SmartPtrsHelper.h"

namespace cz
{

//////////////////////////////////////////////////////////////////////////
// SharedPtrTracingRegistry
//////////////////////////////////////////////////////////////////////////

#if CZ_SHAREDPTR_STACKTRACES

void SharedPtrTracingRegistry::internal_add(void* objBasePtr, details::ControlBlockDetails* ctrlBlock)
{
	auto lk = std::lock_guard(m_mtx);
	m_c.try_emplace(objBasePtr, Info{m_idCounter++, ctrlBlock, nullptr});
}

void SharedPtrTracingRegistry::internal_remove(void* objBasePtr)
{
	auto lk = std::lock_guard(m_mtx);
	m_c.erase(objBasePtr);
}

std::pair<bool, void*> SharedPtrTracingRegistry::getTag(void* objBasePtr)
{
	auto lk = std::lock_guard(m_mtx);
	auto it = m_c.find(objBasePtr);
	if (it == m_c.end())
		return {false, nullptr};
	else
		return {true, it->second.tag};
}

#endif


namespace details
{

//////////////////////////////////////////////////////////////////////////
// TraceList
//////////////////////////////////////////////////////////////////////////

TraceList::TraceList(uint32_t maxHistorySize)
	: m_maxHistorySize(maxHistorySize)
{
}

TraceList::~TraceList()
{
	// If this triggered, we probably have a bug somewhere where we forgot to remove entries.
	assert(m_list.empty());
}

void TraceList::add(struct SharedPtrTrace* trace)
{
	m_lock.lock();
	m_list.push_back(trace);
	m_lock.unlock();
}

void TraceList::remove(struct SharedPtrTrace* trace)
{
	m_lock.lock();
	cz::removeFirst(m_list, trace);

	if (m_maxHistorySize)
	{
		if (m_history.size() == m_maxHistorySize)
			m_history.pop_front();

		DeletedPtrTrace& e = m_history.emplace_back();
		e.traceInfo.type = trace->type;
		e.traceInfo.ts = trace->timestamp;
		e.traceInfo.frame = trace->frame;
		e.traceInfo.trace = std::move(trace->trace);
		// Information about when the deletion happened (right now)
		e.deletionInfo.ts = std::chrono::high_resolution_clock::now();
		e.deletionInfo.frame = gFrameCounter.load();
	}

	m_lock.unlock();
}

bool TraceList::isEmpty()
{
	m_lock.lock();
	bool res = m_list.empty();
	m_lock.unlock();
	return res;
}

std::pair<const SharedPtrTrace*, uint32_t> TraceList::getFirstAndCount()
{
	m_lock.lock();
	SharedPtrTrace* res = m_list.empty() ? nullptr : m_list.front();
	uint32_t size = static_cast<uint32_t>(m_list.size());
	m_lock.unlock();
	return {res, size};
}

void TraceList::setMaxHistorySize(uint32_t maxHistorySize)
{
	m_lock.lock();

	while(m_history.size() > maxHistorySize)
		m_history.pop_front();
	m_maxHistorySize = maxHistorySize;

	m_lock.unlock();
}

//////////////////////////////////////////////////////////////////////////
// ControlBlockDetails
//////////////////////////////////////////////////////////////////////////

ControlBlockDetails::~ControlBlockDetails()
{
	#if CZ_SHAREDPTR_STACKTRACES
	CZ_CHECK(m_objBasePtr);
	m_tracing([this](TracingData& data)
	{
		if (data.totalLifetimeTraces)
			SharedPtrTracingRegistry::get().internal_remove(m_objBasePtr);
	});
	#endif
}

#if CZ_SHAREDPTR_STACKTRACES

/**
 * Creates a stack trace if tracing is enabled.
 * If not enabled, it returns nullptr.
 *
 * The "is tracing enabled" check is done in this function instead of on the caller side
 * so that we we can simplify the caller side.
 */
std::unique_ptr<SharedPtrTrace> ControlBlockDetails::createStackTrace(SharedPtrTraceType type)
{
	ZoneScoped;

	// We only lock for the time we need to figure out things.
	// The stack trace is then created outside the lock
	std::shared_ptr<TraceList> dstList = m_tracing([this](TracingData& data) -> std::shared_ptr<TraceList>
	{
		// Tracing is not enabled, then don't create a trace
		if (!data.enabled)
			return nullptr;

		// If it's the first ever trace, then add to the tracing registry
		if (data.totalLifetimeTraces == 0)
			SharedPtrTracingRegistry::get().internal_add(m_objBasePtr, this);

		data.totalLifetimeTraces++;

		// Create the trace list if it doesn't exist
		if (!data.traceList)
			data.traceList = std::make_shared<TraceList>(data.defaultMaxHistorySize);

		return data.traceList;
	});

	if (dstList)
	{
		// Using `new` instead of make_unique, so `std::make_unique` doesn't show up in the stacktrace.
		// This makes it easier for tools by allowing them to skip all the frames at the top that start with `cz::`
		return std::unique_ptr<SharedPtrTrace>(new SharedPtrTrace(type, std::move(dstList)));
	}
	else
	{
		return nullptr;
	}
}

SharedPtrTraces ControlBlockDetails::getTraces()
{
	SharedPtrTraces res;

	std::shared_ptr<TraceList> traceList = m_tracing([](TracingData& data)
	{
		return data.traceList;
	});

	if (traceList)
	{
		traceList->visitAll([&res](const SharedPtrTrace* ele)
		{
			SharedPtrTraces::Entry entry{ele->timestamp, ele->frame, ele->trace};
			if (ele->type == SharedPtrTraceType::Creation)
				res.creationTrace = std::move(entry);
			else if (ele->type == SharedPtrTraceType::StrongRef)
				res.strong.emplace_back(std::move(entry));
			else if (ele->type == SharedPtrTraceType::WeakRef)
				res.weak.emplace_back(std::move(entry));
			else
			{
				CZ_CHECK(false);
			}
		});

		traceList->visitHistory([&res](const DeletedPtrTrace& ele)
		{
			res.history.push_back(ele);
		});
	}

	return res;
}

uint64_t ControlBlockDetails::getTotalLifetimeTraces()
{
	return m_tracing([](TracingData& data) -> uint64_t
	{
		return data.totalLifetimeTraces;
	});
}

#endif

} // namespace details
} // namespace cz

