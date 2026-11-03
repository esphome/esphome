from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The frame tests call the decoder directly; no bus is driven
    manifest.dependencies = manifest.dependencies + ["spi"]
