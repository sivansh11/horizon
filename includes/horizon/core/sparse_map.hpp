#ifndef SPARSE_MAP_HPP
#define SPARSE_MAP_HPP

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <vector>

template <size_t page_size = 1024> //
struct sparse_map {
  using page_t = std::array<size_t, page_size>;
  static constexpr size_t invalid_index = std::numeric_limits<size_t>::max();

  sparse_map(size_t component_size,
             std::function<void(uint8_t *)> destroy_callback,
             std::function<void(uint8_t *, uint8_t *)> move_construct_callback,
             std::function<void(uint8_t *)> construct_callback)
      // align component size
      : _component_size(component_size), _destroy_callback(destroy_callback),
        _move_construct_callback(move_construct_callback),
        _construct_callback(construct_callback) {
    assert(_destroy_callback);
    assert(_move_construct_callback);
    assert(_construct_callback);
  }
  ~sparse_map() {
    for (auto page : _sparse)
      if (page)
        delete page;
  }

  // returns nullptr if key not in map
  uint8_t *at(const uint64_t key) {
    const size_t page_index = key / page_size;
    const size_t page_offset = key % page_size;
    if (page_index >=
            _sparse.size() || // page_index is more than pages in sparse
        !_sparse[page_index]) // if the page in sparse is not allcoated
      return nullptr;
    size_t dense_index = (*_sparse[page_index])[page_offset];
    if (dense_index == invalid_index)
      return nullptr;
    return &_dense[dense_index * _component_size];
  }

  // should not fail
  uint8_t *insert(const uint64_t key) {
    const size_t page_index = key / page_size;
    const size_t page_offset = key % page_size;
    if (page_index >= _sparse.size()) {
      _sparse.resize(page_index + 1, nullptr);
      _occupancy.resize(page_index + 1, 0);
    }
    if (!_sparse[page_index]) {
      page_t *page = _sparse[page_index] = new page_t();
      page->fill(invalid_index);
    }
    size_t dense_index = (*_sparse[page_index])[page_offset];
    if (dense_index != invalid_index) { // already exists in map
      return &_dense[dense_index * _component_size];
    }
    _occupancy[page_index]++;
    dense_index = _dense_index_to_key.size();
    (*_sparse[page_index])[page_offset] = dense_index;
    _dense_index_to_key.emplace_back(key);

    const size_t old_element_count = _dense_index_to_key.size() - 1;
    const size_t old_size = old_element_count * _component_size;
    const size_t new_size = old_size + _component_size;
    const size_t old_capacity = _dense.capacity();
    // move all the old data to a new vector
    if (old_capacity < new_size) {
      // TODO: maybe use golden ratio for increasing capacity
      size_t new_capacity =
          old_capacity == 0 ? _component_size : old_capacity * 2;
      if (new_capacity < new_size)
        new_capacity = new_size;
      std::vector<uint8_t> dense;
      dense.reserve(new_capacity);
      dense.resize(old_size);
      for (size_t i = 0; i < old_size; i += _component_size) {
        uint8_t *new_ptr = dense.data() + i;
        _move_construct_callback(new_ptr, &_dense[i]);
        _destroy_callback(&_dense[i]);
      }
      _dense.swap(dense);
      _dense.resize(new_size);
    } else {
      _dense.resize(new_size);
    }

    uint8_t *ptr = &_dense[old_size];
    _construct_callback(ptr);
    return ptr;
  }

  // should not fail
  void erase(const uint64_t key) {
    const size_t page_index = key / page_size;
    const size_t page_offset = key % page_size;
    if (page_index >=
            _sparse.size() || // page_index is more than pages in sparse
        !_sparse[page_index]) // if the page in sparse is not allcoated
      return;
    size_t &dense_index_ref = (*_sparse[page_index])[page_offset];
    if (dense_index_ref == invalid_index)
      return;
    const size_t index_to_remove = dense_index_ref;
    const uint64_t last_key = _dense_index_to_key.back();
    const size_t last_dense_index = _dense_index_to_key.size() - 1;
    _destroy_callback(&_dense[dense_index_ref * _component_size]);
    if (index_to_remove != last_dense_index) {
      uint8_t *dst = &_dense[index_to_remove * _component_size];
      uint8_t *src = &_dense[last_dense_index * _component_size];
      _move_construct_callback(dst, src);
      _destroy_callback(src);
      const size_t last_page_index = last_key / page_size;
      const size_t last_page_offset = last_key % page_size;
      (*_sparse[last_page_index])[last_page_offset] = index_to_remove;
      _dense_index_to_key[index_to_remove] = last_key;
    }
    dense_index_ref = invalid_index;
    _dense_index_to_key.pop_back();
    _dense.resize(_dense.size() - _component_size);
    if (--_occupancy[page_index] == 0) {
      delete _sparse[page_index];
      _sparse[page_index] = nullptr;
    }
  }

  const size_t _component_size;
  std::function<void(uint8_t *)> _destroy_callback;
  std::function<void(uint8_t *, uint8_t *)> _move_construct_callback;
  std::function<void(uint8_t *)> _construct_callback;

  std::vector<page_t *> _sparse;
  std::vector<uint64_t> _occupancy;
  std::vector<uint8_t> _dense;
  std::vector<uint64_t> _dense_index_to_key;
};

#endif
