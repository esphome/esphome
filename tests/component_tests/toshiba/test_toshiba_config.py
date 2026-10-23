"""Tests for Toshiba model selection and the RAS-25J2KVSG-ND class binding."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.toshiba import climate
from esphome.config import read_config
from esphome.const import CONF_ID, CONF_MODEL, PlatformFramework
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize("model", list(climate.MODELS))
def test_model_schema(model: str, set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    config = climate.CONFIG_SCHEMA({"name": "Toshiba", CONF_MODEL: model.lower()})
    assert config[CONF_MODEL] == model
    expected = (
        climate.ToshibaRas25j2kvsgNdClimate
        if model == climate.MODEL_RAS_25J2KVSG_ND
        else climate.ToshibaClimate
    )
    assert config[CONF_ID].type is expected


def test_default_model_is_unchanged(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    config = climate.CONFIG_SCHEMA({"name": "Toshiba"})
    assert config[CONF_MODEL] == "GENERIC"
    assert config[CONF_ID].type is climate.ToshibaClimate


def test_ras_25j2kvsg_nd_uses_its_class(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("ras_25j2kvsg_nd.yaml"))
    assert "new(ras) toshiba::ToshibaRas25j2kvsgNdClimate();" in main_cpp
    assert "new(generic) toshiba::ToshibaClimate();" in main_cpp
    assert "generic->set_model(toshiba::MODEL_GENERIC);" in main_cpp
    assert "ras->set_model(" not in main_cpp
    assert "USE_TOSHIBA_RAS_25J2KVSG_ND" in {d.name for d in CORE.defines}


@pytest.mark.parametrize("file_name", ["select_on_generic.yaml", "fix_on_generic.yaml"])
def test_ras_25j2kvsg_nd_features_reject_other_models(
    file_name: str,
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = component_config_path(file_name)
    assert read_config({}) is None
    assert "ID 'generic' of type toshiba::ToshibaClimate doesn't inherit from" in (
        capsys.readouterr().out
    )
