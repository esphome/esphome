import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The driver lives in the sensor platform, so pull in what it builds against, ST's library included.
    manifest.dependencies = manifest.dependencies + ["i2c", "sensor"]

    async def to_code(config: ConfigType) -> None:
        cg.add_library("esphome/VL53L4CX", "2.0.0")

    manifest.to_code = to_code
