#!/bin/bash

# Скрипт для применения миграции 002 (таблицы метрик)
# Использование: ./apply_migration_002.sh [DATABASE_NAME] [HOST] [PORT] [USER]

set -e

# Параметры по умолчанию
DATABASE_NAME=${1:-"monitoring_agent"}
HOST=${2:-"localhost"}
PORT=${3:-"5432"}
USER=${4:-"postgres"}

echo "🔧 Применение миграции 002: Таблицы метрик..."
echo "📊 База данных: $DATABASE_NAME"
echo "🌐 Хост: $HOST:$PORT"
echo "👤 Пользователь: $USER"

# Проверяем, что PostgreSQL запущен
if ! pg_isready -h $HOST -p $PORT -U $USER -d $DATABASE_NAME -q; then
    echo "❌ PostgreSQL не запущен или недоступен"
    echo "💡 Убедитесь, что PostgreSQL запущен и доступен по адресу $HOST:$PORT"
    exit 1
fi

# Применяем миграцию
echo "📝 Применение миграции 002_metrics_tables.sql..."
psql -h $HOST -p $PORT -U $USER -d $DATABASE_NAME -f database/migrations/002_metrics_tables.sql

if [ $? -eq 0 ]; then
    echo "✅ Миграция 002 успешно применена!"
    echo "📊 Созданы таблицы:"
    echo "   - agent_metrics (основная таблица метрик)"
    echo "   - metrics_network_connections (сетевые соединения)"
    echo ""
    echo "🔍 Проверка созданных таблиц:"
    psql -h $HOST -p $PORT -U $USER -d $DATABASE_NAME -c "\dt agent_metrics"
    psql -h $HOST -p $PORT -U $USER -d $DATABASE_NAME -c "\dt metrics_network_connections"
else
    echo "❌ Ошибка при применении миграции"
    exit 1
fi
