import esphome.codegen as cg
from esphome.loader import FileResource
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_VBUS_DATAGRAM")

    manifest.multi_conf = False
    manifest.to_code = to_code_testing
    manifest.auto_load = [*manifest.auto_load, "sensor"]
    manifest.add_resources(
        FileResource("esphome.components.vbus", "sensor/vbus_sensor.h"),
        FileResource("esphome.components.vbus", "sensor/vbus_sensor.cpp"),
    )
