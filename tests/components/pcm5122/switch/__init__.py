from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Builds the power switch platform against the DAC.
    manifest.dependencies = manifest.dependencies + ["pcm5122"]
