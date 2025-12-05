from esphome.cpp_generator import MockObj

CODEOWNERS = ["@stuartparmenter"]

# Use fully-qualified namespace to avoid collision with external hub75 library's global ::hub75 namespace
hub75_ns = MockObj("::esphome::hub75", "::")
