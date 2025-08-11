# Новая архитектура агента мониторинга

## Обзор

Агент был переработан для поддержки двустороннего взаимодействия с сервером мониторинга. Теперь агент может не только отправлять метрики, но и принимать команды от сервера.

## Архитектура

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   AgentManager  │    │ AgentHttpServer │    │MonitoringServer │
│                 │    │                 │    │    Client       │
│ - Управление    │◄──►│ - HTTP сервер   │    │ - Отправка      │
│ - Конфигурация  │    │ - Команды       │    │   метрик        │
│ - Сбор метрик   │    │ - Обработчики   │    │ - Heartbeat     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│ AgentConfig     │    │ Command         │    │ HTTP клиент     │
│ - JSON конфиг   │    │ - Структуры     │    │ - CPR библиотека│
│ - Загрузка/     │    │ - Обработка     │    │ - REST API      │
│   сохранение    │    │ - Ответы        │    │ - Таймауты      │
│ - Автоопределение│   │ - Фильтрация    │    │ - Авторегистрация│
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## Компоненты

### 1. AgentConfig
**Файлы:** `src/agent_config.hpp`, `src/agent_config.cpp`

Управление конфигурацией агента:
- Загрузка/сохранение в JSON
- Динамическое обновление
- **Автоматическое определение ID и имени машины**
- **Словарь метрик с флагами включения/отключения**

**Основные параметры:**
```json
{
  "agent_id": "",  // Автоматически определяется
  "machine_name": "",  // Автоматически определяется
  "server_url": "http://localhost:8000/metrics",
      "update_frequency": 60,
  "auto_detect_id": true,
  "auto_detect_name": true,
  "enabled_metrics": {
    "cpu": true,
    "memory": true,
    "disk": true,
    "network": true,
    "gpu": false,
    "hdd": false,
    "inventory": true
  }
}
```

### 2. AgentHttpServer
**Файлы:** `src/agent_api.hpp`, `src/agent_api.cpp`

HTTP сервер для приема команд:
- Прослушивание порта 8081
- Обработка JSON команд
- Система обработчиков команд

**Поддерживаемые команды:**
- `collect_metrics` - сбор метрик (с поддержкой фильтрации)
- `update_config` - обновление конфигурации
- `restart` - перезапуск агента
- `stop` - остановка агента

### 3. MonitoringServerClient
**Файлы:** `src/agent_api.hpp`, `src/agent_api.cpp`

Клиент для взаимодействия с сервером:
- Отправка метрик
- Heartbeat
- **Автоматическая регистрация агента**
- Получение конфигурации

### 4. AgentManager
**Файлы:** `src/agent_api.hpp`, `src/agent_api.cpp`

Главный компонент управления агентом:
- Координация всех компонентов
- Управление потоками
- Обработка команд
- **Фильтрация метрик согласно конфигурации**

## Новые возможности

### 1. Автоматическое определение ID и имени
```cpp
// При запуске агент автоматически определяет:
agent_id = generate_agent_id();  // agent_1703123456789_1234
machine_name = get_machine_name();  // DESKTOP-ABC123
```

### 2. Гибкая конфигурация метрик
```json
{
  "enabled_metrics": {
    "cpu": true,
    "memory": true,
    "disk": false,  // Отключен
    "network": true,
    "gpu": false,   // Отключен
    "hdd": false,   // Отключен
    "inventory": true
  }
}
```

### 3. Динамическая конфигурация
```bash
# Обновление конфигурации метрик
curl -X POST "http://localhost:8000/api/agents/agent_001/config" \
  -H "Content-Type: application/json" \
  -d '{
    "enabled_metrics": {
      "cpu": true,
      "memory": true,
      "disk": false
    },
    "update_frequency": 30
  }'
```

### 4. Запрос метрик с фильтрацией
```bash
# Запрос только CPU и памяти
curl -X POST "http://localhost:8000/api/agents/agent_001/request_metrics" \
  -H "Content-Type: application/json" \
  -d '{
    "metrics": {
      "cpu": true,
      "memory": true,
      "disk": false,
      "network": false
    },
    "immediate": true
  }'
```

### 5. Обратная совместимость
```bash
# Старый формат (массив строк) все еще поддерживается
curl -X POST "http://localhost:8000/api/agents/agent_001/request_metrics" \
  -H "Content-Type: application/json" \
  -d '{
    "metrics": ["cpu", "memory", "disk"],
    "immediate": true
  }'
```

## Сборка

### Сборка новой версии
```bash
mkdir build && cd build
cmake -DUSE_NEW_AGENT=ON ..
make -j$(nproc)
```

### Запуск
```bash
# Запуск с конфигурацией по умолчанию
./monitoring_agent_new

# Запуск с кастомной конфигурацией
./monitoring_agent_new --config agent_config.json
```

## Конфигурация

### Создание конфигурационного файла
```json
{
  "agent_id": "",  // Автоматически определится
  "machine_name": "",  // Автоматически определится
  "server_url": "http://monitoring-server:8000/metrics",
      "update_frequency": 60,
  "auto_detect_id": true,
  "auto_detect_name": true,
  "enabled_metrics": {
    "cpu": true,
    "memory": true,
    "disk": false,
    "network": true,
    "gpu": false,
    "hdd": false,
    "inventory": true
  }
}
```

### Переменные окружения
```bash
export MONITORING_AGENT_URL="http://server:8000/metrics"
export AGENT_ID="custom_agent_id"  # Переопределяет автоопределение
export MACHINE_NAME="Custom Machine Name"  # Переопределяет автоопределение
```

## API агента

### Endpoints агента (порт 8081)

```
POST /command
{
  "command": "collect_metrics",
  "data": {
    "metrics": {
      "cpu": true,
      "memory": true,
      "disk": false
    },
    "immediate": true
  },
  "timestamp": "2024-01-01T00:00:00"
}
```

### Ответы агента
```json
{
  "success": true,
  "message": "Metrics collected and sent",
  "data": {
    "cpu": {"usage_percent": 45.2},
    "memory": {"usage_percent": 60.1}
  },
  "timestamp": "2024-01-01T00:00:00"
}
```

## Логирование

Агент выводит подробные логи:
```
🚀 Starting Monitoring Agent...
📋 Agent ID: agent_1703123456789_1234
🖥️  Machine: DESKTOP-ABC123
🌐 Server URL: http://localhost:8000/metrics
⏱️  Heartbeat interval: 60 seconds
📊 Enabled metrics:
   ✅ cpu
   ✅ memory
   ✅ disk
   ✅ network
   ❌ gpu
   ❌ hdd
   ✅ inventory
🔧 Starting agent manager...
✅ Agent started successfully!
📡 Listening for commands on port 8081
💓 Sending heartbeat every 60 seconds
📊 Collecting metrics every 60 seconds
==================================================
```

## Безопасность

### Рекомендации для продакшена:
1. **Аутентификация** - добавить JWT токены
2. **HTTPS** - использовать SSL/TLS
3. **Firewall** - ограничить доступ к порту 8081
4. **Валидация** - проверка входящих команд
5. **Логирование** - детальные логи операций

## Миграция

### С старой версии на новую:
1. Создайте `agent_config.json` (или оставьте пустым для автоопределения)
2. Остановите старый агент
3. Запустите новый агент: `./monitoring_agent_new`
4. Проверьте регистрацию на сервере

### Обратная совместимость:
- Старый агент продолжает работать
- Новый агент совместим с существующим сервером
- Поддерживается старый формат запросов метрик
- Постепенная миграция возможна

## Тестирование

### Запуск тестов новых возможностей:
```bash
python test_new_agent_features.py
```

### Тестируемые функции:
- ✅ Автоматическое определение ID/имени
- ✅ Конфигурация метрик с флагами
- ✅ Запрос метрик с фильтрацией
- ✅ Обратная совместимость
- ✅ Команды агента
- ✅ Динамическая конфигурация 