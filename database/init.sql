-- Инициализация базы данных
-- Создание БД и пользователей

-- Создание базы данных (выполняется от имени postgres)
-- CREATE DATABASE monitoring_agent;

-- Создание пользователя (выполняется от имени postgres)
-- CREATE USER agent_user WITH PASSWORD 'your_secure_password';

-- Предоставление прав пользователю (выполняется от имени postgres)
-- GRANT ALL PRIVILEGES ON DATABASE monitoring_agent TO agent_user;

-- Подключение к БД monitoring_agent
\c monitoring_agent;

-- Применение схемы
\i schema.sql;

