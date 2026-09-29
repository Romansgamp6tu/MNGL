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

    //proxy для объектов из рода SDL_Rect и др, для сокращения кода
    template<typename F, typename T,
        T(*Convert)(const F&),
        void (*Sync)(F&, const T&)>
    class Proxy
    {
    public:
        explicit Proxy(F& obj) : Proxy(obj, proxy_mode::inout) {}
        Proxy(F& obj, proxy_mode mode)
            : obj_(obj),
              temp_(mode == proxy_mode::output ? T{} : Convert(obj_)),
              sync_(mode != proxy_mode::input)
        {}

        operator T* () { return &temp_; }
        operator const T* () const { return &temp_; }

        ~Proxy()
        {
            if (sync_) Sync(obj_, temp_);
        }

        Proxy(const Proxy&) = delete;
        Proxy& operator=(const Proxy&) = delete;
    private:
        F& obj_;
        T temp_;
        bool sync_;
    };
}