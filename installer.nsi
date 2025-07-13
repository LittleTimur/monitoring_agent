; Имя итогового установщика
OutFile "monitoring_agent_installer.exe"

; Папка установки по умолчанию
InstallDir "$PROGRAMFILES\MonitoringAgent"

; Требуем права администратора (для записи в Program Files)
RequestExecutionLevel admin

; Страницы установщика
Page directory
Page instfiles

Section "Установка Monitoring Agent"
    ; Указываем, куда распаковывать файлы (ОБЯЗАТЕЛЬНО!)
    SetOutPath "$INSTDIR"
    
    ; Включаем перезапись файлов
    SetOverwrite on

    ; Копируем файлы в $INSTDIR
    File "monitoring_agent.exe"
    File "cpr.dll"
    File "libcurl.dll"
    File "zlib.dll"
    File "smartmontools-7.3-1.win32-setup.exe"

    ; Устанавливаем smartmontools в тихом режиме (исправлена опечатка)
    ExecWait '"$INSTDIR\smartmontools-7.3-1.win32-setup.exe" /S'

    ; Добавляем Monitoring Agent в автозагрузку (исправлен путь реестра)
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Run" "MonitoringAgent" '"$INSTDIR\monitoring_agent.exe"'

    ; Создаем ярлык в меню Пуск
    CreateDirectory "$SMPROGRAMS\MonitoringAgent"
    CreateShortCut "$SMPROGRAMS\MonitoringAgent\MonitoringAgent.lnk" "$INSTDIR\monitoring_agent.exe"
    CreateShortCut "$SMPROGRAMS\MonitoringAgent\Uninstall.lnk" "$INSTDIR\uninstall.exe"

    ; Создаем ярлык на рабочем столе
    CreateShortCut "$DESKTOP\MonitoringAgent.lnk" "$INSTDIR\monitoring_agent.exe"

    ; Записываем информацию для удаления
    WriteUninstaller "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MonitoringAgent" "DisplayName" "Monitoring Agent"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MonitoringAgent" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MonitoringAgent" "DisplayIcon" "$INSTDIR\monitoring_agent.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MonitoringAgent" "Publisher" "Your Company"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MonitoringAgent" "DisplayVersion" "1.0"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MonitoringAgent" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MonitoringAgent" "NoRepair" 1

    ; Сообщение об успехе
    DetailPrint "Установка завершена!"

    ; Запускаем агент после установки
    Exec '"$INSTDIR\monitoring_agent.exe"'
SectionEnd

Section "Uninstall"
    ; Удаляем из автозагрузки
    DeleteRegValue HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Run" "MonitoringAgent"
    
    ; Удаляем файлы
    Delete "$INSTDIR\monitoring_agent.exe"
    Delete "$INSTDIR\cpr.dll"
    Delete "$INSTDIR\libcurl.dll"
    Delete "$INSTDIR\zlib.dll"
    Delete "$INSTDIR\smartmontools-7.3-1.win32-setup.exe"
    
    ; Удаляем ярлыки
    Delete "$SMPROGRAMS\MonitoringAgent\MonitoringAgent.lnk"
    Delete "$SMPROGRAMS\MonitoringAgent\Uninstall.lnk"
    RMDir "$SMPROGRAMS\MonitoringAgent"
    Delete "$DESKTOP\MonitoringAgent.lnk"
    
    ; Удаляем папку установки
    RMDir "$INSTDIR"
    
    ; Удаляем информацию из реестра
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MonitoringAgent"
SectionEnd