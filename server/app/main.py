from fastapi import FastAPI, HTTPException, BackgroundTasks, Request
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import uvicorn
from datetime import datetime
import json
from typing import Optional, Dict, Any
import os

# Импорт модулей БД
from .database.connection import init_db, close_db
from .api.agents import router as agents_router

# Создаем FastAPI приложение
app = FastAPI(
    title="Monitoring Server",
    description="Сервер для сбора и управления метриками от агентов мониторинга",
    version="1.0.0"
)

# Настройка CORS для веб-интерфейса
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # В продакшене указать конкретные домены
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Модели данных
class MetricsData(BaseModel):
    timestamp: float
    machine_type: str
    agent_id: Optional[str] = None
    machine_name: Optional[str] = None
    cpu: Optional[Dict[str, Any]] = None
    memory: Optional[Dict[str, Any]] = None
    disk: Optional[Dict[str, Any]] = None
    network: Optional[Dict[str, Any]] = None
    gpu: Optional[Dict[str, Any]] = None
    hdd: Optional[Dict[str, Any]] = None
    user: Optional[Dict[str, Any]] = None
    inventory: Optional[Dict[str, Any]] = None

# Подключаем роутеры
app.include_router(agents_router)

# События жизненного цикла приложения
@app.on_event("startup")
async def startup_event():
    """Инициализация при запуске"""
    print("🚀 Запуск Monitoring Server...")
    try:
        await init_db()
        print("✅ База данных инициализирована")
    except Exception as e:
        print(f"❌ Ошибка инициализации БД: {e}")

@app.on_event("shutdown")
async def shutdown_event():
    """Очистка при завершении"""
    print("🛑 Завершение работы Monitoring Server...")
    await close_db()
    print("✅ Соединения с БД закрыты")

@app.get("/")
async def root():
    """Главная страница"""
    return {
        "message": "Monitoring Server API",
        "version": "1.0.0",
        "database": "PostgreSQL",
        "endpoints": {
            "agents": "/api/v1/agents",
            "metrics": "/metrics",
            "health": "/health",
            "docs": "/docs"
        }
    }

@app.post("/metrics")
async def receive_metrics(metrics: MetricsData, request: Request):
    """Получение метрик от агента (устаревший endpoint - используйте /api/v1/agents/{agent_id}/metrics)"""
    try:
        print(f"📊 Получены метрики от агента")
        print(f"   Timestamp: {metrics.timestamp}")
        print(f"   Machine type: {metrics.machine_type}")
        print(f"   Agent ID: {metrics.agent_id}")
        print(f"   Machine name: {metrics.machine_name}")
        
        # Генерируем ID агента, если не указан
        agent_id = metrics.agent_id or f"agent_{int(metrics.timestamp)}"
        print(f"   Используемый Agent ID: {agent_id}")
        
        # Получаем IP адрес агента из запроса
        client_ip = request.client.host if request.client else "127.0.0.1"
        print(f"   Client IP: {client_ip}")
        
        # --- ЛОГИРОВАНИЕ В ФАЙЛ (временно) ---
        log_path = os.path.join(os.path.dirname(__file__), "metrics_log.jsonl")
        with open(log_path, "a", encoding="utf-8") as f:
            f.write(json.dumps(metrics.dict(), ensure_ascii=False) + "\n")
        
        print(f"💾 Метрики сохранены для агента {agent_id}")
        return {
            "status": "success", 
            "message": "Metrics received (legacy endpoint)",
            "note": "Use /api/v1/agents/{agent_id}/metrics for new API"
        }
    except Exception as e:
        print(f"❌ Ошибка при обработке метрик: {e}")
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/health")
async def health_check():
    """Проверка состояния сервера"""
    return {
        "status": "healthy",
        "timestamp": datetime.now().isoformat(),
        "database": "PostgreSQL",
        "version": "1.0.0"
    }

@app.exception_handler(404)
async def not_found_handler(request: Request, exc: HTTPException):
    """Обработчик для несуществующих эндпоинтов"""
    print(f"❌ 404 ошибка для {request.method} {request.url}")
    return {"detail": "Not Found", "path": str(request.url.path)}

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000) 