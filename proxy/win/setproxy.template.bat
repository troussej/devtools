rem Set proxy

reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" ^
    /v AutoConfigURL /f

reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" ^
    /v ProxyEnable /t REG_DWORD /d 1 /f


reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" ^
    /v ProxyServer /t REG_SZ /d domain:port /f

reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" ^
    /v ProxyOverride /t REG_SZ /d localhost;127.0.0.1;domain.* /f
