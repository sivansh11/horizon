#ifndef ECS_HPP
#define ECS_HPP

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace ecs {

// storage would be u32/u64 or something similar, its the underlaying storage of
// the bits
template <typename storage_t> class bit_set_t {
public:
  bit_set_t(storage_t data = 0) : _data(data) {}

  void set(uint32_t n) {
    assert(n < (sizeof(storage_t) * 8));
    _data = _data | (static_cast<storage_t>(1) << n);
  }

  void unset(uint32_t n) {
    assert(n < (sizeof(storage_t) * 8));
    _data = _data & ~(static_cast<storage_t>(1) << n);
  }

  void toggle(uint32_t n) {
    assert(n < (sizeof(storage_t) * 8));
    _data = _data ^ (static_cast<storage_t>(1) << n);
  }

  bool test(uint32_t n) const {
    assert(n < (sizeof(storage_t) * 8));
    return (_data & (static_cast<storage_t>(1) << n)) != 0;
  }

  bool test_all(const bit_set_t &other) const {
    return (other._data & ~_data) == 0;
  }

  constexpr uint32_t size() const { return sizeof(storage_t) * 8; }

private:
  storage_t _data;
};

template <typename storage_t>
std::ostream &operator<<(std::ostream &o, const bit_set_t<storage_t> &bit_set) {
  for (uint32_t i = 0; i < sizeof(storage_t) * 8; i++) {
    o << bit_set.test(i);
  }
  return o;
}

using entity_id_t = uint32_t;
using component_id_t = uint32_t;
using component_mask_t = bit_set_t<uint32_t>;

static const entity_id_t null_entity_id =
    std::numeric_limits<entity_id_t>::max();

template <typename T>
component_id_t _get_component_id_for(component_id_t &component_id_counter) {
  static component_id_t s_component_id = component_id_counter++;
  return s_component_id;
}

static constexpr uint32_t invalid_index = std::numeric_limits<uint32_t>::max();

template <typename value_t, size_t page_size> struct sparse_map {
  using page_t = std::array<uint32_t, page_size>;

  value_t *get(uint32_t id) {
    uint32_t page_index = id / page_size;
    uint32_t in_page_index = id % page_size;

    assert(page_index < sparse.size());

    uint32_t dense_index = sparse[page_index][in_page_index];

    assert(dense_index != invalid_index);

    return dense.data() + sparse[page_index][in_page_index];
  }

  bool check_if_value_exist(uint32_t id) {
    uint32_t page_index = id / page_size;
    uint32_t in_page_index = id % page_size;

    if (page_index >= sparse.size())
      return false;

    uint32_t dense_index = sparse[page_index][in_page_index];

    if (dense_index == invalid_index)
      return false;

    return true;
  }

  template <typename... args_t>
  value_t *construct(uint32_t id, args_t &&...args) {
    uint32_t page_index = id / page_size;
    uint32_t in_page_index = id % page_size;

    while (page_index >= sparse.size()) {
      uint32_t current_page_index = sparse.size();
      sparse.emplace_back();
      sparse[current_page_index].fill(invalid_index);
    }

    uint32_t dense_index = sparse[page_index][in_page_index];

    assert(dense_index == invalid_index);

    dense_index = dense.size();

    while (dense.size() < (dense_index + 1)) {
      dense.emplace_back();
    }

    while (dense_to_id.size() <= dense_index) {
      dense_to_id.emplace_back(invalid_index);
    }

    value_t *component =
        reinterpret_cast<value_t *>(dense.data() + (dense_index));

    new (component) value_t{std::forward<args_t>(args)...};

    dense_to_id[dense_index] = id;
    sparse[page_index][in_page_index] = dense_index;

    return component;
  }

  void destroy(uint32_t id) {
    uint32_t page_index = id / page_size;
    uint32_t in_page_index = id % page_size;

    uint32_t dense_index = sparse[page_index][in_page_index];

    assert(dense_index != invalid_index);

    uint32_t top_dense_index = dense.size() - 1;

    uint32_t top_id = dense_to_id[top_dense_index];

    uint32_t top_page_index = top_id / page_size;
    uint32_t top_in_page_index = top_id % page_size;

    std::swap(dense[dense_index], dense[top_dense_index]);
    std::swap(dense_to_id[dense_index], dense_to_id[top_dense_index]);
    std::swap(sparse[page_index][in_page_index],
              sparse[top_page_index][top_in_page_index]);

    dense.pop_back();

    dense_to_id.pop_back();

    sparse[page_index][in_page_index] = invalid_index;
  }
  std::vector<page_t> sparse;
  std::vector<value_t> dense;
  std::vector<uint32_t> dense_to_id;
};

template <size_t page_size> struct base_component_pool_t {
  using page_t = std::array<uint32_t, page_size>;

  base_component_pool_t(component_id_t id, uint32_t component_size)
      : _id(id), _component_size(component_size) {}

  virtual ~base_component_pool_t() {}

  void *get(entity_id_t id) {
    uint32_t page_index = id / page_size;
    uint32_t in_page_index = id % page_size;

    page_t &page = *sparse.get(page_index);

    uint32_t dense_index = page[in_page_index];

    assert(dense_index != invalid_index);

    return dense.data() + (dense_index * _component_size);
  }

  virtual void destroy(entity_id_t id) = 0;

  component_id_t _id;
  uint32_t _component_size;
  sparse_map<page_t, 1> sparse;
  std::vector<uint8_t> dense;
  std::vector<entity_id_t> dense_index_to_entity_id;
};

template <typename T, size_t page_size>
struct component_pool_t : public base_component_pool_t<page_size> {
  component_pool_t(component_id_t id)
      : base_component_pool_t<page_size>(id, sizeof(T)) {}

  ~component_pool_t() override {}

  template <typename... args_t> T *construct(entity_id_t id, args_t &&...args) {
    uint32_t page_index = id / page_size;
    uint32_t in_page_index = id % page_size;

    if (!this->sparse.check_if_value_exist(page_index)) {
      this->sparse.construct(page_index)->fill(invalid_index);
    }

    auto &page = *this->sparse.get(page_index);

    uint32_t dense_index = page[in_page_index];

    assert(dense_index == invalid_index);

    dense_index = this->dense.size() / sizeof(T);

    while (this->dense.size() < (dense_index + 1) * this->_component_size) {
      this->dense.emplace_back(0);
    }

    while (this->dense_index_to_entity_id.size() <= dense_index) {
      this->dense_index_to_entity_id.emplace_back(ecs::null_entity_id);
    }

    T *component = reinterpret_cast<T *>(this->dense.data() +
                                         (dense_index * this->_component_size));

    new (component) T{std::forward<args_t>(args)...};

    this->dense_index_to_entity_id[dense_index] = id;
    page[in_page_index] = dense_index;

    return component;
  }

  void destroy(entity_id_t id) override {
    uint32_t page_index = id / page_size;
    uint32_t in_page_index = id % page_size;

    auto &page = *this->sparse.get(page_index);

    uint32_t dense_index = page[in_page_index];

    assert(dense_index != invalid_index);

    uint32_t top_dense_index = this->dense.size() / sizeof(T) - 1;

    entity_id_t top_id = this->dense_index_to_entity_id[top_dense_index];

    uint32_t top_page_index = top_id / page_size;
    uint32_t top_in_page_index = top_id % page_size;

    auto &top_page = *this->sparse.get(top_page_index);

    std::swap(*(T *)(&this->dense[dense_index * this->_component_size]),
              *(T *)(&this->dense[top_dense_index * this->_component_size]));
    std::swap(this->dense_index_to_entity_id[dense_index],
              this->dense_index_to_entity_id[top_dense_index]);
    std::swap(page[in_page_index], top_page[top_in_page_index]);

    for (uint32_t i = 0; i < sizeof(T); i++)
      this->dense.pop_back();

    this->dense_index_to_entity_id.pop_back();

    page[in_page_index] = invalid_index;

    // check if page is empty, destroy page
    for (uint32_t i = 0; i < page_size; i++) {
      if (page[i] != invalid_index) return;
    }

    this->sparse.destroy(page_index);
  }
};

template <size_t page_size = 2048> class scene_t {
  struct entity_description_t {
    entity_id_t id;
    component_mask_t mask;
    bool is_valid;
  };

public:
  scene_t(entity_id_t max_entities = 1000) : _max_entities(max_entities) {
    _available_entities.reserve(_max_entities);
    entity_id_t counter = _max_entities;
    while (counter--) {
      _available_entities.push_back(counter);
    }
  }

  ~scene_t() {
    for (auto &entity : _entities)
      if (entity.is_valid) {
        for (auto &component_pool : _component_pools)
          if (entity.mask.test(component_pool->_id)) {
            component_pool->destroy(entity.id);
          }
      }

    for (auto &component_pool : _component_pools) {
      delete component_pool;
    }
  }

  template <typename T> component_id_t get_component_id_for() {
    return _get_component_id_for<T>(component_id_counter);
  }

  entity_id_t create() {
    assert(_available_entities.size() > 0);
    entity_id_t id = _available_entities[_available_entities.size() - 1];
    _available_entities.pop_back();

    if (_entities.size() <= id) {
      entity_description_t entity_description = {
          .id = id, .mask = {}, .is_valid = true};

      _entities.push_back(entity_description);
    } else {
      assert(!_entities[id].is_valid);
    }

    return id;
  }

  void destroy(entity_id_t id) {
    assert(id != null_entity_id);
    assert(_entities[id].is_valid);
    entity_description_t &entity_description = _entities[id];
    for (component_id_t i = 0; i < entity_description.mask.size(); i++) {
      if (entity_description.mask.test(i)) {
        _component_pools[i]->destroy(id);
      }
    }
    entity_description.is_valid = false;
    entity_description.mask = {};
    _available_entities.push_back(id);
  }

  template <typename T> T &get(entity_id_t id) {
    assert(id != null_entity_id);
    assert(_entities[id].is_valid);

    component_id_t component_id = get_component_id_for<T>();
    if (_component_pools.size() <= component_id) {
      _component_pools.resize(component_id + 1);
      _component_pools[component_id] =
          new component_pool_t<T, page_size>(component_id);
    }

    entity_description_t &entity_description = _entities[id];
    entity_description.mask.set(component_id);
    return *reinterpret_cast<T *>(_component_pools[component_id]->get(id));
  }

  template <typename T, typename... args_t>
  T &construct(entity_id_t id, args_t &&...args) {
    assert(id != null_entity_id);
    assert(_entities[id].is_valid);
    assert(!_entities[id].mask.test(get_component_id_for<T>()));

    component_id_t component_id = get_component_id_for<T>();
    if (_component_pools.size() <= component_id) {
      _component_pools.resize(component_id + 1);
      _component_pools[component_id] =
          reinterpret_cast<base_component_pool_t<page_size> *>(
              new component_pool_t<T, page_size>(component_id));
    }

    entity_description_t &entity_description = _entities[id];
    entity_description.mask.set(component_id);
    return *(reinterpret_cast<component_pool_t<T, page_size> *>(
                 _component_pools[component_id])
                 ->construct(id, std::forward<args_t>(args)...));
  }

  template <typename T> void remove(entity_id_t id) {
    assert(id != null_entity_id);
    assert(_entities[id].is_valid);
    assert(_entities[id].mask.test(get_component_id_for<T>()));

    component_id_t component_id = get_component_id_for<T>();
    if (_component_pools.size() <= component_id) {
      _component_pools.resize(component_id + 1);
      _component_pools[component_id] =
          reinterpret_cast<base_component_pool_t<page_size> *>(
              new component_pool_t<T, page_size>(component_id));
    }

    _component_pools[component_id]->destroy(id);

    entity_description_t &entity_description = _entities[id];
    entity_description.mask.unset(component_id);
  }

  template <typename... T> bool has(entity_id_t id) {
    assert(id != null_entity_id);
    assert(_entities[id].is_valid);

    component_mask_t mask{};
    component_id_t component_ids[] = {get_component_id_for<T>()...};
    for (uint32_t i = 0; i < sizeof...(T); i++) {
      mask.set(component_ids[i]);
    }

    entity_description_t &entity_description = _entities[id];

    return entity_description.mask.test_all(mask);
  }

  template <typename... T> void for_all(auto callback) {
    if constexpr (sizeof...(T) == 0) {
      for (auto &entity : _entities)
        if (entity.is_valid) {
          callback(entity.id);
        }
    } else {
      component_mask_t mask{};
      component_id_t component_ids[] = {get_component_id_for<T>()...};
      for (uint32_t i = 0; i < sizeof...(T); i++) {
        mask.set(component_ids[i]);
      }
      for (auto &entity : _entities)
        if (entity.is_valid && entity.mask.test_all(mask)) {
          callback(entity.id, get<T>(entity.id)...);
        }
    }
  }

private:
  // TODO: make a free entities queue
  const entity_id_t _max_entities;
  std::vector<entity_id_t> _available_entities;
  std::vector<entity_description_t> _entities;
  std::vector<base_component_pool_t<page_size> *> _component_pools;
  component_id_t component_id_counter = 0;
};

} // namespace ecs
#endif
