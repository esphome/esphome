#pragma once

#include "esphome/core/helpers.h"

namespace esphome::custom_i2c {

/**
 * Return a copy of the specified std::array with zero-initialized elements added to the left to grow it to `to`
 * entries.
 *
 * For example:
 *
 * ```cpp
 * grow_left<3, 5>({ 1, 2, 3 }) // returns { 0, 0, 1, 2, 3 }
 * grow_left<3, 3>({ 1, 2, 3 }) // returns { 1, 2, 3 }
 * grow_left<3, 2>({ 1, 2, 3 }) // compile-time error: `to` must be greater than or equal to `from`
 * ```
 */
template<size_t from, size_t to, typename T = uint8_t> std::array<T, to> grow_left(std::array<T, from> in) {
  static_assert(to >= from, "Attempting to use grow_left to shrink an array; use shrink_left or resize_left instead");

  std::array<T, to> out{};
  memcpy(out.data() + (to - from), in.data(), from);
  return out;
}

/**
 * Return a copy of the specified std::array with zero-initialized elements added to the right to grow it to `to`
 * entries.
 *
 * For example:
 *
 * ```cpp
 * grow_right<3, 5>({ 1, 2, 3 }) // returns { 1, 2, 3, 0, 0 }
 * grow_right<3, 3>({ 1, 2, 3 }) // returns { 1, 2, 3 }
 * grow_right<3, 2>({ 1, 2, 3 }) // compile-time error: `to` must be greater than or equal to `from`
 * ```
 */
template<size_t from, size_t to, typename T = uint8_t> std::array<T, to> grow_right(std::array<T, from> in) {
  static_assert(to >= from,
                "Attempting to use grow_right to shrink an array; use shrink_right or resize_right instead");

  std::array<T, to> out{};
  memcpy(out.data(), in.data(), from);
  return out;
}

/**
 * Return a copy of the specified std::array with elements removed from the left to shrink it to `to` entries.
 *
 * For example:
 *
 * ```cpp
 * shrink_left<5, 3>({ 1, 2, 3, 4, 5 }) // returns { 3, 4, 5 }
 * shrink_left<5, 5>({ 1, 2, 3, 4, 5 }) // returns { 1, 2, 3, 4, 5 }
 * shrink_left<5, 6>({ 1, 2, 3, 4, 5 }) // compile-time error: `to` must be less than or equal to `from`
 * ```
 */
template<size_t from, size_t to, typename T = uint8_t> std::array<T, to> shrink_left(std::array<T, from> in) {
  static_assert(to <= from, "Attempting to use shrink_left to grow an array; use grow_left or resize_left instead");

  std::array<T, to> out;  // no need to zero-initialize when shrinking
  memcpy(out.data(), in.data() + (from - to), to);
  return out;
}

/**
 * Return a copy of the specified std::array with elements removed from the right to shrink it to `to` entries.
 *
 * For example:
 *
 * ```cpp
 * shrink_right<5, 3>({ 1, 2, 3, 4, 5 }) // returns { 1, 2, 3 }
 * shrink_right<5, 5>({ 1, 2, 3, 4, 5 }) // returns { 1, 2, 3, 4, 5 }
 * shrink_right<5, 6>({ 1, 2, 3, 4, 5 }) // compile-time error: `to` must be less than or equal to `from`
 * ```
 */
template<size_t from, size_t to, typename T = uint8_t> std::array<T, to> shrink_right(std::array<T, from> in) {
  static_assert(to <= from, "Attempting to use shrink_right to grow an array; use grow_right or resize_right instead");

  std::array<T, to> out;  // no need to zero-initialize when shrinking
  memcpy(out.data(), in.data(), to);
  return out;
}

/**
 * Return a resized copy of the specified std::array, adding or removing elements from the left side of the array as
 * necessary.
 *
 * If the array is being shrunk, elements will be removed from the left, and only the rightmost `to` bytes will be
 * preserved. If the arary is being expanded, zero-initialized elements will be added on the left to pad out the array
 * to a total of `to` bytes.
 *
 * For example:
 *
 * ```cpp
 * resize_left<5, 3>({ 1, 2, 3, 4, 5 }) // returns { 3, 4, 5 }
 * resize_left<3, 5>({ 1, 2, 3 }) // returns { 0, 0, 1, 2, 3 }
 * ```
 */
template<size_t from, size_t to, typename T = uint8_t, enable_if_t<(from < to), bool> = true>
std::array<T, to> resize_left(std::array<T, from> in) {
  return grow_left<from, to, T>(in);
}
template<size_t from, size_t to, typename T = uint8_t, enable_if_t<(from >= to), bool> = true>
std::array<T, to> resize_left(std::array<T, from> in) {
  return shrink_left<from, to, T>(in);
}

/**
 * Return a resized copy of the specified std::array, adding or removing elements from the right side of the array as
 * necessary.
 *
 * If the array is being shrunk, elements will be removed from the right, and only the leftmost `to` bytes will be
 * preserved. If the arary is being expanded, zero-initialized elements will be added on the right to pad out the array
 * to a total of `to` bytes.
 *
 * For example:
 *
 * ```cpp
 * resize_right<5, 3>({ 1, 2, 3, 4, 5 }) // returns { 1, 2, 3 }
 * resize_right<3, 5>({ 1, 2, 3 }) // returns { 1, 2, 3, 0, 0 }
 * ```
 */
template<size_t from, size_t to, typename T = uint8_t, enable_if_t<(from < to), bool> = true>
std::array<T, to> resize_right(std::array<T, from> in) {
  return grow_right<from, to, T>(in);
}
template<size_t from, size_t to, typename T = uint8_t, enable_if_t<(from >= to), bool> = true>
std::array<T, to> resize_right(std::array<T, from> in) {
  return shrink_right<from, to, T>(in);
}

}  // namespace esphome::custom_i2c
