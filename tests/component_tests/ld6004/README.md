# LD6004 protocol tests

`test_protocol.py` compiles and executes the production protocol with address and
undefined-behaviour sanitisers on macOS and Linux. Windows runs the same harness
without sanitiser flags because the CI MinGW toolchain lacks their runtime libraries.
