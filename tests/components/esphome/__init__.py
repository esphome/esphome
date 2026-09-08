from esphome.loader import FileResource
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # to_code must run: it emits the component count the application needs
    manifest.enable_codegen()
    # The deflate decoder lives with the ota platform, which is not part of
    # this build; only the decoder itself is under test here
    manifest.resources = manifest.resources + [
        FileResource("esphome.components.esphome", "ota/ota_esphome_inflate.c"),
        FileResource("esphome.components.esphome", "ota/ota_esphome_inflate.h"),
    ]
