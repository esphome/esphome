import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # filter.cpp only compiles with the define the real to_code emits for configured filters
        cg.add_define("USE_BINARY_SENSOR_FILTER")
        # The raw callback is only compiled in when a config uses on_raw_value.
        cg.add_define("USE_BINARY_SENSOR_RAW_CALLBACK")

    manifest.to_code = to_code_testing
