#!/bin/bash
# Скрипт инициализации базы данных
# Создает БД, пользователей и применяет миграции

set -e

echo "🗄️  Инициализация PostgreSQL базы данных..."

# Проверка наличия PostgreSQL
if ! command -v psql &> /dev/null; then
    echo "❌ PostgreSQL не установлен. Установите PostgreSQL и попробуйте снова."
    exit 1
fi

# Переменные окружения
DB_NAME="monitoring_agent"
DB_USER="agent_user"
DB_PASSWORD="your_secure_password"

echo "📊 Создание базы данных: $DB_NAME"
echo "👤 Создание пользователя: $DB_USER"

# Создание базы данных и пользователя
psql -U postgres -c "CREATE DATABASE $DB_NAME;" || echo "База данных уже существует"
psql -U postgres -c "CREATE USER $DB_USER WITH PASSWORD '$DB_PASSWORD';" || echo "Пользователь уже существует"
psql -U postgres -c "GRANT ALL PRIVILEGES ON DATABASE $DB_NAME TO $DB_USER;"

echo "📋 Применение миграций..."

# Применение схемы
psql -U $DB_USER -d $DB_NAME -f database/schema.sql

echo "✅ База данных успешно инициализирована!"
echo "📊 База данных: $DB_NAME"
echo "👤 Пользователь: $DB_USER"
echo "🔗 Подключение: postgresql://$DB_USER:$DB_PASSWORD@localhost:5432/$DB_NAME"

