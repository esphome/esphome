from esphome.loader import FileResource
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # to_code emits the component count the application needs
    manifest.enable_codegen()
    # Only the decoder is under test; its ota platform is not in this build
    manifest.resources = manifest.resources + [
        FileResource("esphome.components.esphome", "ota/ota_esphome_inflate.c"),
        FileResource("esphome.components.esphome", "ota/ota_esphome_inflate.h"),
    ]
