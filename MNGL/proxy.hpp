#pragma once
#include <type_traits>

namespace mngl
{
	enum class proxy_mode
	{
		input,
		output,
		inout
	};

	// proxy для объектов из рода SDL_Rect и др, для сокращения кода
	template <typename F, typename T, T (*Convert)(const F&), void (*Sync)(F&, const T&)> class proxy
	{
	public:
		explicit proxy(F& obj) : proxy(obj, proxy_mode::inout) {}
		proxy(F& obj, proxy_mode mode)
		    : obj_(obj), temp_(mode == proxy_mode::output ? T{} : Convert(obj_)),
		      sync_(mode != proxy_mode::input)
		{
		}

		operator T*() { return &temp_; }
		operator const T*() const { return &temp_; }

		~proxy()
		{
			if (sync_)
				Sync(obj_, temp_);
		}

		proxy(const proxy&) = delete;
		proxy& operator=(const proxy&) = delete;

	private:
		F& obj_;
		T temp_;
		bool sync_;
	};
} // namespace mngl
namespace mngl
{
	template <typename F, typename T, T (*Convert)(const F&), void (*Sync)(F&, const T&)>
	using Proxy = proxy<F, T, Convert, Sync>;
}
