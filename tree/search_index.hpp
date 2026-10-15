#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

template <class T>
class search_index
{
public:
    using value_type = T;
    using visitor_type = std::function<void(const T&)>;

    virtual ~search_index() = default;

    virtual bool insert(T value) = 0;
    virtual bool erase(const T& value) = 0;
    virtual bool contains(const T& value)
    {
        return lookup(value) != nullptr;
    }

    virtual bool contains(const T& value) const
    {
        return lookup(value) != nullptr;
    }

    // Указатели заимствованы и могут стать недействительными после любой мутации.
    virtual const T* lookup(const T& value) = 0;
    virtual const T* lookup(const T& value) const = 0;
    virtual const T* lower_bound(const T& value) const = 0;
    virtual void for_each(const visitor_type& visitor) const = 0;

    // Обходит значения в порядке компаратора на полуинтервале [lower, upper).
    virtual void for_each_range(
        const T& lower,
        const T& upper,
        const visitor_type& visitor) const = 0;
    virtual std::string to_string() const = 0;
    virtual std::vector<std::uint8_t> to_binary() const = 0;
};
