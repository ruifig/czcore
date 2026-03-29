#pragma once

#include "crazygaze/core/Common.h"
#include "crazygaze/core/PlatformUtils.h"
#include "crazygaze/core/Logging.h"

namespace cz
{

namespace details
{

	struct Win32Handle_NULL
	{
		static constexpr void* value = NULL;
	};

	struct Win32Handle_ERROR_INVALID_HANDLE
	{
		static constexpr void* value = (void*) ERROR_INVALID_HANDLE;
	};

	/**
	 * Generic HANDLE wrapper.
	 * This is not supposed to be used directly, due to all the small differences between HANDLE types.
	 * For example, The CreateFile API returns INVALID_HANDLE_VALUE on failure while CreateEvent returns NULL.
	 * 
	 * Specific types should inherit from this and use the methods they need to follow Microsoft's API contract for the
	 * function in question.
	 */
	template<typename InvalidT>
	class Win32Handle
	{
	  protected:
		using Invalid = InvalidT;

		Win32Handle() = default;
		~Win32Handle()
		{
			close();
		}

		// For simplicity, we don't allow handle duplications.
		// Some handle types allow duplication, but I'm opting for not allowing this until proven otherwise
		Win32Handle(const Win32Handle&) = delete;
		Win32Handle& operator=(const Win32Handle&&) = delete;

		Win32Handle(Win32Handle&& other)
		{
			m_handle = other.m_handle;
			other.m_handle = Invalid::value;
		}

	  public:

		Win32Handle& operator=(Win32Handle&& other)
		{
			if (this != other)
			{
				close();
				m_handle = other.m_handle;
				other.m_handle = Invalid::value;
			}

			return *this;
		}

		bool isValid() const
		{
			return m_handle != Invalid::value;
		}

	  protected:

		/**
		 * @important
		 * Intentionally NOT implementing operator bool(), to avoid confusion between these two concepts:
		 * - Is the handle valid ?
		 * - Is the handle signaled ?
		 *
		 * The user should use the explicit isValid() or isSignaled() methods to avoid confusion.
		 */
		operator bool() const;
		
		/**
		 * Makes use of the WaitForSingleObject API
		 * 
		 * @param timeoutMs
		 *		Timeout value in milliseconds (the same as in the Win32 WaitForSingleObject function).
		 *		`0` : Just checks if the object is signaled and return immediately.
		 *		`>0` : Timeout in milliseconds.
		 *		`INFINITE` : No Timeout
		 * @return
		 *		`true`  : If the object is signaled.
		 *		`false` : Timed out (timeout>0), object not signaled (timeout=0), or handle is invalid.
		 */
		bool waitImpl(DWORD timeoutMs) const
		{
			if (!isValid())
			{
				return false;
			}

			auto res = WaitForSingleObject(m_handle, timeoutMs);
			if (res == WAIT_OBJECT_0)
			{
				return true;
			}
			else if (res == WAIT_TIMEOUT)
			{
				return false;
			}
			else
			{
				CZ_LOG(Main, Error, "{}", getWin32Error("WaitForSingleObject"));
				return false;
			}
		}

		/**
		 * Returns true if the object is in the signaled state.
		 */
		bool isSignaled() const
		{
			return waitImpl(0);
		}

		/**
		 * Blocks until the object is signaled.
		 *
		 * @return `true` if the object is signaled. `false` for any other scenarios.
		 */
		bool wait() const
		{
			return waitImpl(INFINITE);
		}

		/**
		 * Waits until the object is signaled or the specified timeout elapses.
		 *
		 * @return `true` if the object is signaled. `false` if it timed out or an error occurred.
		 */
		bool wait(std::chrono::milliseconds timeout)
		{
			return waitImpl(static_cast<DWORD>(timeout.count()));
		}

		void close()
		{
			if (m_handle != Invalid::value)
			{
				if (CloseHandle(m_handle) == 0)
				{
					CZ_LOG(Main, Error, "{}", getWin32Error("CloseHandle"));
				}

				m_handle = Invalid::value;
			}
		}

		HANDLE m_handle = Invalid::value;
	};
}


/**
 * Wrapper for a Win32 Event object.
 */
class Win32Event : public details::Win32Handle<details::Win32Handle_NULL>
{
  public:

	/**
	 * @param manualReset
	 *		The `bManualReset` parameter to the CreateEvent API.
	 *		If `true` a call to `reset()` is needed to set the event to non-signaled state. if `false`, the event is created
	 *		as an auto-reset event, and the system will automatically reset the event state to nonsignaled after a single
	 *		thread has been released.
	 *
	 * @param initialState
	 *		Initial state of the event (`true` means signaled. `false` means nonsignaled.
	 */
	Win32Event(bool manualReset, bool initialState);

	// Expose some of the methods that are applicable to a Win32 Event
	using Win32Handle::isValid;
	using Win32Handle::isSignaled;
	using Win32Handle::wait;

	/**
	 * Wrapper for the SetEvent API. Sets the event to signaled state.
	 */
	void signal();

	/**
	 * Wrapper for the ResetEvent API. Sets the event to nonsignaled state.
	 * This is only required when `manualReset` was specified in the constructor.
	 */
	void reset();

};


}

