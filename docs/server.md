# Сервер мониторинга

FastAPI сервер для сбора, хранения и управления метриками от агентов мониторинга. Использует PostgreSQL для хранения данных и предоставляет REST API для взаимодействия.

## 🏗️ Архитектура сервера

```
┌─────────────────┐    HTTP/HTTPS    ┌─────────────────┐
│   Monitoring    │ ────────────────► │   FastAPI       │
│     Agent       │                  │     Server      │
└─────────────────┘                  └─────────────────┘
                                              │
                                              ▼
                                     ┌─────────────────┐
                                     │   PostgreSQL    │
                                     │   Database      │
                                     └─────────────────┘
```

### Компоненты сервера

- **FastAPI Application** - веб-фреймворк для API
- **SQLAlchemy ORM** - работа с базой данных
- **PostgreSQL** - хранение данных
- **Pydantic** - валидация данных
- **Docker** - контейнеризация

## 🔧 Технологический стек

### Backend
- **FastAPI** - современный веб-фреймворк для Python
- **SQLAlchemy** - ORM для работы с базой данных
- **PostgreSQL** - реляционная база данных
- **Pydantic** - валидация и сериализация данных
- **asyncpg** - асинхронный драйвер PostgreSQL

### Инфраструктура
- **Docker** - контейнеризация приложения
- **Docker Compose** - оркестрация сервисов
- **Uvicorn** - ASGI сервер для FastAPI

## 📁 Структура проекта сервера

```
server/
├── app/
│   ├── __init__.py
│   ├── main.py                 # Главный файл приложения
│   ├── schemas.py              # Pydantic схемы
│   ├── api/
│   │   ├── __init__.py
│   │   └── agents.py           # API эндпоинты для агентов
│   ├── database/
│   │   ├── __init__.py
│   │   ├── connection.py       # Подключение к БД
│   │   ├── models.py          # SQLAlchemy модели
│   │   └── api.py             # CRUD операции
│   ├── models/
│   │   ├── __init__.py
│   │   └── agent.py           # Модели агентов (legacy)
│   └── services/
│       ├── __init__.py
│       └── agent_service.py   # Бизнес-логика (legacy)
├── Dockerfile                 # Docker образ сервера
├── requirements.txt           # Python зависимости
└── env_example.txt           # Пример переменных окружения
```

## 🚀 Основные функции

### Сбор метрик
- Прием метрик от агентов
- Валидация данных
- Сохранение в базу данных
- Обработка различных типов метрик

### Управление агентами
- Регистрация новых агентов
- Отслеживание статуса агентов
- Управление конфигурацией агентов
- Heartbeat мониторинг

### API для агентов
- RESTful API для взаимодействия
- Асинхронная обработка запросов
- Автоматическая документация (Swagger)
- Валидация входных данных

### Хранение данных
- Структурированное хранение метрик
- Индексы для быстрого поиска
- Связи между таблицами
- Поддержка JSONB для детальных данных

## 🔌 API эндпоинты

### Информация о сервере
```http
GET /
```
Возвращает информацию о сервере и доступных эндпоинтах.

### Управление агентами

**Регистрация агента:**
```http
POST /api/v1/agents
Content-Type: application/json

{
  "agent_id": "unique_agent_id",
  "machine_name": "My Computer",
  "server_url": "http://localhost:8000"
}
```

**Получение списка агентов:**
```http
GET /api/v1/agents
```

**Получение информации об агенте:**
```http
GET /api/v1/agents/{agent_id}
```

**Обновление heartbeat:**
```http
PUT /api/v1/agents/{agent_id}/heartbeat
```

**Удаление агента:**
```http
DELETE /api/v1/agents/{agent_id}
```

### Управление метриками

**Отправка метрики:**
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

**Получение метрик агента:**
```http
GET /api/v1/agents/{agent_id}/metrics?metric_type=cpu&limit=100
```

**Получение сводки метрик:**
```http
GET /api/v1/agents/{agent_id}/metrics/summary?hours=24
```

### Управление параметрами

**Создание пользовательского параметра:**
```http
POST /api/v1/agents/{agent_id}/parameters
Content-Type: application/json

{
  "parameter_key": "custom_metric",
  "command": "echo 'Hello World'"
}
```

**Получение параметров агента:**
```http
GET /api/v1/agents/{agent_id}/parameters
```

### Справочники

**Получение интерпретаторов:**
```http
GET /api/v1/interpreters
```

**Получение типов метрик:**
```http
GET /api/v1/metric-types
```

## 📊 Типы метрик

Сервер поддерживает следующие типы метрик:

| Тип | Описание | Поля |
|-----|----------|------|
| `cpu` | Процессор | `usage_percent`, `temperature`, `details` |
| `memory` | Память | `usage_percent`, `total_bytes`, `used_bytes`, `free_bytes` |
| `disk` | Диски | `usage_percent`, `total_bytes`, `used_bytes`, `free_bytes` |
| `network` | Сеть | `details`, `network_connections` |
| `gpu` | Графический процессор | `usage_percent`, `temperature`, `details` |
| `hdd` | Жесткие диски | `temperature`, `details` |
| `user` | Пользовательские метрики | `details` |
| `inventory` | Инвентарь системы | `details` |

## 🔧 Конфигурация сервера

### Переменные окружения

| Переменная | Описание | По умолчанию |
|------------|----------|--------------|
| `DATABASE_URL` | URL подключения к PostgreSQL | `postgresql+asyncpg://agent_user:agent_password@localhost:5432/monitoring_agent` |
| `HOST` | Хост для привязки сервера | `0.0.0.0` |
| `PORT` | Порт сервера | `8000` |
| `LOG_LEVEL` | Уровень логирования | `INFO` |

### Файл конфигурации

Создайте файл `.env` в папке `server/`:

```env
# Конфигурация базы данных
DATABASE_URL=postgresql+asyncpg://agent_user:agent_password@localhost:5432/monitoring_agent

# Настройки сервера
HOST=0.0.0.0
PORT=8000

# Настройки логирования
LOG_LEVEL=INFO
```

## 🐳 Docker контейнеризация

### Dockerfile сервера

```dockerfile
FROM python:3.11-slim

WORKDIR /app

# Установка системных зависимостей
RUN apt-get update && apt-get install -y \
    gcc \
    && rm -rf /var/lib/apt/lists/*

# Установка Python зависимостей
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Копирование кода приложения
COPY . .

# Создание директории для логов
RUN mkdir -p /app/logs

# Открытие порта
EXPOSE 8000

# Команда запуска
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000", "--reload"]
```

### Docker Compose

```yaml
services:
  postgres:
    image: postgres:15
    container_name: monitoring_postgres
    restart: unless-stopped
    ports:
      - "5432:5432"
    environment:
      POSTGRES_DB: monitoring_agent
      POSTGRES_USER: agent_user
      POSTGRES_PASSWORD: agent_password
    volumes:
      - postgres_data:/var/lib/postgresql/data
      - ./database/schema.sql:/docker-entrypoint-initdb.d/01_schema.sql:ro
    networks:
      - monitoring_network
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U agent_user -d monitoring_agent"]
      interval: 30s
      timeout: 10s
      retries: 3

  server:
    build: ./server
    container_name: monitoring_server
    restart: unless-stopped
    ports:
      - "8000:8000"
    environment:
      DATABASE_URL: postgresql+asyncpg://agent_user:agent_password@postgres:5432/monitoring_agent
    depends_on:
      postgres:
        condition: service_healthy
    networks:
      - monitoring_network
    volumes:
      - ./server:/app
      - ./logs:/app/logs

volumes:
  postgres_data:
    driver: local

networks:
  monitoring_network:
    driver: bridge
```

## 📈 Производительность

### Оптимизации

- **Асинхронная обработка** - FastAPI использует async/await
- **Connection pooling** - пул соединений с PostgreSQL
- **Индексы БД** - оптимизированные индексы для быстрого поиска
- **JSONB** - эффективное хранение JSON данных
- **Lazy loading** - загрузка связанных данных по требованию

### Масштабирование

- **Горизонтальное масштабирование** - запуск нескольких экземпляров сервера
- **Load balancer** - распределение нагрузки между серверами
- **Database sharding** - разделение данных по агентам
- **Caching** - кэширование часто запрашиваемых данных

## 🔒 Безопасность

### Аутентификация и авторизация
- **API ключи** - для аутентификации агентов
- **HTTPS** - шифрование трафика
- **Rate limiting** - ограничение частоты запросов
- **Input validation** - валидация всех входных данных

### Защита данных
- **SQL injection** - защита через ORM
- **XSS** - защита от межсайтового скриптинга
- **CORS** - настройка политик CORS
- **Data encryption** - шифрование чувствительных данных

## 📝 Логирование

### Уровни логирования
- **DEBUG** - детальная отладочная информация
- **INFO** - общая информация о работе
- **WARNING** - предупреждения
- **ERROR** - ошибки
- **CRITICAL** - критические ошибки

### Формат логов
```
2025-09-06 10:30:15 [INFO] Server started on http://0.0.0.0:8000
2025-09-06 10:30:16 [INFO] Agent agent_001 registered
2025-09-06 10:30:17 [INFO] Received metrics from agent_001
2025-09-06 10:30:18 [ERROR] Failed to save metric: Database connection lost
```

## 🐛 Устранение неполадок

### Сервер не запускается
1. Проверьте доступность порта 8000
2. Убедитесь, что PostgreSQL запущен
3. Проверьте переменные окружения
4. Посмотрите логи: `docker logs monitoring_server`

### Ошибки подключения к БД
1. Проверьте строку подключения `DATABASE_URL`
2. Убедитесь, что PostgreSQL доступен
3. Проверьте права пользователя БД
4. Проверьте сетевую связность

### Медленная работа API
1. Проверьте индексы в базе данных
2. Оптимизируйте запросы
3. Увеличьте пул соединений
4. Добавьте кэширование

## 📚 Дополнительные ресурсы

- [Агент мониторинга](agent.md)
- [База данных](database.md)
- [Развертывание](deployment.md)
- [FastAPI документация](https://fastapi.tiangolo.com/)
- [SQLAlchemy документация](https://docs.sqlalchemy.org/)
