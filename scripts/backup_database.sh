#!/bin/bash
# Скрипт резервного копирования базы данных
# Создает бэкап PostgreSQL БД

set -e

echo "💾 Создание резервной копии базы данных..."

# Переменные окружения
DB_NAME="monitoring_agent"
DB_USER="agent_user"
BACKUP_DIR="backups"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
BACKUP_FILE="${BACKUP_DIR}/monitoring_agent_${TIMESTAMP}.sql"

# Создание директории для бэкапов
mkdir -p $BACKUP_DIR

echo "📊 База данных: $DB_NAME"
echo "👤 Пользователь: $DB_USER"
echo "📁 Файл бэкапа: $BACKUP_FILE"

# Создание бэкапа
pg_dump -U $DB_USER -d $DB_NAME -f $BACKUP_FILE

# Сжатие бэкапа
gzip $BACKUP_FILE

echo "✅ Резервная копия создана: ${BACKUP_FILE}.gz"

# Удаление старых бэкапов (старше 30 дней)
find $BACKUP_DIR -name "*.gz" -mtime +30 -delete

echo "🧹 Старые бэкапы удалены (старше 30 дней)"
echo "📊 Размер бэкапа: $(du -h ${BACKUP_FILE}.gz | cut -f1)"

