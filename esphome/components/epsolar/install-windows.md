# Install Esphome on Windows 10 from scratch

You need a working Python 3 setup on your machine. 
You can either install the package from python.org https://www.python.org/downloads/ or using powershell 

## Steps for powershell

Open Powershell as an administrator: right click Windows Start Button - Windows Powershell (admin) 

Install choco
````
Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))
````
Install Python and git using choco
````
choco install -y python3
choco install -y git.install
````
close the admin powershell window and open it again
Create directory (name doesn't matter but avoid spaces in the path to make things easier) 

````
mkdir \esphome-dev
cd \esphome-dev
````
Now install the forked version of esphome 

````
git clone https://github.com/martgras/esphome.git
cd esphome/
pip3 install -r requirements.txt -r requirements_test.txt
python -m pip install --upgrade pip
pip3 install -e .
````

Close powershell and open a new powershell window as a standard user
Copy your configuration file to \esphome-dev (in my example solarpanel.yaml)

````
## Build
esphome ..\solarpanel.yaml compile 
## Upload to esp32 
esphome ..\solarpanel.yaml run
