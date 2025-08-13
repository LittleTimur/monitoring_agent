# API для управления агентами через эндпоинт `/command`

## Обзор

Все операции управления агентами выполняются через один универсальный эндпоинт `/command` с разными командами.

## Базовый URL

```
POST /api/agents/{agent_id}/command
POST /api/agents/command_all
```

## Поддерживаемые команды

### 1. Изменение собираемых метрик и времени сбора

**Команда:** `update_config`

**Пример запроса:**
```json
{
  "command": "update_config",
  "data": {
    "enabled_metrics": {
      "cpu": true,
      "memory": false,
      "disk": false,
      "network": false,
      "gpu": false,
      "hdd": false,
      "inventory": false
    },
    "update_frequency": 39
  }
}
```

**Описание:**
- `enabled_metrics` - объект с флагами включения/выключения метрик
- `update_frequency` - интервал сбора метрик в секундах

### 2. Немедленный сбор выбранных метрик

**Команда:** `collect_metrics`

**Пример запроса:**
```json
{
  "command": "collect_metrics",
  "data": {
    "metrics": {
      "cpu": false,
      "memory": true,
      "disk": false,
      "network": false,
      "gpu": false,
      "hdd": false,
      "inventory": false
    },
    "immediate": true
  }
}
```

**Описание:**
- `metrics` - объект с флагами для сбора конкретных метрик
- `immediate` - флаг немедленного сбора (всегда true)

### 3. Перезапуск агента

**Команда:** `restart`

**Пример запроса:**
```json
{
  "command": "restart",
  "data": {}
}
```

### 4. Остановка агента

**Команда:** `stop`

**Пример запроса:**
```json
{
  "command": "stop",
  "data": {}
}
```

## Примеры использования

### Отправка команды конкретному агенту

```bash
curl -X POST "http://localhost:8000/api/agents/agent_123/command" \
  -H "Content-Type: application/json" \
  -d '{
    "command": "update_config",
    "data": {
      "enabled_metrics": {
        "cpu": true,
        "memory": true,
        "disk": false
      },
      "update_frequency": 60
    }
  }'
```

### Отправка команды всем агентам

```bash
curl -X POST "http://localhost:8000/api/agents/command_all" \
  -H "Content-Type: application/json" \
  -d '{
    "command": "collect_metrics",
    "data": {
      "metrics": {
        "cpu": true,
        "memory": true
      }
    }
  }'
```

### Немедленный сбор всех метрик

```bash
curl -X POST "http://localhost:8000/api/agents/agent_123/command" \
  -H "Content-Type: application/json" \
  -d '{
    "command": "collect_metrics",
    "data": {
      "metrics": {
        "cpu": true,
        "memory": true,
        "disk": true,
        "network": true,
        "gpu": false,
        "hdd": false,
        "inventory": true
      }
    }
  }'
```

## Ответы

### Успешное выполнение
```json
{
  "status": "success",
  "message": "Command 'update_config' sent to agent agent_123"
}
```

### Ошибка
```json
{
  "detail": "Agent not found"
}
```

## Логирование

Все команды логируются на сервере:
- `🚀 Отправка команды '{command}' агенту {agent_id}`
- `✅ Команда успешно отправлена агенту {agent_id}`
- `❌ Ошибка отправки команды: {message}`

## Примечания

1. **Агент автоматически перезагружает конфигурацию** при получении команды `update_config`
2. **Метрики отправляются немедленно** при получении команды `collect_metrics`
3. **Команды выполняются асинхронно** через background tasks
4. **Все команды добавляются в очередь** агента для отслеживания
5. **Поддерживается повторная отправка** при ошибках с экспоненциальной задержкой
