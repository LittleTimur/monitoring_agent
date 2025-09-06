# База данных

PostgreSQL база данных для хранения метрик, конфигурации агентов и системной информации. Использует реляционную модель с JSONB для гибкого хранения детальных данных.

## 🏗️ Архитектура базы данных

```
┌─────────────────┐    SQLAlchemy    ┌─────────────────┐
│   FastAPI       │ ────────────────► │   PostgreSQL    │
│     Server      │                  │   Database      │
└─────────────────┘                  └─────────────────┘
                                              │
                                              ▼
                                     ┌─────────────────┐
                                     │   Tables &      │
                                     │   Relations     │
                                     └─────────────────┘
```

## 📊 Схема базы данных

### Основные таблицы

#### 1. `agents` - Агенты мониторинга
Основная таблица для хранения информации об агентах.

| Поле | Тип | Описание |
|------|-----|----------|
| `agent_id` | VARCHAR(255) | Уникальный идентификатор агента (PK) |
| `machine_name` | VARCHAR(255) | Имя машины |
| `auto_detect_id` | BOOLEAN | Автоопределение ID |
| `auto_detect_name` | BOOLEAN | Автоопределение имени |
| `command_server_host` | INET | IP для прослушивания команд |
| `command_server_port` | INTEGER | Порт для прослушивания команд |
| `command_server_url` | TEXT | URL командного сервера |
| `server_url` | TEXT | URL центрального сервера |
| `scripts_dir` | TEXT | Директория скриптов |
| `audit_log_enabled` | BOOLEAN | Включено логирование |
| `audit_log_path` | TEXT | Путь к логу |
| `enable_inline_commands` | BOOLEAN | Разрешены inline-команды |
| `enable_user_parameters` | BOOLEAN | Разрешены пользовательские параметры |
| `job_retention_seconds` | INTEGER | Время хранения результатов |
| `max_buffer_size` | INTEGER | Макс. размер буфера |
| `max_concurrent_jobs` | INTEGER | Макс. число одновременных задач |
| `max_output_bytes` | BIGINT | Макс. размер вывода |
| `max_script_timeout_sec` | INTEGER | Макс. время выполнения скрипта |
| `send_timeout_ms` | INTEGER | Таймаут отправки |
| `update_frequency` | INTEGER | Частота обновления |
| `created_at` | TIMESTAMPTZ | Время создания |
| `last_heartbeat` | TIMESTAMPTZ | Последний heartbeat |

#### 2. `agent_metrics` - Метрики агентов
Таблица для хранения метрик от агентов.

| Поле | Тип | Описание |
|------|-----|----------|
| `id` | BIGSERIAL | Уникальный идентификатор (PK) |
| `agent_id` | VARCHAR(255) | ID агента (FK) |
| `timestamp` | TIMESTAMPTZ | Временная метка |
| `machine_type` | VARCHAR(50) | Тип машины (physical/virtual) |
| `machine_name` | VARCHAR(255) | Имя машины |
| `metric_type` | VARCHAR(20) | Тип метрики |
| `usage_percent` | FLOAT | Процент использования |
| `temperature` | FLOAT | Температура |
| `total_bytes` | BIGINT | Общий объем в байтах |
| `used_bytes` | BIGINT | Использованный объем |
| `free_bytes` | BIGINT | Свободный объем |
| `details` | JSONB | Детальные данные |

#### 3. `metrics_network_connections` - Сетевые соединения
Таблица для хранения информации о сетевых соединениях.

| Поле | Тип | Описание |
|------|-----|----------|
| `id` | BIGSERIAL | Уникальный идентификатор (PK) |
| `metric_id` | BIGINT | ID метрики (FK) |
| `local_ip` | VARCHAR(45) | Локальный IP |
| `local_port` | INTEGER | Локальный порт |
| `remote_ip` | VARCHAR(45) | Удаленный IP |
| `remote_port` | INTEGER | Удаленный порт |
| `protocol` | VARCHAR(10) | Протокол (TCP/UDP) |

### Справочные таблицы

#### 4. `interpreters` - Интерпретаторы
Справочная таблица поддерживаемых интерпретаторов.

| Поле | Тип | Описание |
|------|-----|----------|
| `name` | VARCHAR(50) | Название интерпретатора (PK) |

#### 5. `metric_types` - Типы метрик
Справочная таблица типов метрик.

| Поле | Тип | Описание |
|------|-----|----------|
| `name` | VARCHAR(50) | Название типа метрики (PK) |

### Связующие таблицы

#### 6. `agent_allowed_interpreters` - Разрешенные интерпретаторы
Связь многие-ко-многим между агентами и интерпретаторами.

| Поле | Тип | Описание |
|------|-----|----------|
| `agent_id` | VARCHAR(255) | ID агента (FK) |
| `interpreter_name` | VARCHAR(50) | Название интерпретатора (FK) |

#### 7. `agent_enabled_metrics` - Включенные метрики
Связь многие-ко-многим между агентами и типами метрик.

| Поле | Тип | Описание |
|------|-----|----------|
| `agent_id` | VARCHAR(255) | ID агента (FK) |
| `metric_name` | VARCHAR(50) | Название типа метрики (FK) |

#### 8. `agent_user_parameters` - Пользовательские параметры
Пользовательские параметры агентов.

| Поле | Тип | Описание |
|------|-----|----------|
| `id` | SERIAL | Уникальный идентификатор (PK) |
| `agent_id` | VARCHAR(255) | ID агента (FK) |
| `parameter_key` | VARCHAR(255) | Ключ параметра |
| `command` | TEXT | Команда для выполнения |

## 🔗 Связи между таблицами

### Диаграмма связей

```
agents (1) ──────── (N) agent_metrics
   │
   ├── (N) agent_allowed_interpreters (N) ─── interpreters
   ├── (N) agent_enabled_metrics (N) ─────── metric_types
   └── (1) ──────── (N) agent_user_parameters

agent_metrics (1) ──────── (N) metrics_network_connections
```

### Внешние ключи

- `agent_metrics.agent_id` → `agents.agent_id`
- `metrics_network_connections.metric_id` → `agent_metrics.id`
- `agent_allowed_interpreters.agent_id` → `agents.agent_id`
- `agent_allowed_interpreters.interpreter_name` → `interpreters.name`
- `agent_enabled_metrics.agent_id` → `agents.agent_id`
- `agent_enabled_metrics.metric_name` → `metric_types.name`
- `agent_user_parameters.agent_id` → `agents.agent_id`

## 📈 Индексы для производительности

### Основные индексы

```sql
-- Индексы для таблицы agents
CREATE INDEX idx_agents_machine_name ON agents(machine_name);
CREATE INDEX idx_agents_last_heartbeat ON agents(last_heartbeat);
CREATE INDEX idx_agents_created_at ON agents(created_at);

-- Индексы для таблицы agent_metrics
CREATE INDEX idx_agent_metrics_agent_id ON agent_metrics(agent_id);
CREATE INDEX idx_agent_metrics_timestamp ON agent_metrics(timestamp);
CREATE INDEX idx_agent_metrics_type ON agent_metrics(metric_type);
CREATE INDEX idx_agent_metrics_agent_timestamp ON agent_metrics(agent_id, timestamp);
CREATE INDEX idx_agent_metrics_usage ON agent_metrics(usage_percent) WHERE usage_percent IS NOT NULL;
CREATE INDEX idx_agent_metrics_details_gin ON agent_metrics USING GIN (details);

-- Индексы для таблицы metrics_network_connections
CREATE INDEX idx_network_connections_metric ON metrics_network_connections(metric_id);
CREATE INDEX idx_network_connections_local ON metrics_network_connections(local_ip);
CREATE INDEX idx_network_connections_remote ON metrics_network_connections(remote_ip);

-- Индексы для таблицы agent_user_parameters
CREATE INDEX idx_agent_user_parameters_agent_id ON agent_user_parameters(agent_id);
CREATE INDEX idx_agent_user_parameters_key ON agent_user_parameters(parameter_key);
```

### Специальные индексы

- **GIN индекс** для JSONB поля `details` - быстрый поиск по JSON данным
- **Частичный индекс** для `usage_percent` - только для не-NULL значений
- **Составные индексы** для частых запросов по агенту и времени

## 📊 Типы данных

### Поддерживаемые типы метрик

| Тип | Описание | Поля в details |
|-----|----------|----------------|
| `cpu` | Процессор | `cores`, `load_avg`, `frequency` |
| `memory` | Память | `swap_total`, `swap_used`, `swap_free` |
| `disk` | Диски | `device`, `filesystem`, `mount_point` |
| `network` | Сеть | `interface`, `bytes_sent`, `bytes_recv` |
| `gpu` | GPU | `driver_version`, `memory_total`, `memory_used` |
| `hdd` | HDD | `model`, `serial`, `health_status` |
| `user` | Пользовательские | Любые пользовательские данные |
| `inventory` | Инвентарь | `os_version`, `hardware_info`, `software_list` |

### Примеры JSONB данных

**CPU метрика:**
```json
{
  "cores": 8,
  "load_avg": 1.2,
  "frequency": 2400,
  "temperature": 45.5
}
```

**Memory метрика:**
```json
{
  "swap_total": 8589934592,
  "swap_used": 1073741824,
  "swap_free": 7516192768,
  "cached": 2147483648
}
```

**Network метрика:**
```json
{
  "interface": "eth0",
  "bytes_sent": 1024000,
  "bytes_recv": 2048000,
  "packets_sent": 1500,
  "packets_recv": 2000
}
```

## 🔧 Миграции базы данных

### Структура миграций

```
database/
├── migrations/
│   ├── 001_initial_schema.sql    # Основные таблицы
│   └── 002_metrics_tables.sql    # Таблицы метрик
├── schema.sql                   # Объединенная схема
└── seed_data.sql               # Начальные данные
```

### Миграция 001: Основные таблицы
```sql
-- Создание таблиц агентов, интерпретаторов, типов метрик
-- и связующих таблиц
```

### Миграция 002: Таблицы метрик
```sql
-- Создание таблиц для метрик и сетевых соединений
-- с соответствующими индексами
```

### Начальные данные
```sql
-- Заполнение справочных таблиц
INSERT INTO interpreters (name) VALUES 
    ('bash'), ('powershell'), ('cmd'), ('python'), ('sh');

INSERT INTO metric_types (name) VALUES 
    ('cpu'), ('memory'), ('disk'), ('network'), ('gpu'), 
    ('hdd'), ('inventory'), ('user');
```

## 📈 Оптимизация производительности

### Настройки PostgreSQL

```sql
-- Настройки для производительности
ALTER SYSTEM SET shared_buffers = '256MB';
ALTER SYSTEM SET effective_cache_size = '1GB';
ALTER SYSTEM SET maintenance_work_mem = '64MB';
ALTER SYSTEM SET checkpoint_completion_target = 0.9;
ALTER SYSTEM SET wal_buffers = '16MB';
ALTER SYSTEM SET default_statistics_target = 100;
```

### Партиционирование

Для больших объемов данных можно использовать партиционирование:

```sql
-- Партиционирование по времени
CREATE TABLE agent_metrics_y2025m09 PARTITION OF agent_metrics
FOR VALUES FROM ('2025-09-01') TO ('2025-10-01');
```

### Архивация данных

```sql
-- Архивация старых метрик
CREATE TABLE agent_metrics_archive (LIKE agent_metrics INCLUDING ALL);

-- Перенос данных старше 1 года
INSERT INTO agent_metrics_archive 
SELECT * FROM agent_metrics 
WHERE timestamp < NOW() - INTERVAL '1 year';

DELETE FROM agent_metrics 
WHERE timestamp < NOW() - INTERVAL '1 year';
```

## 🔒 Безопасность

### Права доступа

```sql
-- Создание пользователя для приложения
CREATE USER agent_user WITH PASSWORD 'agent_password';

-- Предоставление прав
GRANT CONNECT ON DATABASE monitoring_agent TO agent_user;
GRANT USAGE ON SCHEMA public TO agent_user;
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO agent_user;
GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO agent_user;
```

### Шифрование

```sql
-- Включение шифрования соединений
ALTER SYSTEM SET ssl = on;
ALTER SYSTEM SET ssl_cert_file = 'server.crt';
ALTER SYSTEM SET ssl_key_file = 'server.key';
```

## 📊 Мониторинг базы данных

### Запросы для мониторинга

**Размер базы данных:**
```sql
SELECT pg_size_pretty(pg_database_size('monitoring_agent'));
```

**Количество записей:**
```sql
SELECT 
    schemaname,
    tablename,
    n_tup_ins as inserts,
    n_tup_upd as updates,
    n_tup_del as deletes
FROM pg_stat_user_tables;
```

**Медленные запросы:**
```sql
SELECT query, mean_time, calls 
FROM pg_stat_statements 
ORDER BY mean_time DESC 
LIMIT 10;
```

## 🐛 Устранение неполадок

### Проблемы с производительностью

1. **Медленные запросы:**
   - Проверьте индексы
   - Используйте EXPLAIN ANALYZE
   - Оптимизируйте запросы

2. **Большой размер БД:**
   - Настройте архивацию
   - Используйте партиционирование
   - Очистите старые данные

3. **Блокировки:**
   - Проверьте активные транзакции
   - Используйте мониторинг блокировок

### Проблемы с подключением

1. **Ошибки подключения:**
   - Проверьте настройки PostgreSQL
   - Убедитесь в правильности строки подключения
   - Проверьте права пользователя

2. **Проблемы с миграциями:**
   - Проверьте порядок выполнения миграций
   - Убедитесь в корректности SQL
   - Проверьте права на создание таблиц

## 📚 Дополнительные ресурсы

- [Агент мониторинга](agent.md)
- [Сервер мониторинга](server.md)
- [Развертывание](deployment.md)
- [PostgreSQL документация](https://www.postgresql.org/docs/)
- [SQLAlchemy документация](https://docs.sqlalchemy.org/)
