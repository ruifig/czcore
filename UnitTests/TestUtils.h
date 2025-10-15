#pragma once

template<
	typename H,
	typename = std::enable_if_t< std::is_same_v<void, typename std::invoke_result<H>::type>> 
>
float measureTimeMs(H&& fn)
{
	auto start = std::chrono::high_resolution_clock::now();
	fn();
	float deltaMs = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - start).count();
	return deltaMs;
}

template<
	typename H,
	typename = std::enable_if_t< !std::is_same_v<void, typename std::invoke_result<H>::type>> 
>
std::pair<float, typename std::invoke_result<H>::type> measureTimeMs(H&& fn)
{
	auto start = std::chrono::high_resolution_clock::now();
	auto res = fn();
	float deltaMs = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - start).count();
	return {deltaMs, std::move(res)};
}

template<typename Duration>
float toMs(Duration d)
{
	return std::chrono::duration<float, std::milli>(d).count();
}

