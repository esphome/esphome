@echo off
rem Set up ESPHome dev environment

setlocal enabledelayedexpansion

rem Navigate to the script's parent directory
cd "%~dp0\.."

set location=venv\Scripts\activate

rem Check if not in a dev container, virtual environment, or if ESPHOME_NO_VENV is not set
if not defined DEVCONTAINER if not defined VIRTUAL_ENV if not defined ESPHOME_NO_VENV (
    python -m venv venv
    if exist venv\Scripts\activate (
        set location=venv\Scripts\activate
    )
    call %location%
)

pip install -r requirements.txt -r requirements_optional.txt -r requirements_test.txt -r requirements_dev.txt
pip install setuptools wheel
pip install -e .[dev,test,displays] --config-settings editable_mode=compat

pre-commit install

python script\platformio_install_deps.py platformio.ini --libraries --tools --platforms

echo.
echo.
echo Virtual environment created. Run 'call %location%' to use it.

endlocal
