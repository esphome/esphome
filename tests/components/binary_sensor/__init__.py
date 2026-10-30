from tests.testing_helpers import ComponentManifestOverride, add_defines_to_code


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # filter.cpp only compiles with the define the real to_code emits for configured filters
    add_defines_to_code(
        manifest, "USE_BINARY_SENSOR_FILTER", "USE_BINARY_SENSOR_RAW_CALLBACK"
    )
