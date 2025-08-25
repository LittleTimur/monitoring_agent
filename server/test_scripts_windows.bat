@echo off
chcp 65001 >nul
echo ========================================
echo Тестирование API скриптов для Windows
echo ========================================
echo.

REM Замените на реальный ID вашего агента
set AGENT_ID=agent_1756034799529_8439
set SERVER_URL=http://localhost:8000

echo Используется агент: %AGENT_ID%
echo Сервер: %SERVER_URL%
echo.

:menu
echo Выберите тест:
echo 1. Простая команда echo
echo 2. PowerShell команда
echo 3. Системная информация
echo 4. Информация о дисках
echo 5. Сетевая информация
echo 6. Python скрипт
echo 7. Команда с параметрами
echo 8. Команда с таймаутом
echo 9. Фоновая команда
echo 10. Команда с ошибкой
echo 11. Список всех задач
echo 0. Выход
echo.
set /p choice="Введите номер теста (0-11): "

if "%choice%"=="1" goto test1
if "%choice%"=="2" goto test2
if "%choice%"=="3" goto test3
if "%choice%"=="4" goto test4
if "%choice%"=="5" goto test5
if "%choice%"=="6" goto test6
if "%choice%"=="7" goto test7
if "%choice%"=="8" goto test8
if "%choice%"=="9" goto test9
if "%choice%"=="10" goto test10
if "%choice%"=="11" goto test11
if "%choice%"=="0" goto exit
echo Неверный выбор. Попробуйте снова.
goto menu

:test1
echo.
echo ========================================
echo Тест 1: Простая команда echo
echo ========================================
curl -X POST "%SERVER_URL%/api/agents/%AGENT_ID%/run_script" -H "Content-Type: application/json" -d "{\"script\": \"echo Привет!\", \"interpreter\": \"cmd\"}"
echo.
pause
goto menu

:test2
echo.
echo ========================================
echo Тест 2: PowerShell команда
echo ========================================
curl -X POST "%SERVER_URL%/api/agents/%AGENT_ID%/run_script" -H "Content-Type: application/json" -d "{\"script\": \"Write-Host \\\"Привет из PowerShell!\\\"\", \"interpreter\": \"powershell\"}"
echo.
pause
goto menu

:test3
echo.
echo ========================================
echo Тест 3: Системная информация
echo ========================================
curl -X POST "%SERVER_URL%/api/agents/%AGENT_ID%/run_script" -H "Content-Type: application/json" -d "{\"script\": \"systeminfo ^| findstr /B /C:\\\"OS Name\\\" /C:\\\"OS Version\\\"\", \"interpreter\": \"cmd\"}"
echo.
pause
goto menu

:test4
echo.
echo ========================================
echo Тест 4: Информация о дисках
echo ========================================
curl -X POST "%SERVER_URL%/api/agents/%AGENT_ID%/run_script" -H "Content-Type: application/json" -d "{\"script\": \"wmic logicaldisk get size,freespace,caption\", \"interpreter\": \"cmd\"}"
echo.
pause
goto menu

:test5
echo.
echo ========================================
echo Тест 5: Сетевая информация
echo ========================================
curl -X POST "%SERVER_URL%/api/agents/%AGENT_ID%/run_script" -H "Content-Type: application/json" -d "{\"script\": \"ipconfig ^| findstr IPv4\", \"interpreter\": \"cmd\"}"
echo.
pause
goto menu

:test6
echo.
echo ========================================
echo Тест 6: Python скрипт
echo ========================================
curl -X POST "%SERVER_URL%/api/agents/%AGENT_ID%/run_script" -H "Content-Type: application/json" -d "{\"script\": \"print('Привет из Python!')\", \"interpreter\": \"python\"}"
echo.
pause
goto menu

:test7
echo.
echo ========================================
echo Тест 7: Команда с параметрами
echo ========================================
curl -X POST "%SERVER_URL%/api/agents/%AGENT_ID%/run_script" -H "Content-Type: application/json" -d "{\"script\": \"echo Параметр 1: %%1 ^&^& echo Параметр 2: %%2\", \"interpreter\": \"cmd\", \"args\": [\"Hello\", \"World\"]}"
echo.
pause
goto menu

:test8
echo.
echo ========================================
echo Тест 8: Команда с таймаутом
echo ========================================
curl -X POST "%SERVER_URL%/api/agents/%AGENT_ID%/run_script" -H "Content-Type: application/json" -d "{\"script\": \"ping -n 3 google.com\", \"interpreter\": \"cmd\", \"timeout_sec\": 10}"
echo.
pause
goto menu

:test9
echo.
echo ========================================
echo Тест 9: Фоновая команда
echo ========================================
curl -X POST "%SERVER_URL%/api/agents/%AGENT_ID%/run_script" -H "Content-Type: application/json" -d "{\"script\": \"start /B notepad.exe\", \"interpreter\": \"cmd\", \"capture_output\": false, \"background\": true}"
echo.
pause
goto menu

:test10
echo.
echo ========================================
echo Тест 10: Команда с ошибкой
echo ========================================
curl -X POST "%SERVER_URL%/api/agents/%AGENT_ID%/run_script" -H "Content-Type: application/json" -d "{\"script\": \"nonexistent_command\", \"interpreter\": \"cmd\"}"
echo.
pause
goto menu

:test11
echo.
echo ========================================
echo Тест 11: Список всех задач
echo ========================================
curl "%SERVER_URL%/api/agents/%AGENT_ID%/jobs"
echo.
pause
goto menu

:exit
echo Выход из программы...
exit /b 0

