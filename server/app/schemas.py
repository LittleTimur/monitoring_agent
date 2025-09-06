from __future__ import annotations

from pydantic import BaseModel, Field, validator
from typing import Optional, List, Dict, Any, Union
from datetime import datetime
from enum import Enum

# ===== Enums =====

class MachineType(str, Enum):
    PHYSICAL = "physical"
    VIRTUAL = "virtual"

class MetricType(str, Enum):
    CPU = "cpu"
    MEMORY = "memory"
    DISK = "disk"
    NETWORK = "network"
    GPU = "gpu"
    HDD = "hdd"
    USER = "user"
    INVENTORY = "inventory"

class Protocol(str, Enum):
    TCP = "TCP"
    UDP = "UDP"

# ===== Схемы агентов =====

class AgentBase(BaseModel):
    machine_name: str = Field(..., description="Имя машины")
    auto_detect_id: bool = Field(True, description="Автоопределение ID")
    auto_detect_name: bool = Field(True, description="Автоопределение имени")
    command_server_host: str = Field("0.0.0.0", description="IP для прослушивания команд")
    command_server_port: int = Field(8081, description="Порт для прослушивания команд")
    command_server_url: Optional[str] = Field(None, description="URL командного сервера")
    server_url: str = Field(..., description="URL центрального сервера")
    scripts_dir: str = Field("scripts", description="Директория скриптов")
    audit_log_enabled: bool = Field(False, description="Включено логирование")
    audit_log_path: Optional[str] = Field(None, description="Путь к логу")
    enable_inline_commands: bool = Field(True, description="Разрешены inline-команды")
    enable_user_parameters: bool = Field(True, description="Разрешены пользовательские параметры")
    job_retention_seconds: int = Field(3600, description="Время хранения результатов")
    max_buffer_size: int = Field(10, description="Макс. размер буфера")
    max_concurrent_jobs: int = Field(3, description="Макс. число одновременных задач")
    max_output_bytes: int = Field(1000000, description="Макс. размер вывода")
    max_script_timeout_sec: int = Field(60, description="Макс. время выполнения скрипта")
    send_timeout_ms: int = Field(2000, description="Таймаут отправки")
    update_frequency: int = Field(60, description="Частота обновления")

class AgentCreate(AgentBase):
    agent_id: str = Field(..., description="Уникальный ID агента")

class AgentResponse(AgentBase):
    agent_id: str
    created_at: datetime
    last_heartbeat: Optional[datetime]
    
    # Связанные данные
    enabled_metrics: Optional[List[str]] = Field(None, description="Включенные метрики")
    allowed_interpreters: Optional[List[str]] = Field(None, description="Разрешенные интерпретаторы")
    user_parameters: Optional[List["UserParameterResponse"]] = Field(None, description="Пользовательские параметры")
    
    @validator('command_server_host', pre=True)
    def convert_ip_to_string(cls, v):
        if hasattr(v, '__str__'):
            return str(v)
        return v
    
    @validator('enabled_metrics', pre=True)
    def convert_enabled_metrics(cls, v):
        if v is None:
            return None
        if isinstance(v, list):
            # Если это список объектов AgentEnabledMetric
            return [item.metric_type.name if hasattr(item, 'metric_type') else str(item) for item in v]
        return v
    
    @validator('allowed_interpreters', pre=True)
    def convert_allowed_interpreters(cls, v):
        if v is None:
            return None
        if isinstance(v, list):
            # Если это список объектов AgentAllowedInterpreter
            return [item.interpreter.name if hasattr(item, 'interpreter') else str(item) for item in v]
        return v
    
    class Config:
        from_attributes = True

class AgentHeartbeat(BaseModel):
    agent_id: str = Field(..., description="ID агента")

# ===== Схемы метрик =====

class NetworkConnection(BaseModel):
    local_ip: Optional[str] = Field(None, description="Локальный IP")
    local_port: Optional[int] = Field(None, description="Локальный порт")
    remote_ip: Optional[str] = Field(None, description="Удаленный IP")
    remote_port: Optional[int] = Field(None, description="Удаленный порт")
    protocol: Optional[Protocol] = Field(None, description="Протокол")

class MetricBase(BaseModel):
    agent_id: str = Field(..., description="ID агента")
    machine_type: MachineType = Field(..., description="Тип машины")
    machine_name: str = Field(..., description="Имя машины")
    metric_type: MetricType = Field(..., description="Тип метрики")
    usage_percent: Optional[float] = Field(None, description="Процент использования")
    temperature: Optional[float] = Field(None, description="Температура")
    total_bytes: Optional[int] = Field(None, description="Общий объем в байтах")
    used_bytes: Optional[int] = Field(None, description="Использованный объем в байтах")
    free_bytes: Optional[int] = Field(None, description="Свободный объем в байтах")
    details: Optional[Dict[str, Any]] = Field(None, description="Детальные данные в JSON")

class MetricCreate(MetricBase):
    timestamp: Optional[datetime] = Field(default_factory=datetime.utcnow, description="Временная метка")
    network_connections: Optional[List[NetworkConnection]] = Field(None, description="Сетевые соединения")

class MetricResponse(MetricBase):
    id: int
    timestamp: datetime
    network_connections: Optional[List[NetworkConnection]] = None
    
    class Config:
        from_attributes = True

# ===== Схемы пользовательских параметров =====

class UserParameterCreate(BaseModel):
    parameter_key: str = Field(..., description="Ключ параметра")
    command: str = Field(..., description="Команда для выполнения")

class UserParameterResponse(UserParameterCreate):
    id: int
    agent_id: str
    
    class Config:
        from_attributes = True

# ===== Схемы команд агента =====

class AgentCommand(BaseModel):
    """Команда для отправки агенту"""
    command: str = Field(..., description="Тип команды")
    data: Optional[Dict[str, Any]] = Field(None, description="Данные команды")
    timestamp: Optional[str] = Field(None, description="Временная метка")

class AgentCommandResponse(BaseModel):
    """Ответ от агента на команду"""
    success: bool = Field(..., description="Успешность выполнения")
    message: str = Field(..., description="Сообщение")
    data: Optional[Dict[str, Any]] = Field(None, description="Данные ответа")
    timestamp: str = Field(..., description="Временная метка")

class ScriptExecutionRequest(BaseModel):
    """Запрос на выполнение скрипта"""
    interpreter: Optional[str] = Field("auto", description="Интерпретатор")
    script: Optional[str] = Field(None, description="Код скрипта")
    script_path: Optional[str] = Field(None, description="Путь к скрипту")
    args: Optional[List[str]] = Field(None, description="Аргументы")
    env: Optional[Dict[str, str]] = Field(None, description="Переменные окружения")
    working_dir: Optional[str] = Field(None, description="Рабочая директория")
    timeout_sec: Optional[int] = Field(None, description="Таймаут выполнения")
    capture_output: Optional[bool] = Field(True, description="Захватывать вывод")

class ConfigUpdateRequest(BaseModel):
    """Запрос на обновление конфигурации агента"""
    enabled_metrics: Optional[Dict[str, bool]] = Field(None, description="Включенные метрики")
    allowed_interpreters: Optional[List[str]] = Field(None, description="Разрешенные интерпретаторы")
    update_frequency: Optional[int] = Field(None, description="Частота обновления")
    max_script_timeout_sec: Optional[int] = Field(None, description="Макс. время выполнения скрипта")
    max_output_bytes: Optional[int] = Field(None, description="Макс. размер вывода")
    audit_log_enabled: Optional[bool] = Field(None, description="Включено логирование")
    audit_log_path: Optional[str] = Field(None, description="Путь к логу")

# ===== Схемы справочников =====

class InterpreterResponse(BaseModel):
    name: str
    
    class Config:
        from_attributes = True

class MetricTypeResponse(BaseModel):
    name: str
    
    class Config:
        from_attributes = True

# ===== Схемы для API =====

class AgentListResponse(BaseModel):
    agents: List[AgentResponse]
    total: int

class MetricListResponse(BaseModel):
    metrics: List[MetricResponse]
    total: int

class MetricsSummaryResponse(BaseModel):
    agent_id: str
    summary: Dict[str, MetricResponse]
    hours: int

# ===== Валидаторы =====

@validator('agent_id')
def validate_agent_id(cls, v):
    if not v or len(v) > 255:
        raise ValueError('Agent ID должен быть непустой строкой до 255 символов')
    return v

@validator('machine_name')
def validate_machine_name(cls, v):
    if not v or len(v) > 255:
        raise ValueError('Machine name должен быть непустой строкой до 255 символов')
    return v

@validator('server_url')
def validate_server_url(cls, v):
    if not v:
        raise ValueError('Server URL обязателен')
    return v
