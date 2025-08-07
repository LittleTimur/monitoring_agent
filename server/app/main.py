from fastapi import FastAPI, HTTPException, BackgroundTasks, Request
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import uvicorn
from datetime import datetime
import json
from typing import Optional, Dict, Any

# Импорт новых модулей
from .api.agents import router as agents_router
from .services.agent_service import agent_service
from .models.agent import AgentStatus, MetricType

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
    inventory: Optional[Dict[str, Any]] = None

# Хранилище данных (временно в памяти, потом заменим на БД)
agents_data = {}

# Подключаем роутеры
app.include_router(agents_router)

@app.get("/")
async def root():
    """Главная страница"""
    return {
        "message": "Monitoring Server API",
        "version": "1.0.0",
        "endpoints": {
            "metrics": "/metrics",
            "agents": "/api/agents",
            "agent_statistics": "/api/agents/statistics",
            "agent_config": "/api/agents/{agent_id}/config",
            "request_metrics": "/api/agents/{agent_id}/request_metrics",
            "request_all_metrics": "/api/agents/request_metrics_from_all",
            "restart_agent": "/api/agents/{agent_id}/restart",
            "stop_agent": "/api/agents/{agent_id}/stop",
            "docs": "/docs"
        }
    }

@app.post("/metrics")
async def receive_metrics(metrics: MetricsData, request: Request):
    """Получение метрик от агента"""
    try:
        # Генерируем ID агента, если не указан
        agent_id = metrics.agent_id or f"agent_{int(metrics.timestamp)}"
        
        # Получаем IP адрес агента из запроса
        client_ip = request.client.host if request.client else "127.0.0.1"
        
        # Регистрируем агента, если его нет
        if agent_id not in agent_service.agents:
            machine_name = metrics.machine_name or f"Machine_{agent_id}"
            agent_service.register_agent(
                agent_id=agent_id,
                machine_name=machine_name,
                machine_type=metrics.machine_type,
                ip_address=client_ip
            )
        
        # Обновляем статус агента
        agent_service.update_agent_status(agent_id, AgentStatus.ONLINE)
        
        # Сохраняем метрики (пока в памяти)
        agents_data[agent_id] = {
            "last_update": datetime.now().isoformat(),
            "data": metrics.dict()
        }
        
        return {"status": "success", "message": "Metrics received"}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/agents")
async def get_agents():
    """Получение списка всех агентов (устаревший endpoint)"""
    return {
        "agents": [
            {
                "id": agent_id,
                "last_update": data["last_update"],
                "machine_type": data["data"].get("machine_type", "Unknown")
            }
            for agent_id, data in agents_data.items()
        ]
    }

@app.get("/api/agents/{agent_id}")
async def get_agent(agent_id: str):
    """Получение данных конкретного агента (устаревший endpoint)"""
    if agent_id not in agents_data:
        raise HTTPException(status_code=404, detail="Agent not found")
    
    return agents_data[agent_id]

@app.get("/api/agents/{agent_id}/config")
async def get_agent_config(agent_id: str):
    """Получение конфигурации агента (устаревший endpoint)"""
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")
    
    return agent.config

@app.post("/api/agents/{agent_id}/config")
async def update_agent_config(agent_id: str, config: Dict[str, Any]):
    """Обновление конфигурации агента (устаревший endpoint)"""
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")
    
    # Обновляем конфигурацию
    agent.config.update_frequency = config.get("update_frequency", 60)
    agent.config.enabled_metrics = config.get("enabled_metrics", ["cpu", "memory", "disk", "network"])
    
    return {"status": "success", "message": "Configuration updated"}

@app.post("/api/agents/{agent_id}/execute_script")
async def execute_script(agent_id: str, script: str):
    """Выполнение скрипта на агенте (устаревший endpoint)"""
    # TODO: Реализовать выполнение скриптов
    return {
        "status": "success",
        "message": "Script execution requested",
        "script": script
    }

@app.get("/health")
async def health_check():
    """Проверка состояния сервера"""
    stats = agent_service.get_agent_statistics()
    return {
        "status": "healthy",
        "timestamp": datetime.now().isoformat(),
        "active_agents": stats["online_agents"],
        "total_agents": stats["total_agents"],
        "online_percentage": stats["online_percentage"]
    }

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000) 