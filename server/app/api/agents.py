from fastapi import APIRouter, Depends, HTTPException, status, Body
from sqlalchemy.ext.asyncio import AsyncSession
from typing import List, Optional, Dict
from datetime import datetime

from ..database.connection import get_db
from ..database.api import (
    get_agent, get_all_agents, update_agent_heartbeat, delete_agent,
    get_agent_metrics, get_metrics_summary, get_interpreters, get_metric_types,
    agent_exists, update_agent_config, create_user_parameter, get_user_parameters,
    update_user_parameter, delete_user_parameter
)
from ..schemas import (
    AgentResponse, AgentHeartbeat, AgentListResponse,
    MetricListResponse, MetricsSummaryResponse,
    InterpreterResponse, MetricTypeResponse,
    AgentCommand, AgentCommandResponse, ScriptExecutionRequest, 
    ConfigUpdateRequest, CollectMetricsRequest, UserParameterCreate, UserParameterResponse
)
# Убрали импорт agent_service - теперь работаем только с БД

router = APIRouter(prefix="/api/v1", tags=["agents"])

# ===== Эндпоинты агентов =====

@router.get("/agents", response_model=AgentListResponse)
async def list_agents(db: AsyncSession = Depends(get_db)):
    """Получение списка всех агентов"""
    agents = await get_all_agents(db)
    return AgentListResponse(agents=agents, total=len(agents))

@router.get("/agents/{agent_id}", response_model=AgentResponse)
async def get_agent_by_id(
    agent_id: str, 
    db: AsyncSession = Depends(get_db)
):
    """Получение информации об агенте"""
    agent = await get_agent(db, agent_id)
    if not agent:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    return agent

@router.put("/agents/{agent_id}/heartbeat", response_model=dict)
async def update_heartbeat(
    agent_id: str,
    db: AsyncSession = Depends(get_db)
):
    """Обновление heartbeat агента"""
    success = await update_agent_heartbeat(db, agent_id)
    if not success:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    return {"message": "Heartbeat обновлен", "agent_id": agent_id, "timestamp": datetime.utcnow()}

@router.delete("/agents/{agent_id}", status_code=status.HTTP_204_NO_CONTENT)
async def remove_agent(
    agent_id: str, 
    db: AsyncSession = Depends(get_db)
):
    """Удаление агента"""
    success = await delete_agent(db, agent_id)
    if not success:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )

# ===== Эндпоинты метрик =====

@router.get("/agents/{agent_id}/metrics", response_model=MetricListResponse)
async def get_agent_metrics_list(
    agent_id: str, 
    metric_type: Optional[str] = None,
    limit: int = 100,
    db: AsyncSession = Depends(get_db)
):
    """Получение метрик агента"""
    # Проверяем существование агента
    if not await agent_exists(db, agent_id):
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    
    metrics = await get_agent_metrics(db, agent_id, metric_type, limit)
    return MetricListResponse(metrics=metrics, total=len(metrics))

@router.get("/agents/{agent_id}/metrics/summary", response_model=MetricsSummaryResponse)
async def get_metrics_summary_endpoint(
    agent_id: str,
    hours: int = 24,
    db: AsyncSession = Depends(get_db)
):
    """Получение сводки метрик агента"""
    # Проверяем существование агента
    if not await agent_exists(db, agent_id):
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    
    summary = await get_metrics_summary(db, agent_id, hours)
    return MetricsSummaryResponse(agent_id=agent_id, summary=summary, hours=hours)

# ===== Эндпоинты справочников =====

@router.get("/interpreters", response_model=List[InterpreterResponse])
async def list_interpreters(db: AsyncSession = Depends(get_db)):
    """Получение списка всех интерпретаторов"""
    interpreters = await get_interpreters(db)
    return interpreters

@router.get("/metric-types", response_model=List[MetricTypeResponse])
async def list_metric_types(db: AsyncSession = Depends(get_db)):
    """Получение списка всех типов метрик"""
    metric_types = await get_metric_types(db)
    return metric_types

# ===== Эндпоинты команд агента =====

@router.post("/agents/{agent_id}/commands/collect-metrics", response_model=AgentCommandResponse)
async def collect_agent_metrics(
    agent_id: str,
    request: CollectMetricsRequest = Body(default=CollectMetricsRequest()),
    db: AsyncSession = Depends(get_db)
):
    """Отправка команды агенту на сбор метрик"""
    # Проверяем существование агента в базе данных
    agent = await get_agent(db, agent_id)
    if not agent:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    
    # Агент найден в базе данных, продолжаем
    
    # Отправляем команду агенту через HTTP
    try:
        # Определяем URL командного сервера агента
        # Используем только command_server_url из базы данных
        if hasattr(agent, 'command_server_url') and agent.command_server_url:
            command_server_url = agent.command_server_url
        else:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Агент не имеет настроенного command_server_url"
            )
        
        # Подготавливаем данные для отправки
        command_data = {}
        if request.metrics:
            command_data["metrics"] = request.metrics
        else:
            # Если метрики не указаны, отправляем пустой объект metrics для сбора всех метрик
            command_data["metrics"] = {}
        
        # Добавляем поле immediate
        command_data["immediate"] = request.immediate
        
        # Отправляем HTTP запрос агенту
        json_payload = {
            "command": "collect_metrics",
            "data": command_data,
            "timestamp": datetime.utcnow().isoformat()
        }
        print(f"📤 JSON payload для collect_metrics: {json_payload}")
        print(f"📤 URL для отправки: {command_server_url}/command")
        
        import aiohttp
        async with aiohttp.ClientSession() as session:
            async with session.post(
                f"{command_server_url}/command",
                json=json_payload,
                timeout=aiohttp.ClientTimeout(total=180)
            ) as response:
                response_text = await response.text()
                print(f"📥 Ответ от агента: HTTP {response.status}")
                print(f"📥 Содержимое ответа: {response_text}")
                
                if response.status == 200:
                    try:
                        # Парсим ответ от агента
                        import json
                        agent_response = json.loads(response_text)
                        
                        # Возвращаем ответ агента как есть
                        return AgentCommandResponse(
                            success=agent_response.get("success", True),
                            message=agent_response.get("message", "Command executed"),
                            data=agent_response.get("data"),
                            timestamp=agent_response.get("timestamp", datetime.utcnow().isoformat())
                        )
                    except json.JSONDecodeError as e:
                        print(f"❌ Ошибка парсинга JSON ответа агента: {e}")
                        return AgentCommandResponse(
                            success=False,
                            message=f"Invalid JSON response from agent: {str(e)}",
                            data=None,
                            timestamp=datetime.utcnow().isoformat()
                        )
                else:
                    return AgentCommandResponse(
                        success=False,
                        message=f"Failed to send command: HTTP {response.status} - {response_text}",
                        data=None,
                        timestamp=datetime.utcnow().isoformat()
                    )
    except Exception as e:
        print(f"❌ Ошибка отправки команды collect-metrics: {repr(e)}")
        print(f"❌ Тип ошибки: {type(e).__name__}")
        print(f"❌ Детали ошибки: {str(e)}")
        import traceback
        print(f"❌ Traceback: {traceback.format_exc()}")
        return AgentCommandResponse(
            success=False,
            message=f"Error sending command: {str(e)}",
            data=None,
            timestamp=datetime.utcnow().isoformat()
        )

@router.post("/agents/{agent_id}/commands/update-config", response_model=AgentCommandResponse)
async def update_agent_config_command(
    agent_id: str,
    config_data: ConfigUpdateRequest,
    db: AsyncSession = Depends(get_db)
):
    """Отправка команды агенту на обновление конфигурации"""
    # Проверяем существование агента в базе данных
    agent = await get_agent(db, agent_id)
    if not agent:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    
    # Агент найден в базе данных, продолжаем
    
    # Обновляем конфигурацию в базе данных
    await update_agent_config(db, agent_id, config_data.dict(exclude_unset=True))
    
    # Обновляем включенные метрики, если они переданы
    if config_data.enabled_metrics is not None:
        from ..database.api import create_agent_enabled_metric, delete_agent_enabled_metrics
        
        # Удаляем старые метрики
        await delete_agent_enabled_metrics(db, agent_id)
        
        # Добавляем новые метрики
        for metric_name, enabled in config_data.enabled_metrics.items():
            if enabled:
                await create_agent_enabled_metric(db, agent_id, metric_name)
    
    # Обновляем разрешенные интерпретаторы, если они переданы
    if hasattr(config_data, 'allowed_interpreters') and config_data.allowed_interpreters is not None:
        from ..database.api import create_agent_allowed_interpreter, delete_agent_allowed_interpreters
        
        # Удаляем старые интерпретаторы
        await delete_agent_allowed_interpreters(db, agent_id)
        
        # Добавляем новые интерпретаторы
        for interpreter_name in config_data.allowed_interpreters:
            await create_agent_allowed_interpreter(db, agent_id, interpreter_name)
    
    # Отправляем команду агенту через HTTP
    try:
        # Получаем информацию об агенте из БД для определения URL
        agent = await get_agent(db, agent_id)
        if not agent:
            raise HTTPException(status_code=404, detail="Агент не найден")
        
        # Определяем URL командного сервера агента
        # Используем только command_server_url из базы данных
        if hasattr(agent, 'command_server_url') and agent.command_server_url:
            command_server_url = agent.command_server_url
        else:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Агент не имеет настроенного command_server_url"
            )
        
        # Подготавливаем данные для отправки
        config_dict = config_data.dict(exclude_unset=True)
        
        # Отправляем HTTP запрос агенту
        import aiohttp
        async with aiohttp.ClientSession() as session:
            async with session.post(
                f"{command_server_url}/command",
                json={
                    "command": "update_config",
                    "data": config_dict,
                    "timestamp": datetime.utcnow().isoformat()
                },
                timeout=aiohttp.ClientTimeout(total=180)
            ) as response:
                if response.status == 200:
                    return AgentCommandResponse(
                        success=True,
                        message="Configuration update command sent to agent",
                        data=config_dict,
                        timestamp=datetime.utcnow().isoformat()
                    )
                else:
                    return AgentCommandResponse(
                        success=False,
                        message=f"Failed to send command: HTTP {response.status}",
                        data=None,
                        timestamp=datetime.utcnow().isoformat()
                    )
    except Exception as e:
        return AgentCommandResponse(
            success=False,
            message=f"Error sending command: {str(e)}",
            data=None,
            timestamp=datetime.utcnow().isoformat()
        )

@router.post("/agents/{agent_id}/commands/restart", response_model=AgentCommandResponse)
async def restart_agent_command(
    agent_id: str,
    db: AsyncSession = Depends(get_db)
):
    """Отправка команды агенту на перезапуск"""
    # Проверяем существование агента в базе данных
    agent = await get_agent(db, agent_id)
    if not agent:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    
    # Агент найден в базе данных, продолжаем
    
    # Отправляем команду агенту через HTTP
    try:
        # Определяем URL командного сервера агента
        # Используем только command_server_url из базы данных
        if hasattr(agent, 'command_server_url') and agent.command_server_url:
            command_server_url = agent.command_server_url
        else:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Агент не имеет настроенного command_server_url"
            )
        
        # Отправляем HTTP запрос агенту
        import aiohttp
        async with aiohttp.ClientSession() as session:
            async with session.post(
                f"{command_server_url}/command",
                json={
                    "command": "restart",
                    "timestamp": datetime.utcnow().isoformat()
                },
                timeout=aiohttp.ClientTimeout(total=180)
            ) as response:
                if response.status == 200:
                    return AgentCommandResponse(
                        success=True,
                        message="Restart command sent to agent",
                        data=None,
                        timestamp=datetime.utcnow().isoformat()
                    )
                else:
                    return AgentCommandResponse(
                        success=False,
                        message=f"Failed to send command: HTTP {response.status}",
                        data=None,
                        timestamp=datetime.utcnow().isoformat()
                    )
    except Exception as e:
        return AgentCommandResponse(
            success=False,
            message=f"Error sending command: {str(e)}",
            data=None,
            timestamp=datetime.utcnow().isoformat()
        )

@router.post("/agents/{agent_id}/commands/stop", response_model=AgentCommandResponse)
async def stop_agent_command(
    agent_id: str,
    db: AsyncSession = Depends(get_db)
):
    """Отправка команды агенту на остановку"""
    # Проверяем существование агента в базе данных
    agent = await get_agent(db, agent_id)
    if not agent:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    
    # Агент найден в базе данных, продолжаем
    
    # Отправляем команду агенту через HTTP
    try:
        # Определяем URL командного сервера агента
        # Используем только command_server_url из базы данных
        if hasattr(agent, 'command_server_url') and agent.command_server_url:
            command_server_url = agent.command_server_url
        else:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Агент не имеет настроенного command_server_url"
            )
        
        # Отправляем HTTP запрос агенту
        import aiohttp
        async with aiohttp.ClientSession() as session:
            async with session.post(
                f"{command_server_url}/command",
                json={
                    "command": "stop",
                    "timestamp": datetime.utcnow().isoformat()
                },
                timeout=aiohttp.ClientTimeout(total=180)
            ) as response:
                if response.status == 200:
                    return AgentCommandResponse(
                        success=True,
                        message="Stop command sent to agent",
                        data=None,
                        timestamp=datetime.utcnow().isoformat()
                    )
                else:
                    return AgentCommandResponse(
                        success=False,
                        message=f"Failed to send command: HTTP {response.status}",
                        data=None,
                        timestamp=datetime.utcnow().isoformat()
                    )
    except Exception as e:
        return AgentCommandResponse(
            success=False,
            message=f"Error sending command: {str(e)}",
            data=None,
            timestamp=datetime.utcnow().isoformat()
        )

@router.post("/agents/{agent_id}/commands/run-script", response_model=AgentCommandResponse)
async def run_script_command(
    agent_id: str,
    script_request: ScriptExecutionRequest,
    db: AsyncSession = Depends(get_db)
):
    """Отправка команды агенту на выполнение скрипта"""
    # Проверяем существование агента в базе данных
    agent = await get_agent(db, agent_id)
    if not agent:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    
    # Агент найден в базе данных, продолжаем
    
    # Отправляем команду агенту через HTTP
    try:
        # Определяем URL командного сервера агента
        if hasattr(agent, 'command_server_url') and agent.command_server_url:
            command_server_url = agent.command_server_url
        else:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Агент не имеет настроенного command_server_url"
            )
        
        # Подготавливаем данные для отправки
        script_data = script_request.dict(exclude_unset=True)
        
        # Отправляем HTTP запрос агенту
        json_payload = {
            "command": "run_script",
            "data": script_data,
            "timestamp": datetime.utcnow().isoformat()
        }
        print(f"📤 JSON payload для run_script: {json_payload}")
        print(f"📤 URL для отправки: {command_server_url}/command")
        
        import aiohttp
        async with aiohttp.ClientSession() as session:
            async with session.post(
                f"{command_server_url}/command",
                json=json_payload,
                timeout=aiohttp.ClientTimeout(total=180)
            ) as response:
                response_text = await response.text()
                print(f"📥 Ответ от агента: HTTP {response.status}")
                print(f"📥 Содержимое ответа: {response_text}")
                
                if response.status == 200:
                    try:
                        # Парсим ответ от агента
                        import json
                        agent_response = json.loads(response_text)
                        
                        # Возвращаем ответ агента как есть
                        return AgentCommandResponse(
                            success=agent_response.get("success", True),
                            message=agent_response.get("message", "Script executed"),
                            data=agent_response.get("data"),
                            timestamp=agent_response.get("timestamp", datetime.utcnow().isoformat())
                        )
                    except json.JSONDecodeError as e:
                        print(f"❌ Ошибка парсинга JSON ответа агента: {e}")
                        return AgentCommandResponse(
                            success=False,
                            message=f"Invalid JSON response from agent: {str(e)}",
                            data=None,
                            timestamp=datetime.utcnow().isoformat()
                        )
                else:
                    return AgentCommandResponse(
                        success=False,
                        message=f"Failed to send command: HTTP {response.status} - {response_text}",
                        data=None,
                        timestamp=datetime.utcnow().isoformat()
                    )
    except Exception as e:
        print(f"❌ Ошибка отправки команды run-script: {repr(e)}")
        print(f"❌ Тип ошибки: {type(e).__name__}")
        print(f"❌ Детали ошибки: {str(e)}")
        import traceback
        print(f"❌ Traceback: {traceback.format_exc()}")
        return AgentCommandResponse(
            success=False,
            message=f"Error sending command: {str(e)}",
            data=None,
            timestamp=datetime.utcnow().isoformat()
        )

@router.post("/agents/{agent_id}/commands/run-script", response_model=AgentCommandResponse)
async def run_script_command(
    agent_id: str,
    script_request: ScriptExecutionRequest,
    db: AsyncSession = Depends(get_db)
):
    """Выполнение скрипта на агенте с валидацией payload"""
    # Проверяем существование агента в базе данных
    agent = await get_agent(db, agent_id)
    if not agent:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    
    # Базовая проверка наличия одного из полей
    if not (script_request.script or script_request.script_path):
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="One of script or script_path is required"
        )
    
    # Отправляем команду агенту через HTTP
    try:
        # Определяем URL командного сервера агента
        if hasattr(agent, 'command_server_url') and agent.command_server_url:
            command_server_url = agent.command_server_url
        else:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Агент не имеет настроенного command_server_url"
            )
        
        # Подготавливаем данные для отправки
        script_data = script_request.dict(exclude_unset=True)
        
        # Отправляем HTTP запрос агенту
        json_payload = {
            "command": "run_script",
            "data": script_data,
            "timestamp": datetime.utcnow().isoformat()
        }
        print(f"📤 JSON payload для run_script: {json_payload}")
        print(f"📤 URL для отправки: {command_server_url}/command")
        
        import aiohttp
        async with aiohttp.ClientSession() as session:
            async with session.post(
                f"{command_server_url}/command",
                json=json_payload,
                timeout=aiohttp.ClientTimeout(total=180)
            ) as response:
                response_text = await response.text()
                print(f"📥 Ответ от агента: HTTP {response.status}")
                print(f"📥 Содержимое ответа: {response_text}")
                
                if response.status == 200:
                    try:
                        # Парсим ответ от агента
                        import json
                        agent_response = json.loads(response_text)
                        
                        # Возвращаем ответ агента как есть
                        return AgentCommandResponse(
                            success=agent_response.get("success", True),
                            message=agent_response.get("message", "Script executed"),
                            data=agent_response.get("data"),
                            timestamp=agent_response.get("timestamp", datetime.utcnow().isoformat())
                        )
                    except json.JSONDecodeError as e:
                        print(f"❌ Ошибка парсинга JSON ответа агента: {e}")
                        return AgentCommandResponse(
                            success=False,
                            message=f"Invalid JSON response from agent: {str(e)}",
                            data=None,
                            timestamp=datetime.utcnow().isoformat()
                        )
                else:
                    return AgentCommandResponse(
                        success=False,
                        message=f"Failed to send command: HTTP {response.status} - {response_text}",
                        data=None,
                        timestamp=datetime.utcnow().isoformat()
                    )
    except Exception as e:
        print(f"❌ Ошибка отправки команды run-script: {repr(e)}")
        print(f"❌ Тип ошибки: {type(e).__name__}")
        print(f"❌ Детали ошибки: {str(e)}")
        import traceback
        print(f"❌ Traceback: {traceback.format_exc()}")
        return AgentCommandResponse(
            success=False,
            message=f"Error sending command: {str(e)}",
            data=None,
            timestamp=datetime.utcnow().isoformat()
        )

# ===== Эндпоинты пользовательских параметров =====

@router.post("/agents/{agent_id}/parameters", response_model=UserParameterResponse)
async def create_user_parameter_endpoint(
    agent_id: str,
    parameter_data: UserParameterCreate,
    db: AsyncSession = Depends(get_db)
):
    """Создание пользовательского параметра для агента"""
    # Проверяем существование агента
    if not await agent_exists(db, agent_id):
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    
    # Создаем параметр
    parameter = await create_user_parameter(db, agent_id, parameter_data.dict())
    return parameter

@router.get("/agents/{agent_id}/parameters", response_model=List[UserParameterResponse])
async def get_user_parameters_endpoint(
    agent_id: str,
    db: AsyncSession = Depends(get_db)
):
    """Получение пользовательских параметров агента"""
    # Проверяем существование агента
    if not await agent_exists(db, agent_id):
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    
    # Получаем параметры
    parameters = await get_user_parameters(db, agent_id)
    return parameters

@router.put("/agents/{agent_id}/parameters/{param_id}", response_model=UserParameterResponse)
async def update_user_parameter_endpoint(
    agent_id: str,
    param_id: int,
    parameter_data: UserParameterCreate,
    db: AsyncSession = Depends(get_db)
):
    """Обновление пользовательского параметра агента"""
    # Проверяем существование агента
    if not await agent_exists(db, agent_id):
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    
    # Обновляем параметр
    parameter = await update_user_parameter(db, param_id, parameter_data.dict())
    if not parameter:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Параметр с ID {param_id} не найден"
        )
    return parameter

@router.delete("/agents/{agent_id}/parameters/{param_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_user_parameter_endpoint(
    agent_id: str,
    param_id: int,
    db: AsyncSession = Depends(get_db)
):
    """Удаление пользовательского параметра агента"""
    # Проверяем существование агента
    if not await agent_exists(db, agent_id):
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    
    # Удаляем параметр
    success = await delete_user_parameter(db, param_id)
    if not success:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Параметр с ID {param_id} не найден"
        ) 