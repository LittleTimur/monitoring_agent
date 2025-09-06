# Продакшен развертывание

Руководство по развертыванию системы мониторинга в продакшене. Описание файлов для переноса, настройки безопасности и production конфигурации.

## 📦 Файлы для переноса

### Обязательные файлы

**1. Основные файлы проекта:**
```
monitoring_agent/
├── docker-compose.yml              # Оркестрация сервисов
├── docker-compose.prod.yml         # Production конфигурация
├── .env.prod                       # Production переменные окружения
├── .dockerignore                   # Исключения для Docker
├── README.md                       # Основная документация
└── docs/                          # Документация
    ├── agent.md
    ├── server.md
    ├── database.md
    └── deployment.md
```

**2. Сервер (FastAPI):**
```
server/
├── Dockerfile                      # Образ сервера
├── requirements.txt                # Python зависимости
├── .env.prod                       # Production переменные
├── app/
│   ├── main.py                     # Главный файл приложения
│   ├── schemas.py                  # Pydantic схемы
│   ├── api/
│   │   └── agents.py              # API эндпоинты
│   ├── database/
│   │   ├── connection.py          # Подключение к БД
│   │   ├── models.py              # SQLAlchemy модели
│   │   └── api.py                 # CRUD операции
│   └── services/
│       └── agent_service.py       # Бизнес-логика
```

**3. База данных:**
```
database/
├── schema.sql                      # Объединенная схема БД
├── migrations/
│   ├── 001_initial_schema.sql     # Основные таблицы
│   └── 002_metrics_tables.sql     # Таблицы метрик
├── seed_data.sql                   # Начальные данные
└── backup/                        # Скрипты резервного копирования
    ├── backup.sh
    └── restore.sh
```

**4. Агент (C++):**
```
# Скомпилированные бинарники
monitoring_agent.exe                # Windows
monitoring_agent                    # Linux

# Конфигурация
agent_config.json                  # Конфигурация агента
scripts/                           # Пользовательские скрипты
```

### Опциональные файлы

**1. Мониторинг и логирование:**
```
monitoring/
├── prometheus.yml                  # Конфигурация Prometheus
├── grafana/
│   └── dashboards/                # Дашборды Grafana
└── alertmanager.yml               # Конфигурация алертов

logs/                              # Директория для логов
├── server/
└── agent/
```

**2. SSL сертификаты:**
```
ssl/
├── server.crt                      # SSL сертификат
├── server.key                      # SSL ключ
└── ca.crt                         # CA сертификат
```

**3. Скрипты развертывания:**
```
scripts/
├── deploy.sh                      # Скрипт развертывания
├── backup.sh                      # Резервное копирование
├── update.sh                      # Обновление системы
└── health_check.sh                # Проверка здоровья
```

## 🚀 Production конфигурация

### 1. Docker Compose для продакшена

**docker-compose.prod.yml:**
```yaml
version: '3.8'

services:
  # PostgreSQL Database
  postgres:
    image: postgres:15
    container_name: monitoring_postgres_prod
    restart: always
    ports:
      - "5432:5432"
    environment:
      POSTGRES_DB: ${POSTGRES_DB}
      POSTGRES_USER: ${POSTGRES_USER}
      POSTGRES_PASSWORD: ${POSTGRES_PASSWORD}
      POSTGRES_INITDB_ARGS: "--auth-host=scram-sha-256"
    volumes:
      - postgres_data:/var/lib/postgresql/data
      - ./database/schema.sql:/docker-entrypoint-initdb.d/01_schema.sql:ro
      - ./database/backup:/backup:ro
    networks:
      - monitoring_network
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U ${POSTGRES_USER} -d ${POSTGRES_DB}"]
      interval: 30s
      timeout: 10s
      retries: 3
    deploy:
      resources:
        limits:
          memory: 1G
        reservations:
          memory: 512M

  # FastAPI Server
  server:
    build: 
      context: ./server
      dockerfile: Dockerfile.prod
    container_name: monitoring_server_prod
    restart: always
    ports:
      - "8000:8000"
    environment:
      DATABASE_URL: postgresql+asyncpg://${POSTGRES_USER}:${POSTGRES_PASSWORD}@postgres:5432/${POSTGRES_DB}
      HOST: 0.0.0.0
      PORT: 8000
      LOG_LEVEL: INFO
      WORKERS: 4
      SSL_CERT_PATH: /app/ssl/server.crt
      SSL_KEY_PATH: /app/ssl/server.key
    depends_on:
      postgres:
        condition: service_healthy
    networks:
      - monitoring_network
    volumes:
      - ./logs/server:/app/logs
      - ./ssl:/app/ssl:ro
    deploy:
      resources:
        limits:
          memory: 512M
        reservations:
          memory: 256M

  # Nginx Reverse Proxy
  nginx:
    image: nginx:alpine
    container_name: monitoring_nginx_prod
    restart: always
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx/nginx.conf:/etc/nginx/nginx.conf:ro
      - ./nginx/ssl:/etc/nginx/ssl:ro
      - ./logs/nginx:/var/log/nginx
    depends_on:
      - server
    networks:
      - monitoring_network

volumes:
  postgres_data:
    driver: local

networks:
  monitoring_network:
    driver: bridge
```

### 2. Production Dockerfile

**server/Dockerfile.prod:**
```dockerfile
FROM python:3.11-slim

# Установка системных зависимостей
RUN apt-get update && apt-get install -y \
    gcc \
    && rm -rf /var/lib/apt/lists/*

# Создание пользователя для безопасности
RUN groupadd -r appuser && useradd -r -g appuser appuser

# Установка рабочей директории
WORKDIR /app

# Копирование и установка зависимостей
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Копирование кода приложения
COPY . .

# Создание директорий и установка прав
RUN mkdir -p /app/logs && \
    chown -R appuser:appuser /app

# Переключение на непривилегированного пользователя
USER appuser

# Открытие порта
EXPOSE 8000

# Команда запуска с Gunicorn
CMD ["gunicorn", "app.main:app", "-w", "4", "-k", "uvicorn.workers.UvicornWorker", "--bind", "0.0.0.0:8000", "--access-logfile", "-", "--error-logfile", "-"]
```

### 3. Production переменные окружения

**.env.prod:**
```env
# База данных
POSTGRES_DB=monitoring_agent_prod
POSTGRES_USER=monitoring_user
POSTGRES_PASSWORD=your_secure_password_here

# Сервер
DATABASE_URL=postgresql+asyncpg://monitoring_user:your_secure_password_here@postgres:5432/monitoring_agent_prod
HOST=0.0.0.0
PORT=8000
LOG_LEVEL=INFO
WORKERS=4

# SSL
SSL_CERT_PATH=/app/ssl/server.crt
SSL_KEY_PATH=/app/ssl/server.key

# Безопасность
SECRET_KEY=your_secret_key_here
JWT_SECRET_KEY=your_jwt_secret_key_here

# Мониторинг
PROMETHEUS_ENABLED=true
PROMETHEUS_PORT=9090
```

### 4. Nginx конфигурация

**nginx/nginx.conf:**
```nginx
events {
    worker_connections 1024;
}

http {
    upstream backend {
        server server:8000;
    }

    # Rate limiting
    limit_req_zone $binary_remote_addr zone=api:10m rate=10r/s;

    server {
        listen 80;
        server_name your-domain.com;
        return 301 https://$server_name$request_uri;
    }

    server {
        listen 443 ssl http2;
        server_name your-domain.com;

        # SSL конфигурация
        ssl_certificate /etc/nginx/ssl/server.crt;
        ssl_certificate_key /etc/nginx/ssl/server.key;
        ssl_protocols TLSv1.2 TLSv1.3;
        ssl_ciphers ECDHE-RSA-AES256-GCM-SHA512:DHE-RSA-AES256-GCM-SHA512:ECDHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES256-GCM-SHA384;
        ssl_prefer_server_ciphers off;

        # Security headers
        add_header X-Frame-Options DENY;
        add_header X-Content-Type-Options nosniff;
        add_header X-XSS-Protection "1; mode=block";
        add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;

        # Rate limiting
        limit_req zone=api burst=20 nodelay;

        # Proxy configuration
        location / {
            proxy_pass http://backend;
            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
            proxy_set_header X-Forwarded-Proto $scheme;
            
            # Timeouts
            proxy_connect_timeout 30s;
            proxy_send_timeout 30s;
            proxy_read_timeout 30s;
        }

        # Health check
        location /health {
            access_log off;
            proxy_pass http://backend/health;
        }

        # Static files (если есть)
        location /static/ {
            alias /app/static/;
            expires 1y;
            add_header Cache-Control "public, immutable";
        }
    }
}
```

## 🔒 Безопасность в продакшене

### 1. SSL сертификаты

**Генерация самоподписанного сертификата:**
```bash
# Создание директории для SSL
mkdir -p ssl

# Генерация приватного ключа
openssl genrsa -out ssl/server.key 2048

# Генерация сертификата
openssl req -new -x509 -key ssl/server.key -out ssl/server.crt -days 365 -subj "/C=RU/ST=Moscow/L=Moscow/O=Company/CN=your-domain.com"
```

**Использование Let's Encrypt:**
```bash
# Установка certbot
sudo apt install certbot

# Получение сертификата
sudo certbot certonly --standalone -d your-domain.com

# Копирование сертификатов
sudo cp /etc/letsencrypt/live/your-domain.com/fullchain.pem ssl/server.crt
sudo cp /etc/letsencrypt/live/your-domain.com/privkey.pem ssl/server.key
```

### 2. Firewall настройки

**Ubuntu/Debian:**
```bash
# Установка UFW
sudo apt install ufw

# Базовые правила
sudo ufw default deny incoming
sudo ufw default allow outgoing

# Разрешение SSH
sudo ufw allow ssh

# Разрешение HTTP/HTTPS
sudo ufw allow 80/tcp
sudo ufw allow 443/tcp

# Разрешение для мониторинга (опционально)
sudo ufw allow from 10.0.0.0/8 to any port 5432

# Включение firewall
sudo ufw enable
```

**RHEL/CentOS/Fedora:**
```bash
# Настройка firewalld
sudo systemctl start firewalld
sudo systemctl enable firewalld

# Разрешение сервисов
sudo firewall-cmd --permanent --add-service=ssh
sudo firewall-cmd --permanent --add-service=http
sudo firewall-cmd --permanent --add-service=https

# Применение правил
sudo firewall-cmd --reload
```

### 3. Безопасность базы данных

**Настройка PostgreSQL:**
```bash
# Редактирование postgresql.conf
sudo nano /etc/postgresql/15/main/postgresql.conf

# Настройки безопасности
listen_addresses = 'localhost'
ssl = on
ssl_cert_file = 'server.crt'
ssl_key_file = 'server.key'
password_encryption = scram-sha-256
```

**Настройка pg_hba.conf:**
```bash
# Редактирование pg_hba.conf
sudo nano /etc/postgresql/15/main/pg_hba.conf

# Добавление правил
local   all             all                                     scram-sha-256
host    all             all             127.0.0.1/32            scram-sha-256
host    all             all             ::1/128                 scram-sha-256
```

## 📊 Мониторинг в продакшене

### 1. Prometheus конфигурация

**monitoring/prometheus.yml:**
```yaml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

scrape_configs:
  - job_name: 'monitoring-server'
    static_configs:
      - targets: ['server:8000']
    metrics_path: '/metrics'
    scrape_interval: 30s

  - job_name: 'postgres'
    static_configs:
      - targets: ['postgres:5432']
    scrape_interval: 30s

  - job_name: 'nginx'
    static_configs:
      - targets: ['nginx:80']
    scrape_interval: 30s
```

### 2. Grafana дашборды

**monitoring/grafana/dashboards/monitoring.json:**
```json
{
  "dashboard": {
    "title": "Monitoring System Dashboard",
    "panels": [
      {
        "title": "Active Agents",
        "type": "stat",
        "targets": [
          {
            "expr": "count(agents_total)"
          }
        ]
      },
      {
        "title": "Metrics Rate",
        "type": "graph",
        "targets": [
          {
            "expr": "rate(metrics_received_total[5m])"
          }
        ]
      }
    ]
  }
}
```

## 🚀 Развертывание в продакшене

### 1. Подготовка сервера

**Обновление системы:**
```bash
# Ubuntu/Debian
sudo apt update && sudo apt upgrade -y

# RHEL/CentOS/Fedora
sudo yum update -y
```

**Установка Docker:**
```bash
# Установка Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# Установка Docker Compose
sudo curl -L "https://github.com/docker/compose/releases/download/v2.20.0/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose
```

### 2. Развертывание системы

**Клонирование и настройка:**
```bash
# Клонирование репозитория
git clone <repository-url>
cd monitoring_agent

# Копирование production конфигурации
cp .env.prod .env
cp docker-compose.prod.yml docker-compose.yml

# Генерация SSL сертификатов
mkdir -p ssl
openssl req -x509 -newkey rsa:4096 -keyout ssl/server.key -out ssl/server.crt -days 365 -nodes -subj "/C=RU/ST=Moscow/L=Moscow/O=Company/CN=your-domain.com"

# Создание директорий для логов
mkdir -p logs/{server,nginx,agent}
```

**Запуск системы:**
```bash
# Запуск в production режиме
docker-compose up -d

# Проверка статуса
docker-compose ps

# Просмотр логов
docker-compose logs -f
```

### 3. Проверка развертывания

**Проверка сервисов:**
```bash
# Проверка API
curl -k https://localhost/

# Проверка базы данных
docker exec monitoring_postgres_prod psql -U monitoring_user -d monitoring_agent_prod -c "SELECT COUNT(*) FROM agents;"

# Проверка логов
docker-compose logs server | tail -20
```

**Проверка безопасности:**
```bash
# Проверка SSL
openssl s_client -connect localhost:443 -servername your-domain.com

# Проверка firewall
sudo ufw status

# Проверка процессов
ps aux | grep -E "(docker|postgres|nginx)"
```

## 🔧 Обслуживание в продакшене

### 1. Резервное копирование

**Автоматическое резервное копирование:**
```bash
#!/bin/bash
# backup.sh

BACKUP_DIR="/backup/$(date +%Y%m%d)"
mkdir -p $BACKUP_DIR

# Резервное копирование базы данных
docker exec monitoring_postgres_prod pg_dump -U monitoring_user monitoring_agent_prod > $BACKUP_DIR/database.sql

# Резервное копирование конфигурации
tar -czf $BACKUP_DIR/config.tar.gz docker-compose.yml .env ssl/ nginx/

# Удаление старых бэкапов (старше 30 дней)
find /backup -type d -mtime +30 -exec rm -rf {} \;
```

**Настройка cron:**
```bash
# Добавление в crontab
echo "0 2 * * * /path/to/backup.sh" | crontab -
```

### 2. Обновление системы

**Скрипт обновления:**
```bash
#!/bin/bash
# update.sh

# Создание бэкапа перед обновлением
./backup.sh

# Получение обновлений
git pull origin main

# Пересборка и перезапуск
docker-compose down
docker-compose build --no-cache
docker-compose up -d

# Проверка работоспособности
sleep 30
curl -k https://localhost/health
```

### 3. Мониторинг логов

**Настройка logrotate:**
```bash
# /etc/logrotate.d/monitoring
/var/log/monitoring/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 644 root root
    postrotate
        docker-compose restart nginx
    endscript
}
```

## 📋 Чек-лист развертывания

### Перед развертыванием
- [ ] Подготовлен сервер с достаточными ресурсами
- [ ] Установлен Docker и Docker Compose
- [ ] Настроен firewall
- [ ] Подготовлены SSL сертификаты
- [ ] Созданы production конфигурации
- [ ] Настроено резервное копирование

### После развертывания
- [ ] Проверена работа всех сервисов
- [ ] Проверена безопасность (SSL, firewall)
- [ ] Настроен мониторинг
- [ ] Проверены логи
- [ ] Протестированы API эндпоинты
- [ ] Настроены алерты

### Регулярное обслуживание
- [ ] Мониторинг использования ресурсов
- [ ] Проверка логов на ошибки
- [ ] Обновление системы
- [ ] Резервное копирование
- [ ] Очистка старых данных
- [ ] Проверка безопасности

## 📚 Дополнительные ресурсы

- [Агент мониторинга](agent.md)
- [Сервер мониторинга](server.md)
- [База данных](database.md)
- [Развертывание](deployment.md)
- [Docker Production Best Practices](https://docs.docker.com/develop/dev-best-practices/)
- [PostgreSQL Security](https://www.postgresql.org/docs/current/security.html)
