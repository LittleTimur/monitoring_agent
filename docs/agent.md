# Агент мониторинга

Кроссплатформенный агент мониторинга системы, написанный на C++. Собирает метрики CPU, памяти, дисков, сети, GPU и HDD.

## 🏗️ Архитектура агента

Агент состоит из следующих компонентов:

- **MetricsCollector** - сбор метрик системы
- **AgentManager** - управление жизненным циклом агента
- **ServerClient** - отправка данных на сервер
- **ScriptExecutor** - выполнение пользовательских скриптов
- **AgentConfig** - конфигурация агента

## 📊 Собираемые метрики

### CPU
- Загрузка процессора (общая и по ядрам)
- Температура процессора
- Частота процессора
- Информация о ядрах

### Память
- Общий объем RAM
- Использованная память
- Свободная память
- Процент использования
- Swap память

### Диски
- Список разделов
- Файловые системы
- Общий объем
- Использованное место
- Свободное место
- Процент использования

### Сеть
- Список сетевых интерфейсов
- Статистика по интерфейсам (байты, пакеты)
- Скорость передачи
- Активные соединения

### GPU
- Температура GPU
- Загрузка GPU
- Использование памяти GPU
- Информация о драйверах

### HDD
- Температура дисков
- Время работы
- Статус здоровья (S.M.A.R.T.)
- Скорость вращения

### Инвентарь
- Производитель и модель
- Серийный номер
- UUID системы
- Информация об ОС
- Список установленного ПО

## 🔨 Сборка агента

### Требования

**Общие:**
- CMake ≥ 3.15
- C++17 компилятор
- Git

**Windows:**
- Visual Studio 2019+ (Desktop development with C++)
- CMake
- Git

**Linux:**
- GCC ≥ 7.0 или Clang ≥ 6.0
- libcurl4-openssl-dev
- pkg-config
- make

### Установка зависимостей

**Windows (PowerShell):**
```powershell
# Установка Chocolatey
Set-ExecutionPolicy Bypass -Scope Process -Force
[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))

# Установка инструментов
choco install cmake git
```

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y build-essential cmake git pkg-config libcurl4-openssl-dev
```

**RHEL/CentOS/Fedora:**
```bash
sudo yum groupinstall "Development Tools"
sudo yum install cmake git pkg-config libcurl-devel
```

### Процесс сборки

**Windows:**
```powershell
git clone <repository-url>
cd monitoring_agent
mkdir build
cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release
```

**Linux:**
```bash
git clone <repository-url>
cd monitoring_agent
mkdir build && cd build
cmake ..
make -j$(nproc)
```

**Результат сборки:**
- Windows: `build/bin/Release/monitoring_agent.exe`
- Linux: `build/bin/Release/monitoring_agent`

## ⚙️ Конфигурация агента

Агент использует файл `agent_config.json` для настройки:

```json
{
  "agent_id": "unique_agent_id",
  "machine_name": "My Computer",
  "auto_detect_id": true,
  "auto_detect_name": true,
  "command_server_host": "0.0.0.0",
  "command_server_port": 8081,
  "command_server_url": null,
  "server_url": "http://localhost:8000",
  "scripts_dir": "scripts",
  "audit_log_enabled": false,
  "audit_log_path": null,
  "enable_inline_commands": true,
  "enable_user_parameters": true,
  "job_retention_seconds": 3600,
  "max_buffer_size": 10,
  "max_concurrent_jobs": 3,
  "max_output_bytes": 1000000,
  "max_script_timeout_sec": 60,
  "send_timeout_ms": 2000,
  "update_frequency": 60
}
```

### Параметры конфигурации

| Параметр | Описание | По умолчанию |
|----------|----------|--------------|
| `agent_id` | Уникальный идентификатор агента | Обязательный |
| `machine_name` | Имя машины | Обязательный |
| `auto_detect_id` | Автоматическое определение ID | `true` |
| `auto_detect_name` | Автоматическое определение имени | `true` |
| `command_server_host` | IP для прослушивания команд | `"0.0.0.0"` |
| `command_server_port` | Порт для прослушивания команд | `8081` |
| `server_url` | URL центрального сервера | Обязательный |
| `scripts_dir` | Директория скриптов | `"scripts"` |
| `audit_log_enabled` | Включено логирование | `false` |
| `audit_log_path` | Путь к логу | `null` |
| `enable_inline_commands` | Разрешены inline-команды | `true` |
| `enable_user_parameters` | Разрешены пользовательские параметры | `true` |
| `job_retention_seconds` | Время хранения результатов | `3600` |
| `max_buffer_size` | Макс. размер буфера | `10` |
| `max_concurrent_jobs` | Макс. число одновременных задач | `3` |
| `max_output_bytes` | Макс. размер вывода | `1000000` |
| `max_script_timeout_sec` | Макс. время выполнения скрипта | `60` |
| `send_timeout_ms` | Таймаут отправки | `2000` |
| `update_frequency` | Частота обновления (секунды) | `60` |

## 🚀 Запуск агента

### Windows

**Простой запуск:**
```powershell
.\monitoring_agent.exe
```

**Запуск в фоне:**
```powershell
Start-Process -FilePath ".\monitoring_agent.exe" -WindowStyle Hidden
```

**Запуск как служба:**
```powershell
# Создание службы Windows
sc create "MonitoringAgent" binPath="C:\path\to\monitoring_agent.exe" start=auto
sc start "MonitoringAgent"
```

### Linux

**Простой запуск:**
```bash
./monitoring_agent
```

**Запуск в фоне:**
```bash
nohup ./monitoring_agent > metrics.log 2>&1 &
```

**Запуск как служба systemd:**
```bash
# Копирование бинарника
sudo cp monitoring_agent /usr/local/bin/

# Создание службы
sudo tee /etc/systemd/system/monitoring-agent.service << EOF
[Unit]
Description=Monitoring Agent
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/monitoring_agent
Restart=always
User=root

[Install]
WantedBy=multi-user.target
EOF

# Запуск службы
sudo systemctl daemon-reload
sudo systemctl enable monitoring-agent
sudo systemctl start monitoring-agent
```

## 🔌 API агента

Агент предоставляет HTTP API для взаимодействия с сервером:

### Регистрация агента
```http
POST /api/v1/agents
Content-Type: application/json

{
  "agent_id": "unique_agent_id",
  "machine_name": "My Computer",
  "server_url": "http://localhost:8000"
}
```

### Отправка метрик
```http
POST /api/v1/agents/{agent_id}/metrics
Content-Type: application/json

{
  "machine_type": "physical",
  "machine_name": "My Computer",
  "metric_type": "cpu",
  "usage_percent": 75.5,
  "details": {
    "cores": 4,
    "load_avg": 1.2
  }
}
```

### Обновление heartbeat
```http
PUT /api/v1/agents/{agent_id}/heartbeat
```

### Получение команд
```http
GET /api/v1/agents/{agent_id}/commands
```

## 📝 Логирование

Агент ведет логи в следующих форматах:

**Консольный вывод:**
```
[INFO] Agent started: agent_001
[INFO] Collecting CPU metrics...
[INFO] Sending metrics to server...
[ERROR] Failed to connect to server: Connection refused
```

**Файл логов (если включен):**
```
2025-09-06 10:30:15 [INFO] Agent started: agent_001
2025-09-06 10:30:16 [INFO] Collecting CPU metrics...
2025-09-06 10:30:17 [INFO] Sending metrics to server...
```

## 🔧 Требования к системе

### Минимальные требования
- **RAM:** 64 MB
- **CPU:** 1 ядро
- **Диск:** 10 MB для бинарника
- **Сеть:** HTTP/HTTPS доступ к серверу

### Опциональные утилиты

**Для HDD метрик:**
- `smartctl` (smartmontools)

**Для GPU метрик:**
- `nvidia-smi` (NVIDIA GPU)
- `rocm-smi` (AMD GPU)

**Для инвентарных данных:**
- `lspci` (информация о PCI устройствах)
- `ip` (сетевые адреса)
- `dpkg`/`rpm` (список ПО)

## 🐛 Устранение неполадок

### Агент не запускается
1. Проверьте права на выполнение: `chmod +x monitoring_agent`
2. Убедитесь, что все библиотеки доступны
3. Проверьте конфигурационный файл

### Метрики не отправляются
1. Проверьте подключение к серверу: `curl http://server:8000/`
2. Убедитесь, что сервер запущен
3. Проверьте настройки firewall

### HDD метрики не собираются
1. Установите smartmontools: `sudo apt install smartmontools`
2. Убедитесь в правах доступа к S.M.A.R.T. данным
3. Запустите с правами администратора

### GPU метрики не собираются
1. Установите драйверы GPU
2. Убедитесь, что nvidia-smi/rocm-smi доступны
3. Проверьте права доступа к GPU

## 📚 Дополнительные ресурсы

- [Сервер мониторинга](server.md)
- [База данных](database.md)
- [Развертывание](deployment.md)
