"""Unit tests for esphome.components.zephyr.variants: framework: type: resolution."""

from __future__ import annotations

import pytest

from esphome.components.zephyr.const import VERSION_RECOMMENDED
from esphome.components.zephyr.variants import (
    ZephyrModule,
    ZephyrModuleTemplate,
    ZephyrSDK,
    ZephyrVariant,
    resolve_framework_version,
    resolve_sdk,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_FRAMEWORK,
    CONF_REFRESH,
    CONF_SOURCE,
    CONF_TYPE,
    CONF_VERSION,
)
from esphome.core import EsphomeError

_DEFAULT_SDK = ZephyrSDK(
    manifest_url="http://dummy-default",
    default_version="1.0.0",
    min_version=cv.Version(1, 0, 0),
)
_ALT_SDK = ZephyrSDK(
    manifest_url="http://dummy-alt",
    default_version="2.0.0",
    min_version=cv.Version(2, 0, 0),
)


def _fake_variant() -> ZephyrVariant:
    return ZephyrVariant(
        sdk=_DEFAULT_SDK,
        sdk_name="default",
        alt_sdks={"alt": _ALT_SDK},
    )


def _framework_config(**overrides) -> dict:
    # Mirrors _FRAMEWORK_SCHEMA's post-validation shape: refresh always defaults to
    # "1d", every other key is only present when explicitly set.
    return {CONF_REFRESH: "1d", **overrides}


# ---------------------------------------------------------------------------
# resolve_sdk
# ---------------------------------------------------------------------------


def test_resolve_sdk_none_type_returns_variant_default() -> None:
    variant = _fake_variant()
    assert resolve_sdk(variant, None) == ("default", _DEFAULT_SDK)


def test_resolve_sdk_explicit_default_type_returns_variant_default() -> None:
    variant = _fake_variant()
    assert resolve_sdk(variant, "default") == ("default", _DEFAULT_SDK)


def test_resolve_sdk_alt_type_returns_alt_sdk() -> None:
    variant = _fake_variant()
    assert resolve_sdk(variant, "alt") == ("alt", _ALT_SDK)


def test_resolve_sdk_unknown_type_raises_keyerror() -> None:
    variant = _fake_variant()
    with pytest.raises(KeyError):
        resolve_sdk(variant, "bogus")


# ---------------------------------------------------------------------------
# resolve_framework_version
# ---------------------------------------------------------------------------


def test_resolve_framework_version_defaults_to_variant_default_sdk() -> None:
    variant = _fake_variant()
    config = {CONF_FRAMEWORK: _framework_config()}

    version_str, parsed, sdk_name, sdk = resolve_framework_version(
        variant, "fake", config, "test"
    )

    assert version_str == "1.0.0"
    assert parsed == cv.Version(1, 0, 0)
    assert sdk_name == "default"
    assert sdk is _DEFAULT_SDK


def test_resolve_framework_version_rejects_unknown_type() -> None:
    variant = _fake_variant()
    config = {CONF_FRAMEWORK: _framework_config(**{CONF_TYPE: "bogus"})}

    with pytest.raises(cv.Invalid, match="not a valid framework type"):
        resolve_framework_version(variant, "fake", config, "test")


def test_resolve_framework_version_selects_alt_sdk_type() -> None:
    variant = _fake_variant()
    config = {CONF_FRAMEWORK: _framework_config(**{CONF_TYPE: "alt"})}

    version_str, parsed, sdk_name, sdk = resolve_framework_version(
        variant, "fake", config, "test"
    )

    # The alt sdk's own default_version, not the variant's default sdk's.
    assert version_str == "2.0.0"
    assert parsed == cv.Version(2, 0, 0)
    assert sdk_name == "alt"
    assert sdk is _ALT_SDK


def test_resolve_framework_version_rejects_version_and_source_together() -> None:
    variant = _fake_variant()
    config = {
        CONF_FRAMEWORK: _framework_config(
            **{
                CONF_VERSION: "1.0.0",
                CONF_SOURCE: {CONF_TYPE: "local", "path": "/dummy"},
            }
        )
    }

    with pytest.raises(cv.Invalid, match="mutually exclusive"):
        resolve_framework_version(variant, "fake", config, "test")


def test_resolve_framework_version_rejects_version_below_min() -> None:
    variant = _fake_variant()
    config = {CONF_FRAMEWORK: _framework_config(**{CONF_VERSION: "0.5.0"})}

    with pytest.raises(cv.Invalid, match=">= 1.0.0"):
        resolve_framework_version(variant, "fake", config, "test")


def test_resolve_framework_version_accepts_recommended_literal() -> None:
    variant = _fake_variant()
    config = {CONF_FRAMEWORK: _framework_config(**{CONF_VERSION: VERSION_RECOMMENDED})}

    version_str, parsed, sdk_name, sdk = resolve_framework_version(
        variant, "fake", config, "test"
    )

    assert version_str == "1.0.0"
    assert sdk_name == "default"


def test_resolve_framework_version_accepts_explicit_non_default_version(
    caplog: pytest.LogCaptureFixture,
) -> None:
    variant = _fake_variant()
    config = {CONF_FRAMEWORK: _framework_config(**{CONF_VERSION: "1.2.0"})}

    version_str, parsed, sdk_name, sdk = resolve_framework_version(
        variant, "fake", config, "test"
    )

    assert version_str == "1.2.0"
    assert parsed == cv.Version(1, 2, 0)
    assert "not the recommended one" in caplog.text


# ---------------------------------------------------------------------------
# ZephyrModuleTemplate.resolve
# ---------------------------------------------------------------------------

_MODULE_TEMPLATE = ZephyrModuleTemplate(
    name="dummy-module",
    manifest_url="http://dummy-module",
    default_versions={(3, 4): "1.4.0"},
)


def test_module_template_resolve_uses_matching_default_version() -> None:
    module = _MODULE_TEMPLATE.resolve(cv.Version(3, 4, 2))

    assert module == ZephyrModule(
        name="dummy-module", manifest_url="http://dummy-module", revision="v1.4.0"
    )


def test_module_template_resolve_ignores_root_patch_version() -> None:
    # (major, minor) match only -- root_version's own patch (here .99) doesn't need a
    # corresponding entry.
    module = _MODULE_TEMPLATE.resolve(cv.Version(3, 4, 99))

    assert module.revision == "v1.4.0"


def test_module_template_resolve_explicit_version_wins_over_table() -> None:
    module = _MODULE_TEMPLATE.resolve(cv.Version(3, 4, 0), explicit_version="9.9.9")

    assert module.revision == "v9.9.9"


def test_module_template_resolve_explicit_version_fills_table_gap() -> None:
    # No (3, 9) entry -- would otherwise raise; an explicit version bypasses the
    # table lookup entirely.
    module = _MODULE_TEMPLATE.resolve(cv.Version(3, 9, 0), explicit_version="1.9.9")

    assert module.revision == "v1.9.9"


def test_module_template_resolve_raises_without_table_match_or_override() -> None:
    with pytest.raises(EsphomeError, match="no known compatible version"):
        _MODULE_TEMPLATE.resolve(cv.Version(3, 9, 0))


_VERSIONED_MODULE_TEMPLATE = ZephyrModuleTemplate(
    name="dummy-module",
    manifest_url="http://dummy-module",
    default_versions={(3, 4): "1.4.0"},
    min_version=cv.Version(1, 4, 0),
)


def test_module_template_resolve_rejects_override_below_min_version() -> None:
    with pytest.raises(EsphomeError, match="requires version >= 1.4.0"):
        _VERSIONED_MODULE_TEMPLATE.resolve(
            cv.Version(3, 4, 0), explicit_version="1.0.0"
        )


def test_module_template_resolve_accepts_override_at_min_version() -> None:
    module = _VERSIONED_MODULE_TEMPLATE.resolve(
        cv.Version(3, 4, 0), explicit_version="1.4.0"
    )
    assert module.revision == "v1.4.0"


def test_module_template_resolve_ignores_min_version_for_non_semver_ref() -> None:
    # A git branch/ref (e.g. "main") isn't a version at all -- nothing to compare.
    module = _VERSIONED_MODULE_TEMPLATE.resolve(
        cv.Version(3, 4, 0), explicit_version="main"
    )
    assert module.revision == "main"
