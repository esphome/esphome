from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The light platform declares no dependencies (light and output come from the user's
    # config), and the host unit-test build does not load the platform module, so name them here.
    manifest.dependencies = manifest.dependencies + ["light", "output"]
