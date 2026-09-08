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
	if (firstTrace)
	{
		CZ_CHECK(objBasePtr);
		SharedPtrTracingRegistry::get().internal_remove(objBasePtr);
	}
	#endif
}

#if CZ_SHAREDPTR_STACKTRACES
std::unique_ptr<SharedPtrTrace> ControlBlockDetails::createStackTrace(SharedPtrTrace::Type type)
{
	// If it's the creation trace (aka first trace), then we want to create the TraceList
	if (type == SharedPtrTrace::Type::Creation)
		return std::unique_ptr<SharedPtrTrace>(new SharedPtrTrace(type, std::make_shared<TraceList>()));

	// If we have the first trace, it means we want to capture stack traces
	if (firstTrace)
	{
		ZoneScoped;
		// Using `new` instead of make_unique, so `std::make_unique` doesn't show up in the stacktrace.
		// This makes it easier for tools by allowing them to skip all the frames at the top that start with `cz::`
		return std::unique_ptr<SharedPtrTrace>(new SharedPtrTrace(type, firstTrace->outer));
	}
	else
	{
		return nullptr;
	}
}

SharedPtrTraces ControlBlockDetails::getTraces()
{
	SharedPtrTraces res;

	if (firstTrace)
	{
		firstTrace->outer->visitAll([&res](const SharedPtrTrace* ele)
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

