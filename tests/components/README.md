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

During C++ test builds, `to_code` is suppressed for every component by default — most components do not
need to generate configuration code for a unit test binary.

#### Manifest overrides

If your component needs to customise code generation behavior for testing — for example to re-enable
`to_code`, supply a lightweight stub, add a test-only dependency, or change any other manifest attribute —
create an `__init__.py` in your component's test directory and define `override_manifest`:

**Top-level component** (`tests/components/<component>/__init__.py`):

```python
from tests.testing_helpers import ComponentManifestOverride

def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Re-enable the component's own to_code (needed when the component must
    # emit C++ setup code that the test binary depends on at link time).
    manifest.enable_codegen()
```

Or supply a lightweight stub instead of the real `to_code`:

```python
from tests.testing_helpers import ComponentManifestOverride

def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config):
        # Only emit what the C++ tests actually need
        pass

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["some_test_only_dep"]
```

**Platform component** (`tests/components/<component>/<domain>/__init__.py`,
e.g. `tests/components/my_sensor/sensor/__init__.py`):

```python
from tests.testing_helpers import ComponentManifestOverride

def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.enable_codegen()
```

`override_manifest` receives a `ComponentManifestOverride` that wraps the real manifest.
Attribute assignments store an override; reads fall back to the real manifest when no
override is present.

Key methods:

| Method | Effect |
|---|---|
| `manifest.enable_codegen()` | Remove the `to_code` suppression, re-enabling code generation |
| `manifest.restore()` | Clear **all** overrides, reverting every attribute to the original |

The function is called after `to_code` has already been set to `None`, so calling
`enable_codegen()` is a deliberate opt-in.

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
