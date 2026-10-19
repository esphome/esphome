from typing import Any

from esphome.loader import ComponentManifest, FileResource, _replace_component_manifest
from esphome.types import ConfigType


class ComponentManifestOverride:
    """Mutable wrapper around ComponentManifest for test-specific attribute overrides.

    When ``tests/components/<name>/__init__.py`` defines::

        def override_manifest(manifest: ComponentManifestOverride) -> None:
            ...

    the function receives an instance of this class wrapping the real component
    manifest.  Any attribute assignment stores an override; reads fall back to
    the underlying ``ComponentManifest`` when no override has been set.

    Example::

        def override_manifest(manifest: ComponentManifestOverride) -> None:
            async def to_code_testing(config):
                pass  # lightweight no-op stub for C++ unit tests

            manifest.to_code = to_code_testing
            manifest.dependencies = manifest.dependencies + ["extra_dep_for_tests"]
    """

    def __init__(self, wrapped: "ComponentManifest") -> None:
        object.__setattr__(self, "_wrapped", wrapped)
        object.__setattr__(self, "_overrides", {})
        object.__setattr__(self, "_extra_resources", [])

    def __getattr__(self, name: str) -> Any:
        overrides: dict[str, Any] = object.__getattribute__(self, "_overrides")
        if name in overrides:
            return overrides[name]
        wrapped: ComponentManifest = object.__getattribute__(self, "_wrapped")
        if name == "resources":
            # Read lazily so FILTER_SOURCE_FILES sees the defines to_code added
            return wrapped.resources + object.__getattribute__(self, "_extra_resources")
        return getattr(wrapped, name)

    def __setattr__(self, name: str, value: Any) -> None:
        overrides: dict[str, Any] = object.__getattribute__(self, "_overrides")
        overrides[name] = value

    def enable_codegen(self) -> None:
        """Remove the to_code suppression, re-enabling code generation for this component.

        Call this from ``override_manifest`` when the component needs its real (or a
        custom stub) ``to_code`` to run during C++ unit test builds.
        """
        overrides: dict[str, Any] = object.__getattribute__(self, "_overrides")
        # Keep a custom to_code another override of the same manifest installed
        if overrides.get("to_code") is None:
            overrides.pop("to_code", None)

    def add_resources(self, *resources: FileResource) -> None:
        """Copy extra source files into the build next to the component's own."""
        object.__getattribute__(self, "_extra_resources").extend(resources)

    def restore(self) -> None:
        """Clear all overrides and added resources, reverting to the wrapped manifest's values."""
        object.__getattribute__(self, "_overrides").clear()
        object.__getattribute__(self, "_extra_resources").clear()


def set_testing_manifest(domain: str, manifest: ComponentManifestOverride) -> None:
    """Install a testing manifest override into the component cache.

    Called from the C++ unit test infrastructure when a component's test
    directory provides an ``override_manifest`` function.
    """
    _replace_component_manifest(domain, manifest)


def add_defines_to_code(manifest: ComponentManifestOverride, *defines: str) -> None:
    """Keep the component's real ``to_code`` and add the given defines after it.

    For host test and benchmark builds of code that is gated on a platform
    define, such as loops that only exist on boards with PSRAM.
    """
    from esphome import codegen as cg

    manifest.enable_codegen()
    original_to_code = manifest.to_code

    async def to_code(config: ConfigType) -> None:
        await original_to_code(config)
        for define in defines:
            cg.add_define(define)

    if hasattr(original_to_code, "priority"):
        to_code.priority = original_to_code.priority
    manifest.to_code = to_code
