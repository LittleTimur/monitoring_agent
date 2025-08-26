from pydantic import BaseModel, Field
from typing import List, Optional, Dict, Any, Literal, Union
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
    USER = "user"

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
        "inventory": True,
        "user": True
    }
    server_url: str = "http://localhost:8000/metrics"
    agent_id: Optional[str] = None
    machine_name: Optional[str] = None
    # Настройки запуска скриптов
    scripts_dir: str = "scripts"
    allowed_interpreters: List[str] = ["bash", "powershell", "cmd", "python"]
    max_script_timeout_sec: int = 60
    max_output_bytes: int = 1_000_000
    enable_user_parameters: bool = True
    enable_inline_commands: bool = False
    user_parameters: Dict[str, str] = {}

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
    timestamp: Union[datetime, str] = Field(default_factory=datetime.now)
    
    class Config:
        # Автоматическая конвертация строки в datetime
        json_encoders = {
            datetime: lambda v: v.isoformat() if isinstance(v, datetime) else v
        }

class MetricsRequest(BaseModel):
    """Запрос метрик от агента"""
    metrics: Dict[str, bool] = {}
    immediate: bool = True  # Немедленный сбор или по расписанию

class AgentCommandRequest(BaseModel):
    """Запрос команды для агента"""
    command: str
    agent_id: Optional[str] = None  # Если None, команда для всех агентов
    data: Optional[Dict[str, Any]] = None 


class RunScriptRequest(BaseModel):
    """Запрос запуска скрипта через агента.

    Примечание (Windows): поле env пока не применяется. Используйте
    cmd:   cmd.exe /c "set FOO=BAR && your_cmd"
    PowerShell: $env:FOO='BAR'; your_cmd
    """
    # Один из: script | script_path | key
    script: Optional[str] = None
    script_path: Optional[str] = None
    key: Optional[str] = None
    params: Optional[List[str]] = None

    interpreter: Optional[Literal['auto','bash','powershell','cmd','python']] = 'auto'
    args: Optional[List[str]] = None
    env: Optional[Dict[str, str]] = None
    working_dir: Optional[str] = None
    timeout_sec: Optional[int] = None
    capture_output: Optional[bool] = True
    background: Optional[bool] = False

    def dict(self, *args, **kwargs):  # type: ignore[override]
        d = super().dict(*args, **kwargs)
        # Удаляем None, чтобы не засорять payload
        return {k: v for k, v in d.items() if v is not None}