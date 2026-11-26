import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The light platform declares no dependencies (light and output come from the user's
    # config), and the host unit-test build does not load the platform module, so name them here.
    manifest.dependencies = manifest.dependencies + ["light", "output"]

    # The output tests drive priority mode with weighted white channels
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_LIGHT_COLOR_TEMPERATURE_PRIORITY")
        cg.add_define("USE_LIGHT_WHITE_RELATIVE_BRIGHTNESS")

    manifest.to_code = to_code_testing
