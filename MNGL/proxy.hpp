#pragma once
#include <type_traits>

namespace mngl
{
    //proxy для объектов из рода SDL_Rect и др, для сокращения кода
    template<typename F, typename T,
        T(*Convert)(const F&),
        void (*Sync)(F&, const T&)>
    class Proxy
    {
    public:
        explicit Proxy(F& obj) : obj_(obj), temp_(Convert(obj_)) {}

        operator T* () { return &temp_; }
        operator const T* () const { return &temp_; }

        ~Proxy() { Sync(obj_, temp_); }

        Proxy(const Proxy&) = delete;
        Proxy& operator=(const Proxy&) = delete;
    private:
        F& obj_;
        T temp_;
    };
}