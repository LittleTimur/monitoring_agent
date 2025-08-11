from fastapi import FastAPI, HTTPException, BackgroundTasks, Request
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import uvicorn
from datetime import datetime
import json
from typing import Optional, Dict, Any
import os

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
            "send_command": "/api/agents/{agent_id}/command",
            "send_command_all": "/api/agents/command_all",
            "register_agent": "/api/agents/register",
            "register_agent_with_id": "/api/agents/{agent_id}/register",
            "docs": "/docs"
        }
    }

@app.post("/metrics")
async def receive_metrics(metrics: MetricsData, request: Request):
    """Получение метрик от агента"""
    try:
        print(f"📊 Получены метрики от агента")
        print(f"   Timestamp: {metrics.timestamp}")
        print(f"   Machine type: {metrics.machine_type}")
        print(f"   Agent ID: {metrics.agent_id}")
        print(f"   Machine name: {metrics.machine_name}")
        print(f"   Request URL: {request.url}")
        print(f"   Request method: {request.method}")
        print(f"   Request headers: {dict(request.headers)}")
        
        # Генерируем ID агента, если не указан
        agent_id = metrics.agent_id or f"agent_{int(metrics.timestamp)}"
        print(f"   Используемый Agent ID: {agent_id}")
        
        # Получаем IP адрес агента из запроса
        client_ip = request.client.host if request.client else "127.0.0.1"
        print(f"   Client IP: {client_ip}")
        
        # Обрабатываем метрики через сервис (регистрация + обновление)
        agent_service.process_agent_metrics(agent_id, metrics.dict(), client_ip)
        
        # Сохраняем метрики (пока в памяти)
        agents_data[agent_id] = {
            "last_update": datetime.now().isoformat(),
            "data": metrics.dict()
        }
        
        # --- ЛОГИРОВАНИЕ В ФАЙЛ ---
        log_path = os.path.join(os.path.dirname(__file__), "metrics_log.jsonl")
        with open(log_path, "a", encoding="utf-8") as f:
            f.write(json.dumps(metrics.dict(), ensure_ascii=False) + "\n")
        # --- КОНЕЦ ЛОГИРОВАНИЯ ---
        
        print(f"💾 Метрики сохранены для агента {agent_id}")
        return {"status": "success", "message": "Metrics received"}
    except Exception as e:
        print(f"❌ Ошибка при обработке метрик: {e}")
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



@app.post("/api/agents/{agent_id}/command")
async def send_command_to_agent(agent_id: str, command: Dict[str, Any]):
    """Отправка команды агенту"""
    try:
        from .models.agent import AgentCommand
        from .services.agent_service import agent_service
        
        # Создаем объект команды
        agent_command = AgentCommand(
            command=command.get("command", ""),
            data=command.get("data", {}),
            timestamp=datetime.now()
        )
        
        print(f"🚀 Отправка команды '{agent_command.command}' агенту {agent_id}")
        print(f"   Данные: {agent_command.data}")
        
        # Отправляем команду через сервис
        response = await agent_service.send_command_to_agent(agent_id, agent_command)
        
        if response.success:
            print(f"✅ Команда успешно отправлена агенту {agent_id}")
        else:
            print(f"❌ Ошибка отправки команды: {response.message}")
        
        return response.dict()
        
    except Exception as e:
        print(f"❌ Ошибка при отправке команды: {e}")
        raise HTTPException(status_code=500, detail=str(e))

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

@app.exception_handler(404)
async def not_found_handler(request: Request, exc: HTTPException):
    """Обработчик для несуществующих эндпоинтов"""
    print(f"❌ 404 ошибка для {request.method} {request.url}")
    return {"detail": "Not Found", "path": str(request.url.path)}

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000) 