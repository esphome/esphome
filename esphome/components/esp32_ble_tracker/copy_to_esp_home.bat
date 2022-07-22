set src_folder="C:\Users\Jonathan\esphome\esphome\esphome\components\esp32_ble_tracker"
set dest_folder="C:\Users\Jonathan\AppData\Local\Programs\Python\Python39\Lib\site-packages\esphome\components\esp32_ble_tracker"
cd /d %dest_folder%
for /F "delims=" %%i in ('dir /b') do (rmdir "%%i" /s/q || del "%%i" /s/q)
xcopy /s %src_folder% %dest_folder%
