using namespace cz;

#include "crazygaze/core/Windows/Win32Event.h"
#include "crazygaze/core/LogOutputs.h"

#include "TestUtils.h"

using namespace std::literals::chrono_literals;

TEST_CASE("Win32Event", "[Win32Event]")
{
	LogOutputs logs;

	SECTION("isValid")
	{
		Win32Event evt(true, true);
		CHECK(evt.isValid());

		Win32Event other(std::move(evt));
		CHECK(other.isValid());
		CHECK(!evt.isValid());
	}

	SECTION("isSignaled")
	{
		{
			Win32Event e(true, false);
			CHECK(e.isSignaled() == false);
		}

		{
			Win32Event e(true, true);
			CHECK(e.isSignaled() == true);
		}
	}

	SECTION("wait")
	{

		SECTION("infinite")
		{
			Win32Event e(true, false);
			std::atomic<bool> ready = false;
			auto h = std::async(
				std::launch::async, [&]()
				{
					ready = true;
					std::this_thread::sleep_for(100ms);
					e.signal();
				});

			// Wait until the other thread starts running
			while(!ready){}
			bool res = false;
			// Measure how long we wait for the signaled state
			auto delta = measureTimeMs([&]{
				res = e.wait();
			});

			CHECK(res);
			CHECK_THAT(delta,  Catch::Matchers::WithinAbs(100, 20));
		}

		SECTION("with timeout")
		{
			Win32Event e(true, false);
			std::atomic<bool> ready = false;
			auto h = std::async(
				std::launch::async, [&]()
				{
					ready = true;
					std::this_thread::sleep_for(200ms);
					e.signal();
				});

			// Wait until the other thread starts running
			while(!ready){}
			bool res = false;
			// Measure how long we wait for the signaled state
			auto delta = measureTimeMs([&]{
				res = e.wait(100ms);
			});

			// It should wait the specified time and timeout (return false)
			CHECK(res == false);
			CHECK_THAT(delta,  Catch::Matchers::WithinAbs(100, 20));
		}
	}

	SECTION("reset")
	{
		Win32Event e(true, true);
		CHECK(e.isSignaled());
		e.reset();
		CHECK(!e.isSignaled());
	}

}


