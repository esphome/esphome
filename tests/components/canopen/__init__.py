# required for cpp unit testing.
import esphome.codegen as cg
from esphome.components.canopen import add_canopennode_library
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # the gtests need CANopenNode's headers and configuration, but not a configured hub (no canbus)
    async def to_code_testing(config):
        add_canopennode_library()
        # CAN FD frames, which the hub ignores (sdo_stall_test.cpp)
        cg.add_define("USE_CANBUS_FD")

    manifest.enable_codegen()
    manifest.to_code = to_code_testing
    # MULTI_CONF components get an empty list (no to_code call) in the unit test build
    manifest.multi_conf = False
