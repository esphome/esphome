import esphome.codegen as cg
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        CORE.testing_ensure_platform_registered("text_sensor")
        cg.add_define("USE_SENSOR_FILTER")

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["sensor", "spi", "text_sensor"]
