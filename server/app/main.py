from fastapi import FastAPI, HTTPException, BackgroundTasks, Request
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import uvicorn
from datetime import datetime
import json
from typing import Optional, Dict, Any
import os

# Импорт модулей БД
from .database.connection import init_db, close_db, get_db
from .database.api import create_agent, agent_exists, save_metric, get_agent
from .api.agents import router as agents_router

def clean_null_characters(data):
    """Очищает null-символы из данных"""
    if isinstance(data, dict):
        return {k: clean_null_characters(v) for k, v in data.items()}
    elif isinstance(data, list):
        return [clean_null_characters(item) for item in data]
    elif isinstance(data, str):
        return data.replace('\u0000', '').replace('\x00', '')
    else:
        return data

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
    config: Optional[Dict[str, Any]] = None  # Конфигурация агента
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
    """Получение метрик от агента"""
    from .database.connection import get_db
    from .database.api import create_agent, save_metric, agent_exists
    from sqlalchemy.ext.asyncio import AsyncSession
    
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
        
        # Получаем сессию базы данных
        async for db in get_db():
            # Проверяем, существует ли агент
            if not await agent_exists(db, agent_id):
                # Создаем агента
                agent_data = {
                    "agent_id": agent_id,
                    "machine_name": metrics.machine_name or "Unknown Machine",
                    "agent_ip": client_ip,  # Используем реальный IP адрес агента 
                    "server_url": metrics.config.get("server_url", f"http://{client_ip}:8000"),  # Используем server_url из конфига агента
                    "auto_detect_id": True,
                    "auto_detect_name": True
                }
                await create_agent(db, agent_data)
                print(f"✅ Агент {agent_id} зарегистрирован")
            else:
                # Обновляем IP агента, если он изменился
                from .database.api import update_agent_config
                current_agent = await get_agent(db, agent_id)
                if current_agent and current_agent.agent_ip != client_ip:
                    await update_agent_config(db, agent_id, {"agent_ip": client_ip})
                    print(f"🔄 Обновлен IP агента {agent_id}: {client_ip}")
            
            # Обрабатываем конфигурацию агента, если она есть
            if metrics.config:
                from .database.api import (
                    update_agent_config, 
                    create_agent_enabled_metric, create_agent_allowed_interpreter, 
                    delete_agent_enabled_metrics, delete_agent_allowed_interpreters,
                    get_agent_enabled_metrics, get_agent_allowed_interpreters
                )
                
                # Получаем текущую конфигурацию агента из БД
                current_agent = await get_agent(db, agent_id)
                if current_agent:
                    # Сравниваем и обновляем только изменившиеся поля
                    config_changes = {}
                    
                    # Проверяем основные поля конфигурации
                    if metrics.config.get("update_frequency") != current_agent.update_frequency:
                        config_changes["update_frequency"] = metrics.config.get("update_frequency")
                    
                    if metrics.config.get("max_script_timeout_sec") != current_agent.max_script_timeout_sec:
                        config_changes["max_script_timeout_sec"] = metrics.config.get("max_script_timeout_sec")
                    
                    if metrics.config.get("max_output_bytes") != current_agent.max_output_bytes:
                        config_changes["max_output_bytes"] = metrics.config.get("max_output_bytes")
                    
                    if metrics.config.get("audit_log_enabled") != current_agent.audit_log_enabled:
                        config_changes["audit_log_enabled"] = metrics.config.get("audit_log_enabled")
                    
                    if metrics.config.get("audit_log_path") != current_agent.audit_log_path:
                        config_changes["audit_log_path"] = metrics.config.get("audit_log_path")
                    
                    # Обновляем command_server_url из конфигурации агента
                    if metrics.config.get("command_server_url") != current_agent.command_server_url:
                        config_changes["command_server_url"] = metrics.config.get("command_server_url")
                    
                    # Обновляем основные поля, если есть изменения
                    if config_changes:
                        await update_agent_config(db, agent_id, config_changes)
                        print(f"🔧 Обновлена конфигурация агента {agent_id}: {list(config_changes.keys())}")
                    
                    # Обрабатываем включенные метрики
                    if "enabled_metrics" in metrics.config:
                        # Получаем текущие метрики из БД
                        current_metrics = await get_agent_enabled_metrics(db, agent_id)
                        current_metric_names = {m.metric_name for m in current_metrics}
                        
                        # Получаем новые метрики от агента
                        new_metrics = metrics.config["enabled_metrics"]
                        if isinstance(new_metrics, dict):
                            new_metric_names = {name for name, enabled in new_metrics.items() if enabled}
                        elif isinstance(new_metrics, list):
                            new_metric_names = set(new_metrics)
                        else:
                            new_metric_names = set()
                        
                        # Обновляем только если есть различия
                        if current_metric_names != new_metric_names:
                            await delete_agent_enabled_metrics(db, agent_id)
                            for metric_name in new_metric_names:
                                await create_agent_enabled_metric(db, agent_id, metric_name)
                            print(f"📊 Обновлены метрики агента {agent_id}: {sorted(new_metric_names)}")
                    
                    # Обрабатываем разрешенные интерпретаторы
                    if "allowed_interpreters" in metrics.config:
                        # Получаем текущие интерпретаторы из БД
                        current_interpreters = await get_agent_allowed_interpreters(db, agent_id)
                        current_interpreter_names = {i.interpreter_name for i in current_interpreters}
                        
                        # Получаем новые интерпретаторы от агента
                        new_interpreters = metrics.config["allowed_interpreters"]
                        if isinstance(new_interpreters, list):
                            new_interpreter_names = set(new_interpreters)
                        else:
                            new_interpreter_names = set()
                        
                        # Обновляем только если есть различия
                        if current_interpreter_names != new_interpreter_names:
                            await delete_agent_allowed_interpreters(db, agent_id)
                            for interpreter_name in new_interpreter_names:
                                await create_agent_allowed_interpreter(db, agent_id, interpreter_name)
                            print(f"🐍 Обновлены интерпретаторы агента {agent_id}: {sorted(new_interpreter_names)}")
                    
                    # Обрабатываем пользовательские параметры
                    if "user_parameters" in metrics.config:
                        from .database.api import get_user_parameters, delete_user_parameter, create_user_parameter
                        
                        # Получаем текущие параметры из БД
                        current_params = await get_user_parameters(db, agent_id)
                        current_param_keys = {p.parameter_key for p in current_params}
                        
                        # Получаем новые параметры от агента
                        new_params = metrics.config["user_parameters"]
                        if isinstance(new_params, dict):
                            new_param_keys = set(new_params.keys())
                            
                            # Обновляем только если есть различия
                            if current_param_keys != new_param_keys:
                                # Удаляем старые параметры
                                for param in current_params:
                                    await delete_user_parameter(db, param.id)
                                
                                # Добавляем новые параметры
                                for param_key, command in new_params.items():
                                    await create_user_parameter(db, agent_id, {
                                        "parameter_key": param_key,
                                        "command": command
                                    })
                                
                                print(f"⚙️ Обновлены пользовательские параметры агента {agent_id}: {sorted(new_param_keys)}")
            
            # Сохраняем метрики
            for metric_type, metric_data in metrics.dict().items():
                if metric_type in ['cpu', 'memory', 'disk', 'network', 'gpu', 'hdd', 'user', 'inventory'] and metric_data:
                    # Очищаем null-символы из данных
                    cleaned_data = clean_null_characters(metric_data)
                    
                    metric_dict = {
                        "agent_id": agent_id,
                        "machine_type": metrics.machine_type,
                        "machine_name": metrics.machine_name or "Unknown Machine",
                        "metric_type": metric_type,
                        "timestamp": datetime.fromtimestamp(metrics.timestamp),
                        "details": cleaned_data
                    }
                    
                    # Добавляем числовые поля если есть
                    if metric_type == 'cpu' and 'usage_percent' in metric_data:
                        metric_dict['usage_percent'] = metric_data['usage_percent']
                    elif metric_type == 'memory' and 'usage_percent' in metric_data:
                        metric_dict['usage_percent'] = metric_data['usage_percent']
                    elif metric_type == 'disk' and 'usage_percent' in metric_data:
                        metric_dict['usage_percent'] = metric_data['usage_percent']
                    
                    await save_metric(db, metric_dict)
                    print(f"💾 Сохранена метрика {metric_type} для агента {agent_id}")
            
            break  # Выходим из async for
        
        print(f"✅ Все метрики сохранены для агента {agent_id}")
        return {
            "status": "success", 
            "message": "Metrics received and saved",
            "agent_id": agent_id
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