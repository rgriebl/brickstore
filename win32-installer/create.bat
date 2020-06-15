@echo off

"C:\Program Files (x86)\WiX Toolset v3.8\bin\candle.exe" -out win32-installer\ -dVERSION=%1 -dTARGET=%cd% -dQTDIR=C:\Qt\Static\4.8.5 -ext WixUIExtension -ext WixUtilExtension win32-installer\brickstock.wxs
"C:\Program Files (x86)\WiX Toolset v3.8\bin\light.exe" -out win32-installer\brickstock.msi -ext WixUIExtension -ext WixUtilExtension win32-installer\brickstock.wixobj