@echo off

if defined VIRTUAL_ENV goto :install

echo Starting the Virtual Environment
python -m venv venv
call venv/Scripts/activate
echo Running the Virtual Environment

:install

echo Installing required packages...

python.exe -m pip install --upgrade pip

pip3 install -r requirements.txt -r requirements_test.txt -r requirements_dev.txt
pip3 install setuptools wheel
pip3 install -e ".[dev,test]" --config-settings editable_mode=compat

rem --overwrite replaces any hook already in place. Without it, prek finds a
rem previously installed pre-commit hook, moves it aside to
rem .git/hooks/pre-commit.legacy and keeps calling it, so every commit would
rem run both tools.
prek install --overwrite

echo .
echo .
echo Virtual environment created. Run 'venv/Scripts/activate' to use it.
