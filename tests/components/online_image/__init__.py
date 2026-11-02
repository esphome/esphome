from esphome.components.runtime_image import get_format
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The real to_code needs an image config. The unit tests decode BMP and PNG, and use
    # JPEG for the download buffer resize, so compile in those three decoders.
    async def to_code_testing(config: ConfigType) -> None:
        get_format("BMP").actions()
        get_format("JPEG").actions()
        get_format("PNG").actions()

    manifest.to_code = to_code_testing
