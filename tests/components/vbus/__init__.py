import esphome.codegen as cg
from esphome.components import vbus
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        if hasattr(vbus, "CONF_ON_DATAGRAM"):
            cg.add_define("USE_VBUS_DATAGRAM")
        if hasattr(vbus, "CONF_TRANSMIT"):
            cg.add_define("USE_VBUS_TX")

    manifest.multi_conf = False
    manifest.to_code = to_code_testing
