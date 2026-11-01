---
name: embedded-memory
description: Choose ESPHome C++ containers and storage that avoid heap use and fragmentation on microcontrollers - std::array, StaticVector with cg.slot_list/SLOT_LIST, FixedVector, std::unique_ptr byte buffers, StringRef, RAMAllocator, and what to avoid (std::vector growth, std::map, std::deque, std::string, new (std::nothrow)). Use when adding or changing C++ members, containers, buffers, strings or registration lists, or when reducing RAM or flash use.
---

# Memory on embedded targets

ESP devices run for months with small heaps shared between Wi-Fi, BLE, LWIP, and application code. Over
time, repeated allocations of different sizes fragment the heap. Failures happen when the largest
contiguous block shrinks, even if total free heap is still large. We have seen field crashes caused by
this.

**Heap allocation after `setup()` should be avoided unless absolutely unavoidable.** Every
allocation/deallocation cycle contributes to fragmentation. ESPHome treats runtime heap allocation as a
long-term reliability bug, not a performance issue. Helpers that hide allocation (`std::string`,
`std::to_string`, string-returning helpers) are being deprecated and replaced with buffer and view based
APIs.

## Container guidelines

1. **Compile-time-known sizes:** use `std::array` instead of `std::vector` when size is known at compile
   time.
   ```cpp
   // Bad - generates STL realloc code
   std::vector<int> values;

   // Good - no dynamic allocation
   std::array<int, MAX_VALUES> values;
   ```
   Use `cg.add_define("MAX_VALUES", count)` to set the size from Python configuration.

   **For byte buffers:** avoid `std::vector<uint8_t>` unless the buffer needs to grow. Use
   `std::unique_ptr<uint8_t[]>` instead. It does **not** provide bounds checking or iterator support like
   `std::vector<uint8_t>`; use it only when you do not need these features.
   ```cpp
   // Bad - STL overhead for simple byte buffer
   std::vector<uint8_t> buffer;
   buffer.resize(256);

   // Good - minimal overhead, single allocation
   std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(256);
   // Or if size is constant:
   std::array<uint8_t, 256> buffer;
   ```

2. **Compile-time-known fixed sizes with vector-like API:** use `StaticVector` from
   `esphome/core/helpers.h` for compile-time fixed size with a `push_back()` interface (no dynamic
   allocation).
   ```cpp
   // Bad - generates STL realloc code (_M_realloc_insert)
   std::vector<ServiceRecord> services;
   services.reserve(5);  // Still includes reallocation machinery

   // Good - compile-time fixed size, no dynamic allocation
   StaticVector<ServiceRecord, MAX_SERVICES> services;
   services.push_back(record1);
   ```
   Use `cg.add_define("MAX_SERVICES", count)` to set the size from Python configuration.

   **Listener and child-entity registration lists are the most common case, and the most commonly
   missed.** A `register_*()` method called once per child at code generation time has a count that is
   known at compile time, so it should never be a `std::vector`. Use `cg.slot_list()` with the `SLOT_LIST`
   macro: each call reserves a slot for that hub and returns the register call (for `cg.add()` or a `sub_*`
   setter). After every `to_code` has run, the define holds the largest count any one hub needs, so every
   hub of a `MULTI_CONF` component fits. Pass the hub component's `DOMAIN`: whenever that component is in
   the build the define exists, as 0 when nothing registered, and a list of 0 takes no space.
   ```python
   # hub component's __init__.py
   register_listener = cg.slot_list(
       "MY_COMPONENT_LISTENER_COUNT", "register_listener", DOMAIN
   )


   # a platform of the hub
   async def to_code(config: ConfigType) -> None:
       ...
       hub = await cg.get_variable(config[CONF_MY_COMPONENT_ID])
       cg.add(register_listener(hub, var))
   ```
   ```cpp
   class MyComponent : public Component {
    public:
     SLOT_LIST(MyComponentListener, listeners_, register_listener, MY_COMPONENT_LISTENER_COUNT)
   ```
   No `#ifdef` around the macro or the loops that walk the list: with a count of 0 they compile to nothing.
   Keep an `#if MY_COMPONENT_LISTENER_COUNT` only around code that needs a registered child's own component
   (its headers, its entity type). When registering takes more than a `push_back` (a struct entry), keep a
   hand-written method, preceded by `SLOT_LIST_REQUIRES(MY_COMPONENT_LISTENER_COUNT)`, over a
   `[[no_unique_address]] SlotList<Entry, MY_COMPONENT_LISTENER_COUNT>` member (the attribute keeps an empty
   list free), still registered through
   `cg.slot_list()`. Use `cg.slot_counter()` with the hub as the key, `_request_slot(str(hub))`, when the C++
   side adds itself to the list (from its own `setup()`, say) or when the define gates more than the list;
   that define is absent when nothing requested a slot, so its storage sits behind `#ifdef`.
   Request slots from `to_code`, not from a job that runs after `CoroPriority.FINAL`; a late request
   raises rather than silently undercounting.

3. **Runtime-known sizes:** use `FixedVector` from `esphome/core/helpers.h` when the size is only known at
   runtime initialization.
   ```cpp
   // Bad - generates STL realloc code (_M_realloc_insert)
   std::vector<TxtRecord> txt_records;
   txt_records.reserve(5);  // Still includes reallocation machinery

   // Good - runtime size, single allocation, no reallocation machinery
   FixedVector<TxtRecord> txt_records;
   txt_records.init(record_count);  // Initialize with exact size at runtime
   ```
   This eliminates `_M_realloc_insert` and `_M_default_append` template instantiations (200-500 bytes per
   instance), needs no upper bound, and is compatible with protobuf code generation when using
   `[(fixed_vector) = true]`.

4. **Small datasets (1-16 elements):** use `std::vector` or `std::array` with simple structs instead of
   `std::map`/`std::set`/`std::unordered_map`.
   ```cpp
   // Bad - 2KB+ overhead for red-black tree/hash table
   std::map<std::string, int> small_lookup;
   std::unordered_map<int, std::string> tiny_map;

   // Good - simple struct with linear search (std::vector is fine)
   struct LookupEntry {
     const char *key;
     int value;
   };
   std::vector<LookupEntry> small_lookup = {
     {"key1", 10},
     {"key2", 20},
     {"key3", 30},
   };
   // Or std::array if size is compile-time constant:
   // std::array<LookupEntry, 3> small_lookup = {{ ... }};
   ```
   Linear search on small datasets is often faster than hashing or tree overhead, but this depends on
   lookup frequency; in hot paths the O(1) vs O(n) difference may still matter. It is the heavy containers
   (`map`, `set`, `unordered_map`) that should be avoided for small datasets unless profiling shows
   otherwise.

5. **Strings set once from configuration:** use `StringRef` (`esphome/core/string_ref.h`) rather than
   `std::string`. Code generation passes a string literal that lives in flash for the life of the program,
   so storing a `std::string` copies it onto the heap for nothing. `StringRef` is a non-owning pointer plus
   length; it does not copy, and it must only ever refer to storage that outlives it (a string literal, or
   a buffer owned elsewhere).
   ```cpp
   // Bad - heap copy of a literal that is already in flash
   void set_keys(std::string keys) { this->keys_ = std::move(keys); }
   std::string keys_;
   ```
   ```cpp
   // Good - no allocation
   void set_keys(const char *keys) { this->keys_ = StringRef(keys); }
   StringRef keys_;
   ```

6. **Avoid `std::deque`:** it allocates in 512-byte blocks regardless of element size, guaranteeing at
   least 512 bytes of RAM usage immediately. This is a major source of crashes on memory-constrained
   devices.

7. **Never use `new (std::nothrow)`:** on ESP-IDF exceptions are disabled, so a failed nothrow allocation
   aborts instead of returning `nullptr`. Use `RAMAllocator` from `esphome/core/helpers.h`; CI rejects
   `std::nothrow`.

8. **Detection:** look for these patterns in compiler output:
   - Large code sections with STL symbols (vector, map, set)
   - `alloc`, `realloc`, `dealloc` in symbol names
   - `_M_realloc_insert`, `_M_default_append` (vector reallocation)
   - Red-black tree code (`rb_tree`, `_Rb_tree`)
   - Hash table infrastructure (`unordered_map`, `hash`)

Callback storage (`CallbackManager` vs `LazyCallbackManager`) and templated registration methods are
covered in the `automations` skill.

## Where to spend effort

Prioritize container optimization for core components (API, network, logger), widely used components
(mdns, wifi, ble), and components causing flash size complaints. Avoiding heap allocation after `setup()`
is always required regardless of component type; the priority list is about effort spent on container
migration (e.g. `std::vector` to `StaticVector`).
