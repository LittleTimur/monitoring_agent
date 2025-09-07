# Monitoring Agent

Кроссплатформенная система мониторинга с агентом (C++) и сервером (FastAPI + PostgreSQL).

## 📚 Документация

Вся подробная документация находится в папке `docs/`:

- **[Агент](docs/agent.md)** - Сборка, конфигурация и API агента
- **[Сервер](docs/server.md)** - Описание сервера мониторинга и его архитектуры  
- **[База данных](docs/database.md)** - Схема БД, таблицы и структура данных
- **[Запуск](docs/deployment.md)** - Инструкции по развертыванию системы
- **[Продакшен](docs/production.md)** - Развертывание в production среде

## 🚀 Быстрый старт

1. **Запуск сервера:**
   ```bash
   docker-compose up -d
   ```

2. **Сборка агента:**
   ```bash
   mkdir build && cd build
   cmake .. && make
   ```

3. **Запуск агента:**
   ```bash
   ./monitoring_agent
   ```

## 📊 Архитектура

```
Monitoring Agent (C++)     →     Monitoring Server (FastAPI)
     ↓                              ↓
Сбор метрик                    REST API + PostgreSQL
     ↓                              ↓
Отправка на сервер              Веб-интерфейс + Алерты
```

## 📄 Лицензия


MIT License