#!/usr/bin/env python3
"""
Запуск Monitoring Server
"""

import uvicorn
from app.main import app

if __name__ == "__main__":
    print("🚀 Запуск Monitoring Server...")
    print("📖 Документация API: http://localhost:8000/docs")
    print("🔍 Проверка здоровья: http://localhost:8000/health")
    print("📊 Список агентов: http://localhost:8000/api/agents")
    print("=" * 50)
    
    uvicorn.run(
        app,
        host="0.0.0.0",
        port=8000,
        reload=False,  # Отключаем reload для избежания предупреждения
        log_level="info"
    ) 