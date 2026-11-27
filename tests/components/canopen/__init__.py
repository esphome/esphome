# required for cpp unit testing.
import esphome.codegen as cg
from esphome.components.canopen import add_canopennode_library, enable_sdo_client
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # the gtests need CANopenNode's headers and the optional parts, but not a configured node (no canbus)
    async def to_code_testing(config: ConfigType) -> None:
        add_canopennode_library()
        # the tests run nodes (and client-only hubs)
        cg.add_define("USE_CANOPEN_NODE")
        enable_sdo_client()
        cg.add_define("USE_CANOPEN_HEARTBEAT")
        cg.add_define("USE_CANOPEN_EMCY")

    manifest.enable_codegen()
    manifest.to_code = to_code_testing
    # a MULTI_CONF component gets no to_code call in the unit test build
    manifest.multi_conf = False
