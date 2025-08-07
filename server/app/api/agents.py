from fastapi import APIRouter, HTTPException, BackgroundTasks
from typing import List, Optional, Dict, Any
from datetime import datetime
import asyncio

from ..models.agent import (
    AgentInfo, AgentConfig, AgentStatus, AgentCommand, 
    AgentResponse, MetricsRequest, MetricType, AgentCommandRequest
)
from ..services.agent_service import agent_service

router = APIRouter(prefix="/api/agents", tags=["agents"])

@router.get("/", response_model=List[AgentInfo])
async def get_agents():
    """Получение списка всех агентов"""
    return agent_service.get_all_agents()

@router.get("/statistics")
async def get_agent_statistics():
    """Получение статистики по агентам"""
    return agent_service.get_agent_statistics()

@router.get("/{agent_id}", response_model=AgentInfo)
async def get_agent(agent_id: str):
    """Получение информации о конкретном агенте"""
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")
    return agent

@router.get("/{agent_id}/config", response_model=AgentConfig)
async def get_agent_config(agent_id: str):
    """Получение конфигурации агента"""
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")
    return agent.config

@router.post("/{agent_id}/config")
async def update_agent_config(agent_id: str, config: Dict[str, Any]):
    """Обновление конфигурации агента (принимает словарь с флагами)"""
    # Преобразуем enabled_metrics к нужному виду
    if "enabled_metrics" in config and isinstance(config["enabled_metrics"], dict):
        config_obj = AgentConfig(**config)
    else:
        # Для обратной совместимости
        config_obj = AgentConfig(**config)
    success = agent_service.update_agent_config(agent_id, config_obj)
    if not success:
        raise HTTPException(status_code=404, detail="Agent not found")
    return {"status": "success", "message": "Configuration updated"}

@router.post("/{agent_id}/request_metrics")
async def request_metrics_from_agent(
    agent_id: str,
    request: Dict[str, Any],
    background_tasks: BackgroundTasks
):
    """Запрос метрик от конкретного агента (принимает словарь с флагами)"""
    metrics = request.get("metrics", {})
    immediate = request.get("immediate", True)
    success = agent_service.request_metrics(
        agent_id,
        metrics,
        immediate
    )
    if not success:
        raise HTTPException(status_code=404, detail="Agent not found")
    # Если немедленный запрос, отправляем команду агенту
    if immediate:
        command = AgentCommand(
            command="collect_metrics",
            data={"metrics": metrics, "immediate": True}
        )
        background_tasks.add_task(agent_service.send_command_to_agent, agent_id, command)
    return {"status": "success", "message": "Metrics request sent"}

@router.post("/request_metrics_from_all")
async def request_metrics_from_all_agents(
    request: MetricsRequest,
    background_tasks: BackgroundTasks
):
    """Запрос метрик от всех агентов"""
    results = agent_service.request_metrics_from_all(
        request.metrics, 
        request.immediate
    )
    
    # Если немедленный запрос, отправляем команды всем агентам
    if request.immediate:
        command = AgentCommand(
            command="collect_metrics",
            data={"metrics": [m.value for m in request.metrics], "immediate": True}
        )
        for agent_id in results.keys():
            background_tasks.add_task(agent_service.send_command_to_agent, agent_id, command)
    
    return {
        "status": "success", 
        "message": f"Metrics request sent to {len(results)} agents",
        "results": results
    }

@router.post("/{agent_id}/restart")
async def restart_agent(agent_id: str, background_tasks: BackgroundTasks):
    """Перезапуск агента"""
    success = agent_service.restart_agent(agent_id)
    if not success:
        raise HTTPException(status_code=404, detail="Agent not found")
    
    # Отправляем команду агенту
    command = AgentCommand(command="restart")
    background_tasks.add_task(agent_service.send_command_to_agent, agent_id, command)
    
    return {"status": "success", "message": "Restart command sent"}

@router.post("/{agent_id}/stop")
async def stop_agent(agent_id: str, background_tasks: BackgroundTasks):
    """Остановка агента"""
    success = agent_service.stop_agent(agent_id)
    if not success:
        raise HTTPException(status_code=404, detail="Agent not found")
    
    # Отправляем команду агенту
    command = AgentCommand(command="stop")
    background_tasks.add_task(agent_service.send_command_to_agent, agent_id, command)
    
    return {"status": "success", "message": "Stop command sent"}

@router.post("/{agent_id}/command")
async def send_command_to_agent(
    agent_id: str, 
    command: AgentCommand,
    background_tasks: BackgroundTasks
):
    """Отправка произвольной команды агенту"""
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")
    
    # Добавляем команду в очередь
    agent_service.command_queue[agent_id].append(command)
    
    # Отправляем команду агенту
    background_tasks.add_task(agent_service.send_command_to_agent, agent_id, command)
    
    return {"status": "success", "message": "Command sent"}

@router.post("/command_all")
async def send_command_to_all_agents(
    request: AgentCommandRequest,
    background_tasks: BackgroundTasks
):
    """Отправка команды всем агентам"""
    if request.agent_id:
        # Команда для конкретного агента
        success = agent_service.get_agent(request.agent_id) is not None
        if not success:
            raise HTTPException(status_code=404, detail="Agent not found")
        
        command = AgentCommand(command=request.command, data=request.data)
        agent_service.command_queue[request.agent_id].append(command)
        background_tasks.add_task(agent_service.send_command_to_agent, request.agent_id, command)
        
        return {"status": "success", "message": "Command sent to agent"}
    else:
        # Команда для всех агентов
        results = {}
        command = AgentCommand(command=request.command, data=request.data)
        
        for agent_id in agent_service.agents.keys():
            agent_service.command_queue[agent_id].append(command)
            background_tasks.add_task(agent_service.send_command_to_agent, agent_id, command)
            results[agent_id] = True
        
        return {
            "status": "success", 
            "message": f"Command sent to {len(results)} agents",
            "results": results
        }

@router.get("/{agent_id}/commands")
async def get_pending_commands(agent_id: str):
    """Получение ожидающих команд для агента"""
    commands = agent_service.get_pending_commands(agent_id)
    return {"commands": commands}

@router.delete("/{agent_id}/commands/{command_index}")
async def remove_command(agent_id: str, command_index: int):
    """Удаление команды из очереди"""
    agent_service.remove_command(agent_id, command_index)
    return {"status": "success", "message": "Command removed"}

@router.post("/{agent_id}/register")
async def register_agent(
    agent_id: str,
    machine_name: str,
    machine_type: str,
    ip_address: Optional[str] = None
):
    """Регистрация нового агента"""
    agent_info = agent_service.register_agent(
        agent_id, machine_name, machine_type, ip_address
    )
    return agent_info 