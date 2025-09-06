# Развертывание системы

Полное руководство по развертыванию системы мониторинга, включая сервер, базу данных и агентов.

## 🚀 Быстрый старт

### 1. Клонирование репозитория
```bash
git clone <repository-url>
cd monitoring_agent
```

### 2. Запуск сервера и базы данных
```bash
docker-compose up -d
```

### 3. Проверка работы
```bash
curl http://localhost:8000/
```

## 🐳 Развертывание с Docker

### Требования
- Docker ≥ 20.10
- Docker Compose ≥ 2.0
- 2 GB свободного места на диске
- 1 GB RAM

### Полное развертывание

**1. Клонирование и подготовка:**
```bash
git clone <repository-url>
cd monitoring_agent
```

**2. Настройка переменных окружения:**
```bash
# Создание файла .env (опционально)
cat > .env << EOF
POSTGRES_DB=monitoring_agent
POSTGRES_USER=agent_user
POSTGRES_PASSWORD=agent_password
EOF
```

**3. Запуск системы:**
```bash
# Запуск в фоновом режиме
docker-compose up -d

# Проверка статуса
docker-compose ps

# Просмотр логов
docker-compose logs -f
```

**4. Проверка работы:**
```bash
# Проверка API
curl http://localhost:8000/

# Проверка базы данных
docker exec -it monitoring_postgres psql -U agent_user -d monitoring_agent -c "SELECT COUNT(*) FROM agents;"
```

### Остановка системы
```bash
# Остановка контейнеров
docker-compose down

# Остановка с удалением данных
docker-compose down -v
```

## 🖥️ Развертывание агента

### Сборка агента

**Windows:**
```powershell
# Установка зависимостей
choco install cmake git visualstudio2019buildtools

# Сборка
mkdir build
cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release
```

**Linux:**
```bash
# Установка зависимостей
sudo apt update
sudo apt install -y build-essential cmake git pkg-config libcurl4-openssl-dev

# Сборка
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Конфигурация агента

**Создание конфигурационного файла:**
```bash
cat > agent_config.json << EOF
{
  "agent_id": "agent_001",
  "machine_name": "My Computer",
  "server_url": "http://localhost:8000",
  "update_frequency": 60,
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
EOF
```

### Запуск агента

**Windows:**
```powershell
# Простой запуск
.\monitoring_agent.exe

# Запуск в фоне
Start-Process -FilePath ".\monitoring_agent.exe" -WindowStyle Hidden
```

**Linux:**
```bash
# Простой запуск
./monitoring_agent

# Запуск в фоне
nohup ./monitoring_agent > metrics.log 2>&1 &

# Запуск как служба
sudo cp monitoring_agent /usr/local/bin/
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

sudo systemctl daemon-reload
sudo systemctl enable monitoring-agent
sudo systemctl start monitoring-agent
```

## 🗄️ Развертывание базы данных

### Локальная установка PostgreSQL

**Windows:**
```powershell
# Установка через Chocolatey
choco install postgresql

# Или скачать с официального сайта
# https://www.postgresql.org/download/windows/
```

**Ubuntu/Debian:**
```bash
# Установка PostgreSQL
sudo apt update
sudo apt install -y postgresql postgresql-contrib

# Запуск службы
sudo systemctl start postgresql
sudo systemctl enable postgresql
```

**RHEL/CentOS/Fedora:**
```bash
# Установка PostgreSQL
sudo yum install -y postgresql-server postgresql-contrib

# Инициализация базы данных
sudo postgresql-setup initdb

# Запуск службы
sudo systemctl start postgresql
sudo systemctl enable postgresql
```

### Настройка базы данных

**1. Создание пользователя и базы данных:**
```bash
# Подключение к PostgreSQL
sudo -u postgres psql

# Создание пользователя и базы данных
CREATE USER agent_user WITH PASSWORD 'agent_password';
CREATE DATABASE monitoring_agent OWNER agent_user;
GRANT ALL PRIVILEGES ON DATABASE monitoring_agent TO agent_user;
\q
```

**2. Применение схемы:**
```bash
# Применение схемы базы данных
psql -U agent_user -d monitoring_agent -f database/schema.sql
```

**3. Проверка:**
```bash
# Проверка таблиц
psql -U agent_user -d monitoring_agent -c "\dt"

# Проверка данных
psql -U agent_user -d monitoring_agent -c "SELECT * FROM interpreters;"
```

## 🌐 Развертывание сервера

### Локальная установка Python

**Windows:**
```powershell
# Установка Python
choco install python

# Установка зависимостей
pip install -r server/requirements.txt
```

**Linux:**
```bash
# Установка Python
sudo apt install -y python3 python3-pip

# Установка зависимостей
pip3 install -r server/requirements.txt
```

### Настройка сервера

**1. Создание файла конфигурации:**
```bash
cat > server/.env << EOF
DATABASE_URL=postgresql+asyncpg://agent_user:agent_password@localhost:5432/monitoring_agent
HOST=0.0.0.0
PORT=8000
LOG_LEVEL=INFO
EOF
```

**2. Запуск сервера:**
```bash
cd server
uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
```

**3. Проверка:**
```bash
# Проверка API
curl http://localhost:8000/

# Проверка документации
open http://localhost:8000/docs
```

## 🔧 Конфигурация системы

### Переменные окружения

**Основные переменные:**
```bash
# База данных
POSTGRES_DB=monitoring_agent
POSTGRES_USER=agent_user
POSTGRES_PASSWORD=agent_password

# Сервер
DATABASE_URL=postgresql+asyncpg://agent_user:agent_password@localhost:5432/monitoring_agent
HOST=0.0.0.0
PORT=8000
LOG_LEVEL=INFO
```

**Переменные для агента:**
```bash
# Конфигурация агента
AGENT_ID=agent_001
MACHINE_NAME=My Computer
SERVER_URL=http://localhost:8000
UPDATE_FREQUENCY=60
```

### Файлы конфигурации

**docker-compose.yml:**
```yaml
services:
  postgres:
    image: postgres:15
    container_name: monitoring_postgres
    restart: unless-stopped
    ports:
      - "5432:5432"
    environment:
      POSTGRES_DB: ${POSTGRES_DB:-monitoring_agent}
      POSTGRES_USER: ${POSTGRES_USER:-agent_user}
      POSTGRES_PASSWORD: ${POSTGRES_PASSWORD:-agent_password}
    volumes:
      - postgres_data:/var/lib/postgresql/data
      - ./database/schema.sql:/docker-entrypoint-initdb.d/01_schema.sql:ro
    networks:
      - monitoring_network
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U ${POSTGRES_USER:-agent_user} -d ${POSTGRES_DB:-monitoring_agent}"]
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
      DATABASE_URL: postgresql+asyncpg://${POSTGRES_USER:-agent_user}:${POSTGRES_PASSWORD:-agent_password}@postgres:5432/${POSTGRES_DB:-monitoring_agent}
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

**agent_config.json:**
```json
{
  "agent_id": "agent_001",
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

## 🔍 Мониторинг развертывания

### Проверка статуса системы

**1. Проверка контейнеров:**
```bash
docker-compose ps
```

**2. Проверка логов:**
```bash
# Логи сервера
docker-compose logs server

# Логи базы данных
docker-compose logs postgres

# Все логи
docker-compose logs -f
```

**3. Проверка API:**
```bash
# Информация о сервере
curl http://localhost:8000/

# Список агентов
curl http://localhost:8000/api/v1/agents

# Документация API
open http://localhost:8000/docs
```

**4. Проверка базы данных:**
```bash
# Подключение к БД
docker exec -it monitoring_postgres psql -U agent_user -d monitoring_agent

# Проверка таблиц
\dt

# Проверка данных
SELECT COUNT(*) FROM agents;
SELECT COUNT(*) FROM agent_metrics;
```

### Мониторинг производительности

**1. Использование ресурсов:**
```bash
# Статистика контейнеров
docker stats

# Использование диска
docker system df
```

**2. Логи производительности:**
```bash
# Логи сервера с уровнем DEBUG
docker-compose logs server | grep -i "performance\|slow\|timeout"
```

**3. Мониторинг базы данных:**
```sql
-- Активные соединения
SELECT * FROM pg_stat_activity;

-- Статистика таблиц
SELECT * FROM pg_stat_user_tables;

-- Размер базы данных
SELECT pg_size_pretty(pg_database_size('monitoring_agent'));
```

## 🔧 Обслуживание системы

### Резервное копирование

**1. Резервное копирование базы данных:**
```bash
# Создание бэкапа
docker exec monitoring_postgres pg_dump -U agent_user monitoring_agent > backup.sql

# Восстановление из бэкапа
docker exec -i monitoring_postgres psql -U agent_user monitoring_agent < backup.sql
```

**2. Резервное копирование конфигурации:**
```bash
# Копирование конфигурационных файлов
tar -czf config_backup.tar.gz docker-compose.yml server/.env agent_config.json
```

### Обновление системы

**1. Обновление кода:**
```bash
# Получение обновлений
git pull origin main

# Пересборка и перезапуск
docker-compose down
docker-compose build --no-cache
docker-compose up -d
```

**2. Обновление базы данных:**
```bash
# Применение миграций
psql -U agent_user -d monitoring_agent -f database/migrations/003_new_migration.sql
```

### Очистка системы

**1. Очистка старых данных:**
```sql
-- Удаление метрик старше 30 дней
DELETE FROM agent_metrics 
WHERE timestamp < NOW() - INTERVAL '30 days';

-- Очистка неактивных агентов
DELETE FROM agents 
WHERE last_heartbeat < NOW() - INTERVAL '7 days';
```

**2. Очистка Docker:**
```bash
# Удаление неиспользуемых образов
docker image prune -a

# Удаление неиспользуемых контейнеров
docker container prune

# Полная очистка
docker system prune -a
```

## 🐛 Устранение неполадок

### Проблемы с Docker

**1. Контейнеры не запускаются:**
```bash
# Проверка логов
docker-compose logs

# Проверка статуса
docker-compose ps

# Перезапуск
docker-compose restart
```

**2. Проблемы с сетью:**
```bash
# Проверка сетей
docker network ls

# Проверка подключения
docker exec monitoring_server ping postgres
```

**3. Проблемы с объемами:**
```bash
# Проверка объемов
docker volume ls

# Проверка использования
docker system df -v
```

### Проблемы с базой данных

**1. Ошибки подключения:**
```bash
# Проверка статуса PostgreSQL
docker exec monitoring_postgres pg_isready -U agent_user

# Проверка логов
docker-compose logs postgres
```

**2. Проблемы с миграциями:**
```bash
# Проверка схемы
docker exec monitoring_postgres psql -U agent_user -d monitoring_agent -c "\dt"

# Применение миграций
docker exec -i monitoring_postgres psql -U agent_user monitoring_agent < database/schema.sql
```

### Проблемы с агентом

**1. Агент не подключается к серверу:**
```bash
# Проверка подключения
curl http://localhost:8000/

# Проверка конфигурации
cat agent_config.json

# Проверка логов агента
tail -f metrics.log
```

**2. Метрики не отправляются:**
```bash
# Проверка API
curl http://localhost:8000/api/v1/agents

# Проверка базы данных
docker exec monitoring_postgres psql -U agent_user -d monitoring_agent -c "SELECT COUNT(*) FROM agent_metrics;"
```

## 📚 Дополнительные ресурсы

- [Агент мониторинга](agent.md)
- [Сервер мониторинга](server.md)
- [База данных](database.md)
- [Docker документация](https://docs.docker.com/)
- [PostgreSQL документация](https://www.postgresql.org/docs/)
