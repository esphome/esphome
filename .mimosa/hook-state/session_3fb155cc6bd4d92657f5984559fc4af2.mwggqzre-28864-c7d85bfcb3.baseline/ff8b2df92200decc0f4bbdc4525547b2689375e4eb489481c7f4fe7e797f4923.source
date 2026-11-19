import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: object) -> None:
        cg.add_define("PACKET_INTERFACE_ADDRESSING_PORT_COUNT", 4)
        cg.add_define("USE_PACKET_INTERFACE_BASIC_ADDRESSING")
        cg.add_define("USE_PACKET_INTERFACE_BASIC_FRAGMENTATION")
        cg.add_define("USE_PACKET_INTERFACE_BASIC_SESSION")
        cg.add_define("USE_PACKET_INTERFACE_HDLC")
        cg.add_define("USE_PACKET_INTERFACE_XXTEA")

    manifest.to_code = to_code_testing
