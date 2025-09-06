from sqlalchemy.ext.asyncio import create_async_engine, AsyncSession
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import NullPool
import os
from dotenv import load_dotenv

# Загружаем переменные окружения
load_dotenv()

# Конфигурация подключения к БД
DATABASE_URL = os.getenv(
    "DATABASE_URL", 
    "postgresql+asyncpg://agent_user:agent_password@localhost:5432/monitoring_agent"
)

# Создаем асинхронный движок
engine = create_async_engine(
    DATABASE_URL,
    echo=False,  # Логирование SQL запросов (False для продакшена)
    poolclass=NullPool,  # Отключаем пул для простоты
    future=True
)

# Создаем фабрику сессий
AsyncSessionLocal = sessionmaker(
    engine,
    class_=AsyncSession,
    expire_on_commit=False
)

async def get_db():
    """Dependency для получения сессии БД"""
    async with AsyncSessionLocal() as session:
        try:
            yield session
        finally:
            await session.close()

async def init_db():
    """Инициализация БД (создание таблиц)"""
    from .models import Base
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)

async def close_db():
    """Закрытие соединения с БД"""
    await engine.dispose()

