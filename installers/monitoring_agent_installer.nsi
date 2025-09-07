; NSIS script for packing all files into one installer exe
OutFile "monitoring_agent_installer.exe"

; Default install directory
InstallDir "$PROGRAMFILES\MonitoringAgent"

; Require admin rights
RequestExecutionLevel admin

; Installer pages
Page directory
Page instfiles

Section "Install Monitoring Agent"
    SetOutPath "$INSTDIR"
    SetOverwrite on

    ; Копируем новый исполняемый файл агента
    File "monitoring_agent_new.exe"
    
    ; Копируем необходимые DLL
    File "cpr.dll"
    File "libcurl.dll"
    File "zlib.dll"
    
    ; Копируем конфигурационный файл
    File "agent_config.json"
    
    ; Копируем установщик smartmontools
    File "smartmontools-7.3-1.win32-setup.exe"

    ; Тихая установка smartmontools
    ExecWait '"$INSTDIR\smartmontools-7.3-1.win32-setup.exe" /S'

    ; Пытаемся создать задание в планировщике с правами админа
    nsExec::ExecToStack 'schtasks /Create /TN "MonitoringAgent" /TR "$INSTDIR\monitoring_agent_new.exe" /SC ONLOGON /RL HIGHEST /F'
    Pop $0
    StrCmp $0 "0" done_autorun

    ; Если не получилось, добавляем в автозагрузку пользователя
    WriteRegStr HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Run" "MonitoringAgent" '"$INSTDIR\monitoring_agent_new.exe"'

done_autorun:
    DetailPrint "Установка завершена!"
    Exec '"$INSTDIR\monitoring_agent_new.exe"'
SectionEnd