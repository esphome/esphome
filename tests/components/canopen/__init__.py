from esphome.components.canopen import add_canopennode_library
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # CANopenNode's headers and flags only: there is no canbus for a hub
    async def to_code_testing(config: ConfigType) -> None:
        add_canopennode_library()

    manifest.enable_codegen()
    manifest.to_code = to_code_testing
    # a MULTI_CONF component gets no to_code call in the unit test build
    manifest.multi_conf = False
