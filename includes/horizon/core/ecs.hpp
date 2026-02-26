#ifndef ECS_HPP
#define ECS_HPP

#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <queue>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "horizon/core/sparse_map.hpp"

namespace ecs {

template <typename type_t> //
class bitset_t {
public:
  using storage_t = type_t;
  constexpr bitset_t(storage_t data = 0) : _data(data) {}
  bitset_t(const bitset_t &other) : _data(other._data) {}
  bitset_t(const bitset_t &&other) : _data(other._data) {}
  bitset_t &operator=(const bitset_t &other) {
    _data = other._data;
    return *this;
  }
  bitset_t &operator=(const bitset_t &&other) {
    _data = other._data;
    return *this;
  }
  bool operator==(const bitset_t<storage_t> &other) const {
    return _data == other._data;
  }
  bitset_t operator&(const bitset_t<storage_t> &other) const {
    return bitset_t<storage_t>(_data & other._data);
  }
  storage_t get_data() const { return _data; }
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
  bool test_all(const bitset_t &other) const {
    return (other._data & ~_data) == 0;
  }
  static constexpr uint32_t size() { return sizeof(storage_t) * 8; }

private:
  storage_t _data;
};

} // namespace ecs

namespace std {

template <typename storage_t> struct hash<ecs::bitset_t<storage_t>> {
  size_t operator()(const ecs::bitset_t<storage_t> &b) const {
    return std::hash<storage_t>{}(b.get_data());
  }
};

} // namespace std

namespace ecs {

using entityid_t = uint64_t;
using componentid_t = uint64_t;
using component_mask_t = bitset_t<uint64_t>;

constexpr entityid_t null_entity = 0;
constexpr component_mask_t null_mask = 0;

struct vtable_t {
  void (*destroy)(void *);
  void (*move_construct)(void *, void *);
  void (*construct)(void *);
};

inline static componentid_t component_counter;
inline static std::vector<size_t> component_sizes{};
inline static std::vector<size_t> component_alignment{};
inline static std::vector<vtable_t> component_vtable{};
template <typename component_t> //
inline componentid_t get_componentid_for() {
  static const componentid_t s_componentid = []() {
    componentid_t componentid = component_counter++;
    component_sizes.resize(componentid + 1, 0);
    component_alignment.resize(componentid + 1, 0);
    component_vtable.resize(componentid + 1, vtable_t{nullptr, nullptr});
    component_sizes[componentid] = sizeof(component_t);
    component_alignment[componentid] = alignof(component_t);
    component_vtable[componentid] = {
        .destroy =
            [](void *ptr) { static_cast<component_t *>(ptr)->~component_t(); },
        .move_construct =
            [](void *dst, void *src) {
              new (dst)
                  component_t(std::move(*static_cast<component_t *>(src)));
            },
        .construct = [](void *ptr) { new (ptr) component_t{}; }};
    return componentid;
  }();
  return s_componentid;
}

struct scene_t {
  struct pool_t {
    size_t offsets[component_mask_t::size()];
    component_mask_t mask;
    size_t size;
    sparse_map<1024> storage;
  };

  struct entity_t {
    entityid_t id;
    component_mask_t mask;
    bool is_valid;
  };

  using pools_t = std::unordered_map<component_mask_t, pool_t>;

  scene_t() {
    // empty entity for null_entity
    _entities.emplace_back();
  }
  ~scene_t() {
    // TODO: implement
  }

  entityid_t create() {
    if (_available_entityids.size()) {
      entityid_t id = _available_entityids.front();
      _available_entityids.pop();
      assert(_entities.size() > id);
      assert(!_entities[id].is_valid);
      _entities[id].is_valid = true;
      _entities[id].mask = null_mask;
      return id;
    } else {
      entityid_t id = ++_entity_counter;
      _entities.emplace_back(id, null_mask, true);
      return id;
    }
  }

  void destroy(entityid_t id) {
    if (id == null_entity)
      return;
    entity_t &entity = get_entity_from(id);
    if (!entity.is_valid)
      return;
    _available_entityids.push(id);
    entity.is_valid = false;
    if (entity.mask == null_mask)
      return;
    pool_t &pool = get_pool_from(entity.mask);
    pool.storage.erase(id);
    entity.mask = null_mask;
  }

  // returns nullptr on failure
  template <typename... component_t> //
  decltype(auto) at(entityid_t id) {
    static_assert(sizeof...(component_t) != 0,
                  "need to provide atleast 1 component");
    auto [data, mask] = get_context_from(id);
    pool_t &pool = get_pool_from(mask);
    if constexpr (sizeof...(component_t) == 1)
      return get_ptr_from<component_t...>(data, pool);
    else
      return std::make_tuple<component_t *...>(
          get_ptr_from<component_t>(data, pool)...);
  }

  template <typename component_t, typename... args_t> //
  void construct(void *ptr, args_t &&...args) {
    new (ptr) component_t{std::forward<args_t>(args)...};
  }

  // returns nullptr on failure
  template <typename... component_t, typename... args_t> //
  decltype(auto) insert(entityid_t id, args_t &&...args) {
    static_assert(sizeof...(component_t) != 0,
                  "need to provide atleast 1 component");
    entity_t &entity = get_entity_from(id);
    if (id == null_entity || !entity.is_valid)
      return at<component_t...>(id);
    componentid_t componentids[] = {get_componentid_for<component_t>()...};
    // need to add this section since other wise componentids gets optimised out
    // and _component_sizes is never filled
    for (auto componentid : componentids)
      component_sizes.resize(std::max(componentid + 1, component_sizes.size()),
                             0);
    ((component_sizes[get_componentid_for<component_t>()] =
          sizeof(component_t)),
     ...);
    component_mask_t old_mask = entity.mask;
    component_mask_t new_mask = entity.mask;
    (new_mask.set(get_componentid_for<component_t>()), ...);
    if (old_mask == new_mask) {
      uint8_t *data = get_data_from(get_pool_from(old_mask), entity);
      component_mask_t mask{};
      (mask.set(get_componentid_for<component_t>()), ...);
      component_mask_t::storage_t bits = mask.get_data();
      while (bits > 0) {
        int i = std::countr_zero(bits);
        size_t offset = get_offset_of(i, get_pool_from(old_mask));
        if constexpr (sizeof...(args_t) > 0) {
          assert(component_vtable.size() > i);
          component_vtable[i].destroy(data + offset);
          construct<component_t...>(data + offset,
                                    std::forward<args_t>(args)...);
        }
        bits &= bits - 1;
      }
      return at<component_t...>(id);
    }
    uint8_t *old_data = old_mask == null_mask
                            ? nullptr
                            : get_data_from(get_pool_from(old_mask), entity);
    pool_t &new_pool = get_pool_from(new_mask);
    uint8_t *new_data = insert_entity_into_pool(id, new_pool);
    entity.mask = new_mask;
    if (old_data) {
      pool_t &old_pool = get_pool_from(old_mask);
      component_mask_t::storage_t bits = old_mask.get_data();
      while (bits > 0) {
        int i = std::countr_zero(bits);
        size_t old_offset = get_offset_of(i, old_pool);
        size_t new_offset = get_offset_of(i, new_pool);
        assert(component_vtable.size() > 1);
        component_vtable[i].move_construct(new_data + new_offset,
                                           old_data + old_offset);
        bits &= bits - 1;
      }
      old_pool.storage.erase(id);
    }
    component_mask_t mask{};
    (mask.set(get_componentid_for<component_t>()), ...);
    component_mask_t::storage_t bits = mask.get_data();
    while (bits > 0) {
      int i = std::countr_zero(bits);
      size_t offset = get_offset_of(i, new_pool);
      if constexpr (sizeof...(component_t) == 1) {
        if constexpr (sizeof...(args_t) > 0) {
          assert(component_vtable.size() > i);
          component_vtable[i].destroy(new_data + offset);
          construct<component_t...>(new_data + offset,
                                    std::forward<args_t>(args)...);
        }
      } else {
        // no need to call anything, construct callback already called by
        // sparse_map
      }
      bits &= bits - 1;
    }
    return at<component_t...>(id);
  }

  template <typename... component_t> //
  void erase(entityid_t id) {
    static_assert(sizeof...(component_t) != 0,
                  "need to provide atleast 1 component");
    componentid_t componentids[] = {get_componentid_for<component_t>()...};
    // need to add this section since other wise componentids gets optimised out
    // and _component_sizes is never filled
    for (auto componentid : componentids)
      component_sizes.resize(std::max(componentid + 1, component_sizes.size()),
                             0);
    ((component_sizes[get_componentid_for<component_t>()] =
          sizeof(component_t)),
     ...);
    entity_t &entity = get_entity_from(id);
    if (id == null_entity || !entity.is_valid || entity.mask == null_mask)
      return;
    component_mask_t old_mask = entity.mask;
    component_mask_t new_mask = entity.mask;
    (new_mask.unset(get_componentid_for<component_t>()), ...);
    if (old_mask == new_mask)
      return;
    pool_t &old_pool = get_pool_from(old_mask);
    uint8_t *old_data = get_data_from(old_pool, entity);
    if (new_mask == null_mask) {
      entity.mask = null_mask;
      old_pool.storage.erase(id);
      return;
    }
    pool_t &new_pool = get_pool_from(new_mask);
    uint8_t *new_data = insert_entity_into_pool(id, new_pool);
    component_mask_t::storage_t bits = new_mask.get_data();
    while (bits > 0) {
      int i = std::countr_zero(bits);
      size_t old_offset = get_offset_of(i, old_pool);
      size_t new_offset = get_offset_of(i, new_pool);
      assert(component_vtable.size() > i);
      component_vtable[i].move_construct(new_data + new_offset,
                                         old_data + old_offset);
      bits &= bits - 1;
    }
    entity.mask = new_mask;
    old_pool.storage.erase(id);
  }

  template <typename... component_t> //
  struct view_t {
    static_assert(sizeof...(component_t) != 0,
                  "need to provide atleast 1 component");
    scene_t &scene;
    component_mask_t target_mask{};

    view_t(scene_t &scene) : scene(scene) {
      (target_mask.set(get_componentid_for<component_t>()), ...);
    }

    // TODO: iterators are invalid if user trys to insert while looping
    // a solution to this could be to use sparse_map for pools_t instead of
    // unordered_map
    struct iterator_t {
      scene_t &scene;
      component_mask_t target_mask;
      pools_t::iterator pool_iterator_current;
      pools_t::iterator pool_iterator_end;
      size_t dense_index;
      iterator_t(scene_t &scene, component_mask_t target_mask,
                 pools_t::iterator pool_iterator_current,
                 pools_t::iterator pool_iterator_end)
          : scene(scene), target_mask(target_mask),
            pool_iterator_current(pool_iterator_current),
            pool_iterator_end(pool_iterator_end), dense_index(0) {
        advance();
      }

      void advance() {
        while (pool_iterator_current != pool_iterator_end) {
          if ((pool_iterator_current->first & target_mask) == target_mask &&
              dense_index < pool_iterator_current->second.storage
                                ._dense_index_to_key.size())
            return;
          pool_iterator_current++;
          dense_index = 0;
        }
      }

      iterator_t &operator++() {
        dense_index++;
        if (dense_index >=
            pool_iterator_current->second.storage._dense_index_to_key.size()) {
          pool_iterator_current++;
          dense_index = 0;
        }
        advance();
        return *this;
      }

      bool operator!=(const iterator_t &other) const {
        return pool_iterator_current != other.pool_iterator_current ||
               dense_index != other.dense_index;
      }

      auto operator*() {
        entityid_t id = pool_iterator_current->second.storage
                            ._dense_index_to_key[dense_index];
        uint8_t *data =
            &pool_iterator_current->second.storage
                 ._dense[dense_index *
                         pool_iterator_current->second.storage._component_size];
        if constexpr (sizeof...(component_t) == 1)
          return scene.get_ptr_from_fast<component_t...>(
              data, pool_iterator_current->second);
        else
          return std::make_tuple<component_t *...>(
              scene.get_ptr_from_fast<component_t>(
                  data, pool_iterator_current->second)...);
      }
    };
    iterator_t begin() {
      return iterator_t(scene, target_mask, scene._pools.begin(),
                        scene._pools.end());
    }
    iterator_t end() {
      return iterator_t(scene, target_mask, scene._pools.end(),
                        scene._pools.end());
    }
  };

  template <typename... component_t> //
  view_t<component_t...> view() {
    return view_t<component_t...>(*this);
  }

  inline entity_t &get_entity_from(const entityid_t id) {
    if (id >= _entities.size())
      return _entities[0];
    return _entities[id];
  }

  inline size_t get_size_from(const pool_t &pool) { return pool.size; }

  inline pool_t &get_pool_from(const component_mask_t mask) {
    auto itr = _pools.find(mask);
    if (itr != _pools.end())
      return itr->second;
    size_t size = 0;
    size_t max_align = 1;
    component_mask_t::storage_t bits = mask.get_data();
    while (bits > 0) {
      int i = std::countr_zero(bits);
      assert(component_sizes.size() > i);
      assert(component_alignment.size() > i);
      size_t csize = component_sizes[i];
      size_t calignment = component_alignment[i];
      if (calignment > max_align)
        max_align = calignment;
      size = (size + calignment - 1) & ~(calignment - 1);
      size += csize;
      bits &= bits - 1;
    }
    size = (size + max_align - 1) & ~(max_align - 1);
    pool_t new_pool{
        .mask = mask,
        .storage = {size,
                    [this, mask](uint8_t *ptr) {
                      pool_t &pool = _pools.at(mask);
                      component_mask_t::storage_t bits = mask.get_data();
                      while (bits > 0) {
                        int i = std::countr_zero(bits);
                        assert(component_vtable.size() > i);
                        size_t offset = get_offset_of(i, pool);
                        component_vtable[i].destroy(ptr + offset);
                        bits &= bits - 1;
                      }
                    },
                    [this, mask](uint8_t *dst, uint8_t *src) {
                      pool_t &pool = _pools.at(mask);
                      component_mask_t::storage_t bits = mask.get_data();
                      while (bits > 0) {
                        int i = std::countr_zero(bits);
                        assert(component_vtable.size() > i);
                        size_t offset = get_offset_of(i, pool);
                        component_vtable[i].move_construct(dst + offset,
                                                           src + offset);
                        bits &= bits - 1;
                      }
                    },
                    [this, mask](uint8_t *ptr) {
                      pool_t &pool = _pools.at(mask);
                      component_mask_t::storage_t bits = mask.get_data();
                      while (bits > 0) {
                        int i = std::countr_zero(bits);
                        assert(component_vtable.size() > i);
                        size_t offset = get_offset_of(i, pool);
                        component_vtable[i].construct(ptr + offset);
                        bits &= bits - 1;
                      }
                    }}};
    for (auto &offset : new_pool.offsets) {
      offset = std::numeric_limits<size_t>::max();
    }
    size_t current_offset = 0;
    // TODO: maybe merge the loops
    bits = mask.get_data();
    while (bits > 0) {
      int i = std::countr_zero(bits);
      assert(component_sizes.size() > i);
      assert(component_alignment.size() > i);
      size_t calignment = component_alignment[i];
      current_offset = (current_offset + calignment - 1) & ~(calignment - 1);
      new_pool.offsets[i] = current_offset;
      current_offset += component_sizes[i];
      bits &= bits - 1;
    }
    _pools.emplace(std::pair{mask, new_pool});
    return _pools.at(mask);
  }

  inline uint8_t *get_data_from(pool_t &pool, const entity_t &entity) {
    assert(entity.is_valid);
    uint8_t *data = pool.storage.at(entity.id);
    assert(data); // if data is nullptr, entity not in pool
    return data;
  }

  inline size_t get_offset_of(const componentid_t componentid,
                              const pool_t &pool) {
    assert(pool.offsets[componentid] != std::numeric_limits<size_t>::max());
    return pool.offsets[componentid];
  }

  inline uint8_t *insert_entity_into_pool(entityid_t id, pool_t &pool) {
    assert(!pool.storage.at(id));
    uint8_t *data = pool.storage.insert(id);
    assert(data);
    return data;
  }

  inline size_t get_size_of(componentid_t componentid) {
    assert(component_sizes.size() > componentid);
    return component_sizes[componentid];
  }

  std::pair<uint8_t *, component_mask_t> get_context_from(entityid_t id) {
    if (id == null_entity)
      return {nullptr, null_mask};
    entity_t &entity = get_entity_from(id);
    if (!entity.is_valid || entity.mask == null_mask)
      return {nullptr, null_mask};
    pool_t &pool = get_pool_from(entity.mask);
    return {get_data_from(pool, entity), entity.mask};
  }

  template <typename component_t> //
  component_t *get_ptr_from(uint8_t *data, const pool_t &pool) {
    if (!data)
      return nullptr;
    componentid_t componentid = get_componentid_for<component_t>();
    if (!pool.mask.test(componentid))
      return nullptr;
    return reinterpret_cast<component_t *>(data +
                                           get_offset_of(componentid, pool));
  }

  template <typename component_t> //
  component_t *get_ptr_from_fast(uint8_t *data, const pool_t &pool) {
    componentid_t componentid = get_componentid_for<component_t>();
    return reinterpret_cast<component_t *>(data +
                                           get_offset_of(componentid, pool));
  }

  std::vector<entity_t> _entities;
  std::queue<entityid_t> _available_entityids;
  entityid_t _entity_counter = 0;
  pools_t _pools;
};

} // namespace ecs

#endif
