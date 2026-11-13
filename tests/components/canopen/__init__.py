# required for cpp unit testing.
from esphome.components.canopen import add_canopennode_library
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # the gtests need CANopenNode's headers and configuration, but not a configured hub (no canbus)
    async def to_code_testing(config):
        add_canopennode_library()

    manifest.enable_codegen()
    manifest.to_code = to_code_testing
    # MULTI_CONF components get an empty list (no to_code call) in the unit test build
    manifest.multi_conf = False
