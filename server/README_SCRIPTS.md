# API для работы со скриптами - Инструкция по использованию

## Обзор

API для работы со скриптами позволяет выполнять команды на агентах мониторинга через HTTP запросы. Поддерживаются различные интерпретаторы: cmd, PowerShell, bash, Python.

## Быстрый старт

### 1. Запуск сервера
```bash
cd server
pip install -r requirements.txt
python run.py
```

### 2. Проверка доступности агентов
```bash
curl http://localhost:8000/api/agents/
```

### 3. Простой тест команды
```bash
curl -X POST "http://localhost:8000/api/agents/{AGENT_ID}/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "echo Привет!",
    "interpreter": "cmd"
  }'
```

## Файлы для тестирования

### 1. `script_api_examples.md`
Содержит все возможные примеры API запросов с подробными объяснениями.

### 2. `test_script_api.py`
Python скрипт для автоматического тестирования всех функций API.

### 3. `test_scripts_windows.bat`
Batch файл для Windows с интерактивным меню тестирования.

## Способы выполнения скриптов

### 1. Встроенный скрипт
```json
{
  "script": "echo Привет!",
  "interpreter": "cmd"
}
```

### 2. Скрипт из файла
```json
{
  "script_path": "C:\\scripts\\hello.bat",
  "interpreter": "cmd"
}
```

### 3. Предустановленный скрипт
```json
{
  "key": "system_info",
  "interpreter": "auto"
}
```

## Поддерживаемые интерпретаторы

### Windows
- **cmd** - Командная строка Windows
- **powershell** - PowerShell
- **python** - Python (если установлен)

### Linux
- **bash** - Bash shell
- **python** - Python
- **sh** - POSIX shell

## Параметры запроса

### Основные параметры
- `script` - Текст скрипта для выполнения
- `script_path` - Путь к файлу скрипта
- `key` - Ключ предустановленного скрипта
- `interpreter` - Интерпретатор для выполнения

### Дополнительные параметры
- `args` - Массив аргументов для скрипта
- `timeout_sec` - Таймаут выполнения в секундах
- `capture_output` - Захватывать ли вывод (по умолчанию true)
- `background` - Выполнять ли в фоне (по умолчанию false)
- `working_dir` - Рабочая директория
- `env` - Переменные окружения

## Примеры использования

### Простая команда echo
```bash
curl -X POST "http://localhost:8000/api/agents/{AGENT_ID}/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "echo Привет!",
    "interpreter": "cmd"
  }'
```

### Системная информация Windows
```bash
curl -X POST "http://localhost:8000/api/agents/{AGENT_ID}/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "systeminfo | findstr /B /C:\"OS Name\" /C:\"OS Version\"",
    "interpreter": "cmd"
  }'
```

### PowerShell команда
```bash
curl -X POST "http://localhost:8000/api/agents/{AGENT_ID}/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "Get-ComputerInfo | Select-Object WindowsProductName",
    "interpreter": "powershell"
  }'
```

### Python скрипт
```bash
curl -X POST "http://localhost:8000/api/agents/{AGENT_ID}/run_script" \
  -H "Content-Type: application/json" \
  -d '{
    "script": "import platform\nprint(f\"OS: {platform.system()}\")",
    "interpreter": "python"
  }'
```

## Мониторинг выполнения

### Получение статуса задачи
```bash
curl "http://localhost:8000/api/agents/{AGENT_ID}/jobs/{JOB_ID}"
```

### Список всех задач
```bash
curl "http://localhost:8000/api/agents/{AGENT_ID}/jobs"
```

### Остановка задачи
```bash
curl -X DELETE "http://localhost:8000/api/agents/{AGENT_ID}/jobs/{JOB_ID}"
```

## Тестирование

### Автоматическое тестирование (Python)
```bash
cd server
python test_script_api.py
```

### Интерактивное тестирование (Windows)
```bash
cd server
test_scripts_windows.bat
```

### Ручное тестирование
Используйте примеры из `script_api_examples.md` с curl или Postman.

## Устранение неполадок

### Ошибка "Agent not found"
- Убедитесь, что агент зарегистрирован
- Проверьте правильность ID агента
- Проверьте статус агента: `GET /api/agents/{AGENT_ID}`

### Ошибка "Too many concurrent commands"
- Агент выполняет слишком много команд одновременно
- Подождите завершения текущих команд
- Проверьте список задач: `GET /api/agents/{AGENT_ID}/jobs`

### Команда не выполняется
- Проверьте правильность синтаксиса скрипта
- Убедитесь, что интерпретатор доступен
- Проверьте права доступа агента

### Таймаут команды
- Увеличьте `timeout_sec` в запросе
- Проверьте, не завис ли скрипт
- Используйте `background: true` для длительных операций

## Безопасность

### Ограничения
- Агент выполняет команды с правами пользователя, под которым запущен
- Некоторые команды могут требовать административных прав
- Используйте `capture_output: false` для чувствительных команд

### Рекомендации
- Валидируйте входные данные
- Ограничивайте доступ к API
- Логируйте все выполняемые команды
- Используйте таймауты для предотвращения зависания

## Следующие шаги

1. **Интеграция с веб-интерфейсом** - Создание GUI для управления скриптами
2. **Шаблоны скриптов** - Предустановленные скрипты для типовых задач
3. **Планировщик** - Выполнение скриптов по расписанию
4. **Уведомления** - Оповещения о результатах выполнения
5. **Аудит** - Подробное логирование всех операций

## Поддержка

При возникновении проблем:
1. Проверьте логи сервера
2. Убедитесь в корректности запроса
3. Проверьте статус агента
4. Используйте тестовые скрипты для диагностики
