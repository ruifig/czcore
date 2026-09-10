#include "SmartPtrsHelper.h"

//////////////////////////////////////////////////////////////////////////
// SharedPtrTracingRegistry
//////////////////////////////////////////////////////////////////////////

namespace cz
{

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


namespace details
{

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
std::unique_ptr<SharedPtrTrace> ControlBlockDetails::createStackTrace(SharedPtrTrace::Type type)
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

		// Create the 
		if (!data.traceList)
			data.traceList = std::make_shared<TraceList>();

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
			if (ele->type == SharedPtrTrace::Type::Creation)
				res.creationTrace = std::move(entry);
			else if (ele->type == SharedPtrTrace::Type::StrongRef)
				res.strong.emplace_back(std::move(entry));
			else if (ele->type == SharedPtrTrace::Type::WeakRef)
				res.weak.emplace_back(std::move(entry));
			else
			{
				CZ_CHECK(false);
			}
		});
	}

	return res;
}
#endif

} // namespace details
} // namespace cz

