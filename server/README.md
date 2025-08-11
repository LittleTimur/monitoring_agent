# Monitoring Server (FastAPI)

Сервер мониторинга для управления агентами и сбора метрик.

## Быстрый старт

```bash
cd server
pip install -r requirements.txt
python run.py
```

Сервер будет доступен по адресу: http://localhost:8000

## API Endpoints

### Агенты

#### Регистрация агентов
- `POST /api/agents/register` - Автоматическая регистрация с генерацией ID
- `POST /api/agents/{agent_id}/register` - Регистрация с указанным ID

**Формат данных (JSON):**
```json
{
    "machine_name": "MyMachine",
    "machine_type": "Desktop",
    "ip_address": "192.168.1.100",
    "version": "1.0.0"
}
```

#### Управление агентами
- `GET /api/agents/` - Список всех агентов
- `GET /api/agents/statistics` - Статистика по агентам
- `GET /api/agents/{agent_id}` - Информация об агенте
- `GET /api/agents/{agent_id}/config` - Конфигурация агента
- `POST /api/agents/{agent_id}/config` - Обновление конфигурации

#### Система команд (NEW! 🚀)

**Отправка команд:**
- `POST /api/agents/{agent_id}/command` - Команда конкретному агенту
- `POST /api/agents/command_all` - Команда всем агентам

**Мониторинг выполнения:**
- `GET /api/agents/{agent_id}/command-executions` - История команд агента
- `GET /api/agents/{agent_id}/command-executions/{index}` - Статус команды
- `GET /api/agents/command-executions/status` - Общий статус всех команд

**Управление командами:**
- `POST /api/agents/{agent_id}/command-executions/{index}/retry` - Повтор команды
- `DELETE /api/agents/{agent_id}/command-executions/{index}` - Удаление записи

#### Метрики
- `POST /api/agents/{agent_id}/request_metrics` - Запрос метрик от агента
- `POST /api/agents/request_metrics_from_all` - Запрос метрик от всех агентов
- `POST /metrics` - Прием метрик от агентов (автоматическая регистрация)

#### Управление состоянием
- `POST /api/agents/{agent_id}/restart` - Перезапуск агента
- `POST /api/agents/{agent_id}/stop` - Остановка агента

### Система

- `GET /` - Корневой эндпоинт
- `GET /health` - Проверка здоровья сервера
- `GET /docs` - Swagger UI документация

## Типы команд

### 1. collect_metrics
Сбор метрик с агента
```json
{
    "command": "collect_metrics",
    "data": {
        "metrics": {"cpu": true, "memory": true},
        "immediate": true
    }
}
```

### 2. update_config
Обновление конфигурации агента
```json
{
    "command": "update_config",
    "data": {
        "update_frequency": 300,
        "enabled_metrics": {"cpu": true, "gpu": false}
    }
}
```

### 3. restart
Перезапуск агента
```json
{
    "command": "restart",
    "data": {}
}
```

### 4. stop
Остановка агента
```json
{
    "command": "stop",
    "data": {}
}
```

## Особенности системы команд

### ✅ Мониторинг в реальном времени
- Отслеживание статуса каждой команды
- История выполнения команд
- Статистика успешности

### ✅ Автоматические повторные попытки
- До 3 попыток при ошибках
- Экспоненциальная задержка между попытками
- Настраиваемые таймауты

### ✅ Асинхронная обработка
- Команды не блокируют API
- Поддержка множественных одновременных команд
- Фоновая отправка через BackgroundTasks

### ✅ Детальное логирование
- Все операции логируются с эмодзи
- Отслеживание времени выполнения
- Информация об ошибках и повторных попытках

## Структуры данных

### MetricsData
```python
class MetricsData(BaseModel):
    agent_id: str
    machine_name: str
    machine_type: str
    timestamp: datetime
    cpu_usage: float
    memory_usage: float
    disk_usage: float
    network_io: Dict[str, float]
```

### AgentConfig
```python
class AgentConfig(BaseModel):
    agent_id: str
    machine_name: str
    update_frequency: int = 60
    enabled_metrics: Dict[str, bool] = Field(default_factory=dict)
```

### AgentCommand
```python
class AgentCommand(BaseModel):
    command: str
    data: Dict[str, Any] = Field(default_factory=dict)
```

## Конфигурация

### Переменные окружения
- `HOST` - Хост для сервера (по умолчанию: 0.0.0.0)
- `PORT` - Порт для сервера (по умолчанию: 8000)

### Настройки агентов
- `agent_timeout` - Таймаут для определения офлайн статуса (300 сек)
- `command_timeout` - Таймаут выполнения команды (30 сек)
- `max_concurrent_commands` - Максимум одновременных команд на агента (5)

## Мониторинг

### Логи сервера
Запустите сервер и следите за логами:
```bash
python run.py
```

### API мониторинга
```bash
# Статистика агентов
curl http://localhost:8000/api/agents/statistics

# Статус команд
curl http://localhost:8000/api/agents/command-executions/status

# История команд агента
curl http://localhost:8000/api/agents/agent_001/command-executions
```

## Тестирование

### Тест регистрации
```bash
python test_unified_registration.py
```

### Тест системы команд
```bash
python test_command_system.py
```

## Архитектура

### Компоненты
1. **FastAPI приложение** - Основной сервер
2. **AgentService** - Управление агентами и командами
3. **API роутеры** - REST API эндпоинты
4. **Pydantic модели** - Валидация данных

### Коммуникация
- **Push модель** - Сервер инициирует отправку команд агентам
- **HTTP API** - Агенты слушают команды на порту 8081
- **JSON формат** - Все данные передаются в JSON

## Следующие шаги

1. **PostgreSQL интеграция** - Персистентное хранение метрик и команд
2. **SNMP поддержка** - Мониторинг сетевого оборудования
3. **Web интерфейс** - Графический интерфейс управления
4. **Система уведомлений** - Алерты при превышении порогов
5. **Аутентификация** - JWT токены и HTTPS для продакшена

## Документация

- [Система команд](COMMAND_SYSTEM.md) - Детальное описание системы команд
- [Единый стиль регистрации](unified_registration_style.md) - Регистрация агентов
- [CHANGELOG](CHANGELOG.md) - История изменений 