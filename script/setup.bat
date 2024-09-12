@echo off

if defined DEVCONTAINER goto :install
if defined VIRTUAL_ENV goto :install
if defined ESPHOME_NO_VENV goto :install

echo Starting the Virtual Environment
python -m venv venv
call venv/Scripts/activate
echo Running the Virtual Environment

:install

echo Installing required packages...

python.exe -m pip install --upgrade pip

pip3 install -r requirements.txt -r requirements_optional.txt -r requirements_test.txt -r requirements_dev.txt
pip3 install setuptools wheel
pip3 install -e ".[dev,test,displays]" --config-settings editable_mode=compat

pre-commit install

python script/platformio_install_deps.py platformio.ini --libraries --tools --platforms

echo .
echo .
echo Virtual environment created. Run 'venv/Scripts/activate' to use it.
