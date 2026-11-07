from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The driver builds against the sensor component.
    manifest.dependencies = manifest.dependencies + ["sensor"]
