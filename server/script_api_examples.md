# Примеры API запросов для работы со скриптами

## Базовый URL
```
http://localhost:8000/api/agents/{agent_id}/run_script
```

## 📁 Пути к файлам

**Важно:** При использовании `script_path` указывайте **относительные пути** от папки `scripts`, которая находится рядом с агентом.

- ✅ **Правильно:** `"script_path": "hello.bat"`
- ❌ **Неправильно:** `"script_path": "C:\\scripts\\hello.bat"`

Агент автоматически найдет папку `scripts` рядом с исполняемым файлом.

## 1. Выполнение встроенного скрипта

### Простая команда echo через cmd
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "echo Привет!",
    "interpreter": "cmd"
  }'
```

### Команда через PowerShell
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "Write-Host \"Привет!\"",
    "interpreter": "powershell"
  }'
```

### Команда через bash (Linux)
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "echo \"Привет!\"",
    "interpreter": "bash"
  }'
```

### Python скрипт
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "print(\"Привет!\")\nprint(\"Текущее время:\", __import__(\"datetime\").datetime.now())",
    "interpreter": "python"
  }'
```

## 2. Выполнение скрипта с параметрами

### CMD с параметрами
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "echo Параметр 1: %1 && echo Параметр 2: %2",
    "interpreter": "cmd",
    "args": ["Hello", "World"]
  }'
```

### PowerShell с параметрами
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "param($param1, $param2)\nWrite-Host \"Параметр 1: $param1\"\nWrite-Host \"Параметр 2: $param2\"",
    "interpreter": "powershell",
    "args": ["Hello", "World"]
  }'
```

### Python с параметрами
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "import sys\nprint(f\"Аргументы: {sys.argv[1:]}\")\nprint(f\"Количество аргументов: {len(sys.argv)-1}\")",
    "interpreter": "python",
    "args": ["arg1", "arg2", "arg3"]
  }'
```

## 3. Выполнение скрипта из файла

**Важно:** Папка `scripts` должна находиться **рядом с исполняемым файлом агента**. Агент автоматически найдет эту папку.

**Структура папок:**
```
build/bin/Release/
├── monitoring_agent.exe  ← Агент
├── agent_config.json
└── scripts/              ← Папка со скриптами
    ├── hello.bat
    ├── hello.ps1
    └── hello.py
```

### Указание пути к файлу
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script_path": "hello.bat",
    "interpreter": "cmd"
  }'
```

### PowerShell скрипт из файла
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script_path": "hello.ps1",
    "interpreter": "powershell"
  }'
```

### Python скрипт из файла
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script_path": "hello.py",
    "interpreter": "python"
  }'
```

## 4. Выполнение по ключу (предустановленные скрипты)

### Выполнение скрипта по ключу
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "key": "system_info",
    "interpreter": "auto"
  }'
```

### Выполнение с параметрами по ключу
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "key": "custom_script",
    "interpreter": "auto",
    "args": ["param1", "param2"]
  }'
```

## 5. Дополнительные параметры

### С таймаутом
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "ping google.com",
    "interpreter": "cmd",
    "timeout_sec": 30
  }'
```

### Без захвата вывода (фоновое выполнение)
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "start /B notepad.exe",
    "interpreter": "cmd",
    "capture_output": false,
    "background": true
  }'
```

### С рабочей директорией
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "dir",
    "interpreter": "cmd",
    "working_dir": "scripts"
  }'
```

### С переменными окружения
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "echo %CUSTOM_VAR%",
    "interpreter": "cmd",
    "env": {"CUSTOM_VAR": "Hello World"}
  }'
```

## 6. Комплексные примеры

### Системная информация Windows
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "systeminfo | findstr /B /C:\"OS Name\" /C:\"OS Version\" /C:\"Total Physical Memory\"",
    "interpreter": "cmd",
    "capture_output": true
  }'
```

### Проверка дисков
```bash
curl -X POST "http://localhost:8000/api/agents/agent_1756034799529_8439/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "wmic logicaldisk get size,freespace,caption",
    "interpreter": "cmd",
    "capture_output": true
  }'
```