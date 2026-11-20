import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The NIST type K helpers are compiled only under this define.
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_MAX31855_LINEARIZE")

    manifest.to_code = to_code_testing
    # The driver lives in the sensor platform, so pull in what it builds against.
    manifest.dependencies = manifest.dependencies + ["sensor", "spi"]
