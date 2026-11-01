from tests.testing_helpers import ComponentManifestOverride, add_defines_to_code


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # filter.cpp and automation.cpp only compile with the defines the real to_code emits
    add_defines_to_code(
        manifest,
        "USE_BINARY_SENSOR_FILTER",
        "USE_BINARY_SENSOR_RAW_CALLBACK",
        "USE_BINARY_SENSOR_CLICK_TRIGGER",
    )
