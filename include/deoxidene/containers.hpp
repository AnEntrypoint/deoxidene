#pragma once

#include <cstddef>

#include <boost/container/small_vector.hpp>

namespace deoxidene {

template <typename T, std::size_t N>
using SmallVec = boost::container::small_vector<T, N>;

}  // namespace deoxidene
