#pragma once

#include "logger.hpp"

// Compatibility names; new code uses snake_case.
using Logger = mngl::logger;
template <typename T>
concept Streamable = mngl::streamable<T>;
inline auto& logger = mngl::log;
