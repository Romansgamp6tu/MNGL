#pragma once

#include <memory>
#include <type_traits>
#include <utility>

template <class Meta>
class vertex_metadata
{
public:
    vertex_metadata() = default;

    explicit vertex_metadata(Meta metadata)
        : metadata_(std::move(metadata))
    {
    }

    Meta& metadata() noexcept
    {
        return metadata_;
    }

    const Meta& metadata() const noexcept
    {
        return metadata_;
    }

private:
    Meta metadata_{};
};

template <>
class vertex_metadata<void>
{
};

template <class T, class Meta = void>
class vertex : private vertex_metadata<Meta>
{
public:
    using value_type = T;
    using metadata_type = Meta;
    using pointer = std::unique_ptr<vertex>;

    explicit vertex(T value)
        : value_(std::move(value))
    {
    }

    template <class M = Meta>
        requires (!std::is_void_v<M>)
    vertex(T value, M metadata)
        : vertex_metadata<Meta>(std::move(metadata))
        , value_(std::move(value))
    {
    }

    vertex(const vertex&) = delete;
    vertex& operator=(const vertex&) = delete;
    vertex(vertex&&) = delete;
    vertex& operator=(vertex&&) = delete;
    ~vertex() = default;

    T& value() noexcept
    {
        return value_;
    }

    const T& value() const noexcept
    {
        return value_;
    }

    template <class M = Meta>
        requires (!std::is_void_v<M>)
    M& metadata() noexcept
    {
        return vertex_metadata<Meta>::metadata();
    }

    template <class M = Meta>
        requires (!std::is_void_v<M>)
    const M& metadata() const noexcept
    {
        return vertex_metadata<Meta>::metadata();
    }

    vertex* left() noexcept
    {
        return left_.get();
    }

    const vertex* left() const noexcept
    {
        return left_.get();
    }

    vertex* right() noexcept
    {
        return right_.get();
    }

    const vertex* right() const noexcept
    {
        return right_.get();
    }

    vertex* parent() noexcept
    {
        return parent_;
    }

    const vertex* parent() const noexcept
    {
        return parent_;
    }

    bool is_leaf() const noexcept
    {
        return !left_ && !right_;
    }

    void set_left(pointer child) noexcept
    {
        if (child)
        {
            child->parent_ = this;
        }

        left_ = std::move(child);
    }

    void set_right(pointer child) noexcept
    {
        if (child)
        {
            child->parent_ = this;
        }

        right_ = std::move(child);
    }

    pointer detach_left() noexcept
    {
        return detach_child(left_);
    }

    pointer detach_right() noexcept
    {
        return detach_child(right_);
    }

    void set_parent(vertex* parent) noexcept
    {
        parent_ = parent;
    }

private:
    static pointer detach_child(pointer& child) noexcept
    {
        pointer detached = std::move(child);

        if (detached)
        {
            detached->parent_ = nullptr;
        }

        return detached;
    }

    T value_;
    pointer left_;
    pointer right_;
    vertex* parent_ = nullptr;
};
