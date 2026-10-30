import esphome.codegen as cg
from esphome.components import vbus
from esphome.loader import FileResource
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_VBUS_CS_PLUS")
        cg.add_define("USE_VBUS_CS_PLUS_ADAPTIVE")
        cg.add_define("USE_VBUS_RX_DIAGNOSTICS")
        cg.add_define("USE_VBUS_CS_PLUS_WRITE")
        cg.add_define("USE_VBUS_CS_PLUS_ENTITIES")
        cg.add_define("USE_VBUS_WRITE_RESULT")
        cg.add_define("USE_VBUS_SETTING_TIME")
        cg.add_define("USE_DATETIME_TIME")
        cg.add_define("ESPHOME_ENTITY_TIME_COUNT", 0)
        cg.add_define("VBUS_SETTING_ENTITY_COUNT", 84)
        if hasattr(vbus, "CONF_ON_DATAGRAM"):
            cg.add_define("USE_VBUS_DATAGRAM")
        if hasattr(vbus, "CONF_TRANSMIT"):
            cg.add_define("USE_VBUS_TX")
        if hasattr(vbus, "CONF_TRANSACTIONS"):
            cg.add_define("USE_VBUS_TRANSACTIONS")

        if hasattr(vbus, "CONF_POLLING"):
            cg.add_define("USE_VBUS_POLLING")
            cg.add_define("VBUS_POLL_VALUE_COUNT", 84)

    manifest.multi_conf = False
    manifest.to_code = to_code_testing
    manifest.auto_load = [
        *manifest.auto_load,
        "sensor",
        "number",
        "select",
        "text_sensor",
        "datetime",
    ]
    manifest.add_resources(
        FileResource("esphome.components.vbus", "text_sensor/vbus_text_sensor.h"),
        FileResource("esphome.components.vbus", "sensor/vbus_sensor.h"),
        FileResource("esphome.components.vbus", "sensor/vbus_sensor.cpp"),
        FileResource("esphome.components.vbus", "number/vbus_number.h"),
        FileResource("esphome.components.vbus", "number/vbus_number.cpp"),
        FileResource("esphome.components.vbus", "select/vbus_select.h"),
        FileResource("esphome.components.vbus", "select/vbus_select.cpp"),
        FileResource("esphome.components.vbus", "datetime/vbus_time.h"),
        FileResource("esphome.components.vbus", "datetime/vbus_time.cpp"),
    )
