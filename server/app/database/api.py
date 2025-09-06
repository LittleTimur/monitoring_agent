from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select, update, delete
from sqlalchemy.orm import selectinload
from typing import List, Optional, Dict, Any
from datetime import datetime
from .models import (
    Agent, Interpreter, MetricType, AgentAllowedInterpreter, 
    AgentEnabledMetric, AgentUserParameter, AgentMetric, MetricNetworkConnection
)

# ===== CRUD для агентов =====

async def create_agent(db: AsyncSession, agent_data: Dict[str, Any]) -> Agent:
    """Создание нового агента"""
    agent = Agent(**agent_data)
    db.add(agent)
    await db.commit()
    await db.refresh(agent)
    return agent

async def get_agent(db: AsyncSession, agent_id: str) -> Optional[Agent]:
    """Получение агента по ID"""
    result = await db.execute(
        select(Agent)
        .options(
            selectinload(Agent.user_parameters),
            selectinload(Agent.allowed_interpreters).selectinload(AgentAllowedInterpreter.interpreter),
            selectinload(Agent.enabled_metrics).selectinload(AgentEnabledMetric.metric_type)
        )
        .where(Agent.agent_id == agent_id)
    )
    return result.scalar_one_or_none()

async def get_all_agents(db: AsyncSession) -> List[Agent]:
    """Получение всех агентов"""
    result = await db.execute(
        select(Agent)
        .options(
            selectinload(Agent.user_parameters),
            selectinload(Agent.allowed_interpreters).selectinload(AgentAllowedInterpreter.interpreter),
            selectinload(Agent.enabled_metrics).selectinload(AgentEnabledMetric.metric_type)
        )
    )
    return result.scalars().all()

async def update_agent_heartbeat(db: AsyncSession, agent_id: str) -> bool:
    """Обновление heartbeat агента"""
    result = await db.execute(
        update(Agent)
        .where(Agent.agent_id == agent_id)
        .values(last_heartbeat=datetime.utcnow())
    )
    await db.commit()
    return result.rowcount > 0

async def delete_agent(db: AsyncSession, agent_id: str) -> bool:
    """Удаление агента"""
    result = await db.execute(
        delete(Agent).where(Agent.agent_id == agent_id)
    )
    await db.commit()
    return result.rowcount > 0

# ===== CRUD для метрик =====

async def save_metric(db: AsyncSession, metric_data: Dict[str, Any]) -> AgentMetric:
    """Сохранение метрики агента"""
    metric = AgentMetric(**metric_data)
    db.add(metric)
    await db.commit()
    await db.refresh(metric)
    
    # Загружаем связанные данные для корректной сериализации
    await db.refresh(metric, ['network_connections'])
    
    return metric

async def save_network_connections(
    db: AsyncSession, 
    metric_id: int, 
    connections: List[Dict[str, Any]]
) -> List[MetricNetworkConnection]:
    """Сохранение сетевых соединений"""
    network_connections = []
    for conn_data in connections:
        conn_data['metric_id'] = metric_id
        conn = MetricNetworkConnection(**conn_data)
        network_connections.append(conn)
    
    db.add_all(network_connections)
    await db.commit()
    
    for conn in network_connections:
        await db.refresh(conn)
    
    return network_connections

async def get_agent_metrics(
    db: AsyncSession, 
    agent_id: str, 
    metric_type: Optional[str] = None,
    limit: int = 100
) -> List[AgentMetric]:
    """Получение метрик агента"""
    query = select(AgentMetric).options(
        selectinload(AgentMetric.network_connections)
    ).where(AgentMetric.agent_id == agent_id)
    
    if metric_type:
        query = query.where(AgentMetric.metric_type == metric_type)
    
    query = query.order_by(AgentMetric.timestamp.desc()).limit(limit)
    
    result = await db.execute(query)
    return result.scalars().all()

async def get_metrics_summary(
    db: AsyncSession, 
    agent_id: str,
    hours: int = 24
) -> Dict[str, Any]:
    """Получение сводки метрик за последние N часов"""
    from datetime import timedelta
    
    cutoff_time = datetime.utcnow() - timedelta(hours=hours)
    
    # Получаем последние метрики каждого типа
    result = await db.execute(
        select(AgentMetric)
        .where(
            AgentMetric.agent_id == agent_id,
            AgentMetric.timestamp >= cutoff_time
        )
        .order_by(AgentMetric.metric_type, AgentMetric.timestamp.desc())
    )
    
    metrics = result.scalars().all()
    
    # Группируем по типу метрики
    summary = {}
    for metric in metrics:
        if metric.metric_type not in summary:
            summary[metric.metric_type] = metric
    
    return summary

# ===== CRUD для пользовательских параметров =====

async def create_user_parameter(
    db: AsyncSession, 
    agent_id: str, 
    parameter_key: str, 
    command: str
) -> AgentUserParameter:
    """Создание пользовательского параметра"""
    param = AgentUserParameter(
        agent_id=agent_id,
        parameter_key=parameter_key,
        command=command
    )
    db.add(param)
    await db.commit()
    await db.refresh(param)
    return param

async def get_user_parameters(db: AsyncSession, agent_id: str) -> List[AgentUserParameter]:
    """Получение пользовательских параметров агента"""
    result = await db.execute(
        select(AgentUserParameter).where(AgentUserParameter.agent_id == agent_id)
    )
    return result.scalars().all()

# ===== CRUD для справочников =====

async def get_interpreters(db: AsyncSession) -> List[Interpreter]:
    """Получение всех интерпретаторов"""
    result = await db.execute(select(Interpreter))
    return result.scalars().all()

async def get_metric_types(db: AsyncSession) -> List[MetricType]:
    """Получение всех типов метрик"""
    result = await db.execute(select(MetricType))
    return result.scalars().all()

# ===== Утилиты =====

async def agent_exists(db: AsyncSession, agent_id: str) -> bool:
    """Проверка существования агента"""
    result = await db.execute(
        select(Agent.agent_id).where(Agent.agent_id == agent_id)
    )
    return result.scalar_one_or_none() is not None

# ===== Управление конфигурацией агента =====

async def update_agent_config(db: AsyncSession, agent_id: str, config_data: Dict[str, Any]) -> Agent:
    """Обновление конфигурации агента"""
    agent = await get_agent(db, agent_id)
    if not agent:
        return None
    
    # Обновляем только переданные поля (исключаем relationship поля)
    relationship_fields = {'enabled_metrics', 'allowed_interpreters', 'user_parameters', 'metrics', 'network_connections'}
    for key, value in config_data.items():
        if hasattr(agent, key) and key not in relationship_fields:
            setattr(agent, key, value)
    
    await db.commit()
    await db.refresh(agent)
    return agent

# ===== Управление пользовательскими параметрами =====

async def create_user_parameter(db: AsyncSession, agent_id: str, parameter_data: Dict[str, Any]) -> AgentUserParameter:
    """Создание пользовательского параметра"""
    parameter = AgentUserParameter(
        agent_id=agent_id,
        parameter_key=parameter_data["parameter_key"],
        command=parameter_data["command"]
    )
    db.add(parameter)
    await db.commit()
    await db.refresh(parameter)
    return parameter

async def get_user_parameters(db: AsyncSession, agent_id: str) -> List[AgentUserParameter]:
    """Получение пользовательских параметров агента"""
    result = await db.execute(
        select(AgentUserParameter).where(AgentUserParameter.agent_id == agent_id)
    )
    return result.scalars().all()

async def update_user_parameter(db: AsyncSession, param_id: int, parameter_data: Dict[str, Any]) -> Optional[AgentUserParameter]:
    """Обновление пользовательского параметра"""
    result = await db.execute(
        select(AgentUserParameter).where(AgentUserParameter.id == param_id)
    )
    parameter = result.scalar_one_or_none()
    
    if not parameter:
        return None
    
    # Обновляем поля
    for key, value in parameter_data.items():
        if hasattr(parameter, key):
            setattr(parameter, key, value)
    
    await db.commit()
    await db.refresh(parameter)
    return parameter

async def delete_user_parameter(db: AsyncSession, param_id: int) -> bool:
    """Удаление пользовательского параметра"""
    result = await db.execute(
        select(AgentUserParameter).where(AgentUserParameter.id == param_id)
    )
    parameter = result.scalar_one_or_none()
    
    if not parameter:
        return False
    
    await db.delete(parameter)
    await db.commit()
    return True

# ===== Управление включенными метриками =====

async def create_agent_enabled_metric(db: AsyncSession, agent_id: str, metric_name: str) -> AgentEnabledMetric:
    """Создание записи о включенной метрике для агента"""
    # Проверяем, что метрика существует
    metric_type_result = await db.execute(
        select(MetricType).where(MetricType.name == metric_name)
    )
    if not metric_type_result.scalar_one_or_none():
        # Создаем тип метрики, если его нет
        metric_type = MetricType(name=metric_name)
        db.add(metric_type)
        await db.commit()
    
    # Создаем связь агент-метрика
    enabled_metric = AgentEnabledMetric(
        agent_id=agent_id,
        metric_name=metric_name
    )
    db.add(enabled_metric)
    await db.commit()
    await db.refresh(enabled_metric)
    return enabled_metric

# ===== Управление разрешенными интерпретаторами =====

async def create_agent_allowed_interpreter(db: AsyncSession, agent_id: str, interpreter_name: str) -> AgentAllowedInterpreter:
    """Создание записи о разрешенном интерпретаторе для агента"""
    # Проверяем, что интерпретатор существует
    interpreter_result = await db.execute(
        select(Interpreter).where(Interpreter.name == interpreter_name)
    )
    if not interpreter_result.scalar_one_or_none():
        # Создаем интерпретатор, если его нет
        interpreter = Interpreter(name=interpreter_name)
        db.add(interpreter)
        await db.commit()
    
    # Создаем связь агент-интерпретатор
    allowed_interpreter = AgentAllowedInterpreter(
        agent_id=agent_id,
        interpreter_name=interpreter_name
    )
    db.add(allowed_interpreter)
    await db.commit()
    await db.refresh(allowed_interpreter)
    return allowed_interpreter

async def delete_agent_enabled_metrics(db: AsyncSession, agent_id: str) -> bool:
    """Удаление всех включенных метрик агента"""
    result = await db.execute(
        select(AgentEnabledMetric).where(AgentEnabledMetric.agent_id == agent_id)
    )
    metrics = result.scalars().all()
    
    for metric in metrics:
        await db.delete(metric)
    
    await db.commit()
    return True

async def delete_agent_allowed_interpreters(db: AsyncSession, agent_id: str) -> bool:
    """Удаление всех разрешенных интерпретаторов агента"""
    result = await db.execute(
        select(AgentAllowedInterpreter).where(AgentAllowedInterpreter.agent_id == agent_id)
    )
    interpreters = result.scalars().all()
    
    for interpreter in interpreters:
        await db.delete(interpreter)
    
    await db.commit()
    return True

async def get_agent_enabled_metrics(db: AsyncSession, agent_id: str) -> List[AgentEnabledMetric]:
    """Получение включенных метрик агента"""
    result = await db.execute(
        select(AgentEnabledMetric).where(AgentEnabledMetric.agent_id == agent_id)
    )
    return result.scalars().all()

async def get_agent_allowed_interpreters(db: AsyncSession, agent_id: str) -> List[AgentAllowedInterpreter]:
    """Получение разрешенных интерпретаторов агента"""
    result = await db.execute(
        select(AgentAllowedInterpreter).where(AgentAllowedInterpreter.agent_id == agent_id)
    )
    return result.scalars().all()

