# ESPHome Clang-Tidy Plugin

Custom clang-tidy checks for ESPHome to prevent heap-fragmenting code patterns.

## Checks

### `esphome-no-heap-helpers`

Flags functions that return `std::string` and cause heap allocations. On embedded
devices running for months, these allocations fragment the heap over time, eventually
causing crashes even when total free memory exists.

**Flagged functions:**

| Function | Replacement |
|----------|-------------|
| `std::to_string()` | `snprintf()` or stack-based formatting |
| `format_hex()` | `format_hex_to()` with stack buffer |
| `format_mac_address_pretty()` | `format_mac_addr_upper()` with stack buffer |
| `str_truncate()` | Unused - remove |
| `str_upper_case()` | Unused - remove |
| `str_snake_case()` | Unused - remove |

## Building

Requires LLVM/Clang development libraries (version 18 recommended to match CI).

```bash
# Install LLVM (macOS)
brew install llvm@18

# Install LLVM (Ubuntu/Debian)
apt install llvm-18-dev clang-18 libclang-18-dev

# Build the plugin
cd tools/clang-tidy-plugin
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=$(llvm-config-18 --prefix)
make
```

## Usage

Load the plugin when running clang-tidy:

```bash
clang-tidy -load ./build/libESPHomeClangTidyModule.so \
    -checks='-*,esphome-no-heap-helpers' \
    your_file.cpp -- [compiler flags]
```

Or add to `.clang-tidy`:

```yaml
Checks: >
  *,
  esphome-no-heap-helpers
```

And run with:

```bash
clang-tidy -load ./build/libESPHomeClangTidyModule.so your_file.cpp
```

## CI Integration

The plugin is built and used in CI to prevent new heap-allocating helpers from
being introduced. See `.github/workflows/ci.yml` for integration details.
