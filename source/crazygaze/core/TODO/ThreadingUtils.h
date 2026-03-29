#pragma once

namespace cz
{

/*!
 * Based on Herb Sutters's Monitor<T> class.
 * It protects all access to an object
 */
template <class T>
class Monitor
{
private:
	mutable T m_t;
	mutable std::mutex m_mtx;

public:
	using Type = T;
	Monitor() {}
	Monitor(T t_) : m_t(std::move(t_)) {}
	template <typename F>
	auto operator()(F f) const -> decltype(f(m_t))
	{
		std::lock_guard<std::mutex> hold{ m_mtx };
		return f(m_t);
	}
};

} // namespace cz


