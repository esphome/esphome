# How to write C++ ESPHome unit tests

1. Locate the folder with your component or create a new one with the same name as the component.
2. Write the tests. You can add as many `.cpp` and `.h` files as you need to organize your tests.

**IMPORTANT**: wrap all your testing code in a unique namespace to avoid linker collisions when compiling
testing binaries that combine many components. By convention, this unique namespace is `esphome::component::testing`
(where "component" is the component under test), for example: `esphome::uart::testing`.

### Platform components

For components that expose to a platform component, create a folder under your component test folder with the platform component name, e.g. `binary_sensor` and
include the relevant `.cpp` and `.h` test files there.

### Override component code generation for testing

When generating code for testing, ESPHome suppresses `to_code` for all components except a small allowlist
(`CPP_TESTING_CODEGEN_COMPONENTS` in `script/cpp_unit_test.py`: `core`, `host`, `logger`), since most
components do not need to generate configuration code for C++ unit test builds.

#### Manifest overrides

If your component does need to customise code generation behaviour for testing — for example to restore
`to_code`, supply a lightweight stub, add a test-only dependency, or change any other manifest attribute —
create an `__init__.py` in your component's test directory and define `override_manifest`:

**Top-level component** (`tests/components/<component>/__init__.py`):

```python
from esphome.loader import TestingComponentManifest

def override_manifest(manifest: TestingComponentManifest) -> None:
    async def to_code_testing(config):
        # minimal stub — only emit what the C++ tests actually need
        pass

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["some_test_only_dep"]
```

**Platform component** (`tests/components/<component>/<domain>/__init__.py`,
e.g. `tests/components/my_sensor/sensor/__init__.py`):

```python
from esphome.loader import TestingComponentManifest

def override_manifest(manifest: TestingComponentManifest) -> None:
    manifest.to_code = None  # keep suppressed (explicit, for clarity)
```

`override_manifest` receives a `TestingComponentManifest` that wraps the real manifest.
Attribute assignments store an override; reads fall back to the real manifest when no
override is present.  Call `manifest.restore()` to clear all overrides and revert to
the original values.

The function is called after `to_code` has already been set to `None` for suppressed
components, so restoring it is a deliberate opt-in.

## Running component unit tests

(from the repository root)

```bash
./script/cpp_unit_test.py component1 component2 ...
```

The above will compile and run the provided components and their tests.

To run all tests, you can invoke `cpp_unit_test.py` with the special `--all` flag:

```bash
./script/cpp_unit_test.py --all
```

To run a specific test suite, you can provide a Google Test filter:

```bash
GTEST_FILTER='UART*' ./script/cpp_unit_test.py uart modbus
```

The process will return `0` for success or nonzero for failure. In case of failure, the errors will be printed out to the console.
