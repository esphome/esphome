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

When generating code for testing, ESPHome won't invoke the component's `to_code` function, since most components do not
need to generate configuration code for testing.

If you do need to generate code to for example configure compilation flags or add libraries,
add the component name to the `CPP_TESTING_CODEGEN_COMPONENTS` allowlist in `script/cpp_unit_test.py`.

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
