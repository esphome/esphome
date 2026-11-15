from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # AUTO_LOAD sits on the climate platform, which the unit test build does not load.
    manifest.dependencies = manifest.dependencies + ["climate_ir", "climate"]
