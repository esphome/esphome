from tests.testing_helpers import ComponentManifestOverride, add_defines_to_code


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # core (esphome/core/config.py) must run its to_code during C++ test builds
    # because it bootstraps the fundamental application infrastructure that all
    # components depend on (component registration, event loop, etc.).
    # USE_LINE_READER keeps line_reader.cpp in the build for its tests.
    # The callback manager pool is larger than every test build uses (test_callback.cpp).
    add_defines_to_code(
        manifest, "USE_LINE_READER", ("ESPHOME_CALLBACK_MANAGER_POOL_SIZE", 256)
    )
