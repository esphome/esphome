import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # One empty config entry so to_code runs and compiles the PG/GPO switch and status entity paths.
    manifest.multi_conf = False
    manifest.dependencies = manifest.dependencies + [
        "binary_sensor",
        "i2c",
        "switch",
        "text_sensor",
    ]

    async def to_code(config: ConfigType) -> None:
        cg.add_define("USE_BQ25186_PG_GPO")
        cg.add_define("USE_BINARY_SENSOR")
        cg.add_define("USE_BINARY_SENSOR_RAW_CALLBACK")
        cg.add_define("ESPHOME_ENTITY_BINARY_SENSOR_COUNT", 0)
        cg.add_define("USE_TEXT_SENSOR")
        cg.add_define("ESPHOME_ENTITY_TEXT_SENSOR_COUNT", 0)
        cg.add_define("USE_BQ25186_STATUS_PUBLISHER")

    manifest.to_code = to_code
