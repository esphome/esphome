rem python 3.11.x is required to prevent platformio littlefs errors in later versions.

rem Define the Python executable to use.
rem - If Python 3.11 is in your PATH, leave as 'python'
rem - If you need to point to a specific installation, use the full path, e.g.:

rem set PYTHON_EXE="C:\Program Files\Python311\python.exe"
set PYTHON_EXE=python


rem Check that the defined Python exists
%PYTHON_EXE% --version >nul 2>&1
if errorlevel 1 (
    echo Error: The defined Python executable was not found.
    echo PYTHON_EXE: %PYTHON_EXE%
    echo Please ensure Python 3.11.x is installed and either in PATH or set the full path in PYTHON_EXE.
    pause
    exit /b 1
)

rem Get the version
for /f "tokens=2" %%v in ('%PYTHON_EXE% --version 2^>^&1') do set PYVER=%%v

rem Extract major.minor (e.g., 3.11 from 3.11.9)
set MAJOR_MINOR=%PYVER:~0,5%

if not "%MAJOR_MINOR%" == "3.11." (
    echo Error: Incorrect Python version detected.
    echo PYTHON_EXE:     %PYTHON_EXE%
    echo Python version: %PYVER%
    echo .
    echo Please ensure Python 3.11.x is installed and either in PATH or set the full path in PYTHON_EXE.
    pause
    exit /b 1
)

if defined VIRTUAL_ENV goto :install

echo Starting the Virtual Environment
rem Use specified python version to create virtual environment
%PYTHON_EXE% -m venv venv
call venv/Scripts/activate
echo Running the Virtual Environment

:install

echo Installing required packages...

rem At this point, we're in the virtual environment, so the python version 'should' be correct.
python.exe -m pip install --upgrade pip

pip3 install -r requirements.txt -r requirements_test.txt -r requirements_dev.txt
pip3 install setuptools wheel
pip3 install -e ".[dev,test]" --config-settings editable_mode=compat

pre-commit install

echo .
echo .
echo Virtual environment created. Run 'venv/Scripts/activate' to use it.
