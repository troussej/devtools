rem unset proxy

reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" ^
    /v AutoConfigURL /f

reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" ^
    /v ProxyEnable /t REG_DWORD /d 0 /f