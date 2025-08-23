from fastapi import APIRouter, HTTPException, BackgroundTasks
from typing import List, Optional, Dict, Any
from datetime import datetime
import asyncio

from ..models.agent import (
    AgentInfo, AgentConfig, AgentStatus, AgentCommand, 
    AgentResponse, MetricsRequest, MetricType, AgentCommandRequest, AgentRegistration,
    RunScriptRequest
)
from ..services.agent_service import agent_service, CommandStatus, CommandExecution

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



@router.post("/{agent_id}/command")
async def send_command_to_agent(
    agent_id: str, 
    command: Dict[str, Any],
    background_tasks: BackgroundTasks
):
    """Универсальный эндпоинт для отправки команд агенту
    
    Поддерживаемые команды:
    - update_config: изменение собираемых метрик и времени сбора
    - collect_metrics: немедленный сбор выбранных метрик
    - restart: перезапуск агента
    - stop: остановка агента
    """
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")
    
    command_name = command.get("command", "")
    command_data = command.get("data", {})
    
    # Создаем объект команды
    agent_command = AgentCommand(
        command=command_name,
        data=command_data,
        timestamp=datetime.now()
    )
    
    print(f"🚀 Отправка команды '{command_name}' агенту {agent_id}")
    print(f"   Данные: {command_data}")
    
    # Добавляем команду в очередь
    agent_service.command_queue[agent_id].append(agent_command)
    
    # Отправляем команду агенту
    background_tasks.add_task(agent_service.send_command_to_agent, agent_id, agent_command)
    
    return {"status": "success", "message": f"Command '{command_name}' sent to agent {agent_id}"}


# --- Script execution convenience endpoints ---

@router.post("/{agent_id}/run_script")
async def run_script(agent_id: str, req: RunScriptRequest, background_tasks: BackgroundTasks):
    """Запуск скрипта на агенте с валидацией payload."""
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")

    # Базовая проверка наличия одного из полей
    if not (req.script or req.script_path or req.key):
        raise HTTPException(status_code=400, detail="One of script, script_path or key is required")

    cmd = AgentCommand(
        command="run_script",
        data=req.dict(),
        timestamp=datetime.now()
    )
    agent_service.command_queue[agent_id].append(cmd)
    background_tasks.add_task(agent_service.send_command_to_agent, agent_id, cmd)
    return {"status": "queued", "message": "run_script queued"}


@router.get("/{agent_id}/jobs/{job_id}")
async def get_job_output(agent_id: str, job_id: str):
    """Получение статуса/вывода фоновой задачи на агенте."""
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")

    cmd = AgentCommand(
        command="get_job_output",
        data={"job_id": job_id},
        timestamp=datetime.now()
    )
    resp = await agent_service.send_command_to_agent(agent_id, cmd)
    return resp.dict()


@router.delete("/{agent_id}/jobs/{job_id}")
async def kill_job(agent_id: str, job_id: str):
    """Остановка фоновой задачи на агенте."""
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")

    cmd = AgentCommand(
        command="kill_job",
        data={"job_id": job_id},
        timestamp=datetime.now()
    )
    resp = await agent_service.send_command_to_agent(agent_id, cmd)
    return resp.dict()


@router.get("/{agent_id}/jobs")
async def list_jobs(agent_id: str):
    """Список фоновых задач на агенте."""
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")
    cmd = AgentCommand(
        command="list_jobs",
        data={},
        timestamp=datetime.now()
    )
    resp = await agent_service.send_command_to_agent(agent_id, cmd)
    return resp.dict()


@router.post("/{agent_id}/scripts")
async def push_script(agent_id: str, body: Dict[str, Any]):
    """Загрузка/обновление скрипта на агенте. body: {name, content, chmod?}"""
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")
    name = body.get("name")
    content = body.get("content")
    if not name or not content:
        raise HTTPException(status_code=400, detail="name and content required")
    cmd = AgentCommand(
        command="push_script",
        data={"name": name, "content": content, **({"chmod": body.get("chmod")} if body.get("chmod") else {})},
        timestamp=datetime.now()
    )
    resp = await agent_service.send_command_to_agent(agent_id, cmd)
    return resp.dict()


@router.get("/{agent_id}/scripts")
async def list_scripts(agent_id: str):
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")
    cmd = AgentCommand(command="list_scripts", data={}, timestamp=datetime.now())
    resp = await agent_service.send_command_to_agent(agent_id, cmd)
    return resp.dict()


@router.delete("/{agent_id}/scripts/{name}")
async def delete_script(agent_id: str, name: str):
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")
    cmd = AgentCommand(command="delete_script", data={"name": name}, timestamp=datetime.now())
    resp = await agent_service.send_command_to_agent(agent_id, cmd)
    return resp.dict()

@router.post("/command_all")
async def send_command_to_all_agents(
    command: Dict[str, Any],
    background_tasks: BackgroundTasks
):
    """Отправка команды всем агентам
    
    Поддерживаемые команды:
    - update_config: изменение собираемых метрик и времени сбора
    - collect_metrics: немедленный сбор выбранных метрик
    - restart: перезапуск агента
    - stop: остановка агента
    """
    command_name = command.get("command", "")
    command_data = command.get("data", {})
    
    # Создаем объект команды
    agent_command = AgentCommand(
        command=command_name,
        data=command_data,
        timestamp=datetime.now()
    )
    
    print(f"🚀 Отправка команды '{command_name}' всем агентам")
    print(f"   Данные: {command_data}")
    
    # Команда для всех агентов
    results = {}
    
    for agent_id in agent_service.agents.keys():
        agent_service.command_queue[agent_id].append(agent_command)
        background_tasks.add_task(agent_service.send_command_to_agent, agent_id, agent_command)
        results[agent_id] = True
    
    return {
        "status": "success", 
        "message": f"Command '{command_name}' sent to {len(results)} agents",
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

# Новые эндпоинты для мониторинга выполнения команд

@router.get("/{agent_id}/command-executions")
async def get_command_executions(agent_id: str):
    """Получение истории выполнения команд для агента"""
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")
    
    executions = agent_service.get_command_executions(agent_id)
    
    # Конвертируем в JSON-совместимый формат
    execution_data = []
    for i, execution in enumerate(executions):
        execution_data.append({
            "index": i,
            "command": execution.command.command,
            "data": execution.command.data,
            "status": execution.status,
            "start_time": execution.start_time.isoformat() if execution.start_time else None,
            "end_time": execution.end_time.isoformat() if execution.end_time else None,
            "retry_count": execution.retry_count,
            "error_message": execution.error_message,
            "response": execution.response.dict() if execution.response else None
        })
    
    return {
        "agent_id": agent_id,
        "total_executions": len(executions),
        "executions": execution_data
    }

@router.get("/{agent_id}/command-executions/{execution_index}")
async def get_command_execution_status(agent_id: str, execution_index: int):
    """Получение статуса выполнения конкретной команды"""
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")
    
    execution = agent_service.get_command_status(agent_id, execution_index)
    if not execution:
        raise HTTPException(status_code=404, detail="Command execution not found")
    
    return {
        "agent_id": agent_id,
        "execution_index": execution_index,
        "command": execution.command.command,
        "data": execution.command.data,
        "status": execution.status,
        "start_time": execution.start_time.isoformat() if execution.start_time else None,
        "end_time": execution.end_time.isoformat() if execution.end_time else None,
        "retry_count": execution.retry_count,
        "error_message": execution.error_message,
        "response": execution.response.dict() if execution.response else None
    }

@router.get("/command-executions/status")
async def get_all_command_executions_status():
    """Получение общего статуса выполнения команд по всем агентам"""
    all_executions = {}
    total_executions = 0
    total_completed = 0
    total_failed = 0
    total_pending = 0
    total_in_progress = 0
    
    for agent_id in agent_service.agents.keys():
        executions = agent_service.get_command_executions(agent_id)
        all_executions[agent_id] = {
            "total": len(executions),
            "completed": sum(1 for ex in executions if ex.status == CommandStatus.COMPLETED),
            "failed": sum(1 for ex in executions if ex.status in [CommandStatus.FAILED, CommandStatus.TIMEOUT]),
            "pending": sum(1 for ex in executions if ex.status == CommandStatus.PENDING),
            "in_progress": sum(1 for ex in executions if ex.status == CommandStatus.IN_PROGRESS)
        }
        
        total_executions += all_executions[agent_id]["total"]
        total_completed += all_executions[agent_id]["completed"]
        total_failed += all_executions[agent_id]["failed"]
        total_pending += all_executions[agent_id]["pending"]
        total_in_progress += all_executions[agent_id]["in_progress"]
    
    return {
        "summary": {
            "total_agents": len(agent_service.agents),
            "total_executions": total_executions,
            "total_completed": total_completed,
            "total_failed": total_failed,
            "total_pending": total_pending,
            "total_in_progress": total_in_progress,
            "success_rate": (total_completed / total_executions * 100) if total_executions > 0 else 0
        },
        "agents": all_executions
    }

@router.post("/{agent_id}/command-executions/{execution_index}/retry")
async def retry_command_execution(agent_id: str, execution_index: int, background_tasks: BackgroundTasks):
    """Повторная попытка выполнения команды"""
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")
    
    execution = agent_service.get_command_status(agent_id, execution_index)
    if not execution:
        raise HTTPException(status_code=404, detail="Command execution not found")
    
    if execution.status not in [CommandStatus.FAILED, CommandStatus.TIMEOUT]:
        raise HTTPException(status_code=400, detail="Can only retry failed or timed out commands")
    
    # Сбрасываем счетчик попыток и статус
    execution.retry_count = 0
    execution.status = CommandStatus.PENDING
    execution.start_time = datetime.now()
    execution.end_time = None
    execution.error_message = None
    execution.response = None
    
    # Повторно отправляем команду
    background_tasks.add_task(agent_service.send_command_to_agent, agent_id, execution.command)
    
    return {
        "status": "success", 
        "message": f"Command retry initiated for execution #{execution_index}",
        "execution_index": execution_index,
        "new_status": CommandStatus.PENDING
    }

@router.delete("/{agent_id}/command-executions/{execution_index}")
async def delete_command_execution(agent_id: str, execution_index: int):
    """Удаление записи о выполнении команды"""
    agent = agent_service.get_agent(agent_id)
    if not agent:
        raise HTTPException(status_code=404, detail="Agent not found")
    
    executions = agent_service.get_command_executions(agent_id)
    if execution_index >= len(executions):
        raise HTTPException(status_code=404, detail="Command execution not found")
    
    # Удаляем запись
    executions.pop(execution_index)
    
    return {
        "status": "success", 
        "message": f"Command execution #{execution_index} deleted",
        "remaining_executions": len(executions)
    }

@router.post("/register")
async def register_new_agent(registration_data: AgentRegistration):
    """Автоматическая регистрация нового агента (ID генерируется автоматически)"""
    import time
    import random
    
    # Генерируем уникальный ID агента
    timestamp = int(time.time() * 1000)
    random_suffix = random.randint(1000, 9999)
    agent_id = f"agent_{timestamp}_{random_suffix}"
    
    print(f"🔧 Auto-registration attempt for new agent")
    print(f"   Generated ID: {agent_id}")
    print(f"   Machine name: {registration_data.machine_name}")
    print(f"   Machine type: {registration_data.machine_type}")
    print(f"   IP address: {registration_data.ip_address}")
    
    agent_info = agent_service.register_agent(
        agent_id, 
        registration_data
    )
    
    print(f"✅ New agent {agent_id} auto-registered successfully")
    return {
        "agent_id": agent_id,
        "status": "success",
        "message": "Agent registered successfully",
        "agent_info": agent_info
    }

@router.post("/{agent_id}/register")
async def register_agent(
    agent_id: str,
    registration_data: AgentRegistration
):
    """Регистрация нового агента с указанным ID"""
    print(f"🔧 Registration attempt for agent {agent_id}")
    print(f"   Machine name: {registration_data.machine_name}")
    print(f"   Machine type: {registration_data.machine_type}")
    print(f"   IP address: {registration_data.ip_address}")
    
    agent_info = agent_service.register_agent(
        agent_id, 
        registration_data
    )
    
    print(f"✅ Agent {agent_id} registered successfully")
    return agent_info 