from tests.testing_helpers import ComponentManifestOverride, add_defines_to_code


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The filter benchmarks need filter.cpp, which is only built with this define
    add_defines_to_code(manifest, "USE_BINARY_SENSOR_FILTER")
