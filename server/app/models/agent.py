from pydantic import BaseModel
from typing import List, Optional, Dict, Any
from datetime import datetime
from enum import Enum

class AgentStatus(str, Enum):
    ONLINE = "online"
    OFFLINE = "offline"
    ERROR = "error"

class MetricType(str, Enum):
    CPU = "cpu"
    MEMORY = "memory"
    DISK = "disk"
    NETWORK = "network"
    GPU = "gpu"
    HDD = "hdd"
    INVENTORY = "inventory"

class AgentRegistration(BaseModel):
    """Данные для регистрации агента"""
    machine_name: str
    machine_type: str
    ip_address: Optional[str] = None
    version: Optional[str] = None

class AgentConfig(BaseModel):
    """Конфигурация агента"""
    update_frequency: int = 600  # секунды (10 минут по умолчанию)
    enabled_metrics: Dict[str, bool] = {
        "cpu": True,
        "memory": True,
        "disk": True,
        "network": True,
        "gpu": False,
        "hdd": False,
        "inventory": True
    }
    server_url: str = "http://localhost:8000/metrics"
    agent_id: Optional[str] = None
    machine_name: Optional[str] = None

class AgentInfo(BaseModel):
    """Информация об агенте"""
    agent_id: str
    machine_name: str
    machine_type: str
    status: AgentStatus
    last_seen: datetime
    config: AgentConfig
    ip_address: Optional[str] = None
    version: Optional[str] = None

class AgentCommand(BaseModel):
    """Команда для агента"""
    command: str  # "collect_metrics", "update_config", "restart", "stop"
    data: Optional[Dict[str, Any]] = None
    timestamp: datetime = datetime.now()

class AgentResponse(BaseModel):
    """Ответ от агента"""
    success: bool
    message: str
    data: Optional[Dict[str, Any]] = None
    timestamp: datetime = datetime.now()

class MetricsRequest(BaseModel):
    """Запрос метрик от агента"""
    metrics: Dict[str, bool] = {}
    immediate: bool = True  # Немедленный сбор или по расписанию

class AgentCommandRequest(BaseModel):
    """Запрос команды для агента"""
    command: str
    agent_id: Optional[str] = None  # Если None, команда для всех агентов
    data: Optional[Dict[str, Any]] = None 