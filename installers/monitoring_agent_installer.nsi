; NSIS script for packing all files into one installer exe
OutFile "monitoring_agent_installer.exe"

; Default install directory
InstallDir "$PROGRAMFILES\MonitoringAgent"

; Require admin rights
RequestExecutionLevel admin

; Installer pages
Page directory
Page custom ServerUrlPage ServerUrlPageLeave
Page instfiles

!include nsDialogs.nsh
Var SERVER_URL

Function ServerUrlPage
    nsDialogs::Create 1018
    Pop $0

    ${NSD_CreateLabel} 0 0 100% 12u "Enter the server URL for sending metrics:"
    Pop $1

    ${NSD_CreateText} 0 14u 100% 12u "http://localhost:8080/metrics"
    Pop $SERVER_URL

    nsDialogs::Show
FunctionEnd

Function ServerUrlPageLeave
    ${NSD_GetText} $SERVER_URL $SERVER_URL
FunctionEnd

Section "Install Monitoring Agent"
    SetOutPath "$INSTDIR"
    SetOverwrite on

    File "monitoring_agent.exe"
    File "cpr.dll"
    File "libcurl.dll"
    File "zlib.dll"
    File "smartmontools-7.3-1.win32-setup.exe"

    ; Создаём config.json с URL сервера
    FileOpen $0 "$INSTDIR\config.json" w
    FileWrite $0 "{$\r$\n"
    FileWrite $0 "  $\"server_url$\": $\"$SERVER_URL$\"$\r$\n"
    FileWrite $0 "}"
    FileClose $0

    ; Тихая установка smartmontools
    ExecWait '"$INSTDIR\smartmontools-7.3-1.win32-setup.exe" /S'

    ; Пытаемся создать задание в планировщике с правами админа
    nsExec::ExecToStack 'schtasks /Create /TN "MonitoringAgent" /TR "$INSTDIR\monitoring_agent.exe" /SC ONLOGON /RL HIGHEST /F'
    Pop $0
    StrCmp $0 "0" done_autorun

    ; Если не получилось, добавляем в автозагрузку пользователя
    WriteRegStr HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Run" "MonitoringAgent" '"$INSTDIR\monitoring_agent.exe"'

done_autorun:
    DetailPrint "Установка завершена!"
    Exec '"$INSTDIR\monitoring_agent.exe"'
SectionEnd