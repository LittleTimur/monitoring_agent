from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.ext.asyncio import AsyncSession
from typing import List, Optional
from datetime import datetime

from ..database.connection import get_db
from ..database.api import (
    create_agent, get_agent, get_all_agents, update_agent_heartbeat, delete_agent,
    save_metric, save_network_connections, get_agent_metrics, get_metrics_summary,
    create_user_parameter, get_user_parameters, get_interpreters, get_metric_types,
    agent_exists
)
from ..schemas import (
    AgentCreate, AgentResponse, AgentHeartbeat, AgentListResponse,
    MetricCreate, MetricResponse, MetricListResponse, MetricsSummaryResponse,
    UserParameterCreate, UserParameterResponse, InterpreterResponse, MetricTypeResponse
)

router = APIRouter(prefix="/api/v1", tags=["agents"])

# ===== Эндпоинты агентов =====

@router.post("/agents", response_model=AgentResponse, status_code=status.HTTP_201_CREATED)
async def register_agent(
    agent_data: AgentCreate,
    db: AsyncSession = Depends(get_db)
):
    """Регистрация нового агента"""
    # Проверяем, не существует ли уже агент с таким ID
    if await agent_exists(db, agent_data.agent_id):
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=f"Агент с ID {agent_data.agent_id} уже существует"
        )
    
    agent = await create_agent(db, agent_data.dict())
    return agent

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

@router.post("/agents/{agent_id}/metrics", response_model=MetricResponse, status_code=status.HTTP_201_CREATED)
async def save_agent_metric(
    agent_id: str,
    metric_data: MetricCreate,
    db: AsyncSession = Depends(get_db)
):
    """Сохранение метрики агента"""
    # Проверяем существование агента
    if not await agent_exists(db, agent_id):
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    
    # Подготавливаем данные метрики
    metric_dict = metric_data.dict()
    metric_dict['agent_id'] = agent_id
    
    # Убираем network_connections из данных метрики, так как они сохраняются отдельно
    network_connections = metric_dict.pop('network_connections', None)
    
    # Сохраняем метрику
    metric = await save_metric(db, metric_dict)
    
    # Если есть сетевые соединения, сохраняем их отдельно
    if network_connections:
        await save_network_connections(db, metric.id, network_connections)
    
    return metric

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

# ===== Эндпоинты пользовательских параметров =====

@router.post("/agents/{agent_id}/parameters", response_model=UserParameterResponse, status_code=status.HTTP_201_CREATED)
async def create_agent_parameter(
    agent_id: str,
    parameter_data: UserParameterCreate,
    db: AsyncSession = Depends(get_db)
):
    """Создание пользовательского параметра агента"""
    # Проверяем существование агента
    if not await agent_exists(db, agent_id):
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Агент с ID {agent_id} не найден"
        )
    
    param = await create_user_parameter(db, agent_id, parameter_data.parameter_key, parameter_data.command)
    return param

@router.get("/agents/{agent_id}/parameters", response_model=List[UserParameterResponse])
async def get_agent_parameters(
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
    
    parameters = await get_user_parameters(db, agent_id)
    return parameters

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