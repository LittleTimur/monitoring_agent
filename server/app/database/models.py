from sqlalchemy import Column, String, Boolean, Integer, BigInteger, Float, DateTime, Text, ForeignKey, CheckConstraint
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import relationship
from sqlalchemy.dialects.postgresql import JSONB, INET
from datetime import datetime
from typing import Optional, List

Base = declarative_base()

class Agent(Base):
    """Модель агента"""
    __tablename__ = "agents"
    
    agent_id = Column(String(255), primary_key=True)
    machine_name = Column(String(255), nullable=False)
    auto_detect_id = Column(Boolean, default=True)
    auto_detect_name = Column(Boolean, default=True)
    command_server_host = Column(INET, default='0.0.0.0')
    command_server_port = Column(Integer, default=8081)
    command_server_url = Column(Text)
    server_url = Column(Text, nullable=False)
    scripts_dir = Column(Text, default='scripts')
    audit_log_enabled = Column(Boolean, default=False)
    audit_log_path = Column(Text)
    enable_inline_commands = Column(Boolean, default=True)
    enable_user_parameters = Column(Boolean, default=True)
    job_retention_seconds = Column(Integer, default=3600)
    max_buffer_size = Column(Integer, default=10)
    max_concurrent_jobs = Column(Integer, default=3)
    max_output_bytes = Column(BigInteger, default=1000000)
    max_script_timeout_sec = Column(Integer, default=60)
    send_timeout_ms = Column(Integer, default=2000)
    update_frequency = Column(Integer, default=60)
    created_at = Column(DateTime(timezone=True), default=datetime.utcnow)
    last_heartbeat = Column(DateTime(timezone=True))
    
    # Связи
    user_parameters = relationship("AgentUserParameter", back_populates="agent", cascade="all, delete-orphan")
    allowed_interpreters = relationship("AgentAllowedInterpreter", back_populates="agent", cascade="all, delete-orphan")
    enabled_metrics = relationship("AgentEnabledMetric", back_populates="agent", cascade="all, delete-orphan")
    metrics = relationship("AgentMetric", back_populates="agent", cascade="all, delete-orphan")

class Interpreter(Base):
    """Модель интерпретатора"""
    __tablename__ = "interpreters"
    
    name = Column(String(50), primary_key=True)
    
    # Связи
    allowed_agents = relationship("AgentAllowedInterpreter", back_populates="interpreter")

class MetricType(Base):
    """Модель типа метрики"""
    __tablename__ = "metric_types"
    
    name = Column(String(50), primary_key=True)
    
    # Связи
    enabled_agents = relationship("AgentEnabledMetric", back_populates="metric_type")

class AgentAllowedInterpreter(Base):
    """Связь агент - разрешенные интерпретаторы"""
    __tablename__ = "agent_allowed_interpreters"
    
    agent_id = Column(String(255), ForeignKey("agents.agent_id", ondelete="CASCADE"), primary_key=True)
    interpreter_name = Column(String(50), ForeignKey("interpreters.name", ondelete="CASCADE"), primary_key=True)
    
    # Связи
    agent = relationship("Agent", back_populates="allowed_interpreters")
    interpreter = relationship("Interpreter", back_populates="allowed_agents")

class AgentEnabledMetric(Base):
    """Связь агент - включенные метрики"""
    __tablename__ = "agent_enabled_metrics"
    
    agent_id = Column(String(255), ForeignKey("agents.agent_id", ondelete="CASCADE"), primary_key=True)
    metric_name = Column(String(50), ForeignKey("metric_types.name", ondelete="CASCADE"), primary_key=True)
    
    # Связи
    agent = relationship("Agent", back_populates="enabled_metrics")
    metric_type = relationship("MetricType", back_populates="enabled_agents")

class AgentUserParameter(Base):
    """Модель пользовательских параметров агента"""
    __tablename__ = "agent_user_parameters"
    
    id = Column(Integer, primary_key=True, autoincrement=True)
    agent_id = Column(String(255), ForeignKey("agents.agent_id", ondelete="CASCADE"), nullable=False)
    parameter_key = Column(String(255), nullable=False)
    command = Column(Text, nullable=False)
    
    # Связи
    agent = relationship("Agent", back_populates="user_parameters")

class AgentMetric(Base):
    """Модель метрики агента"""
    __tablename__ = "agent_metrics"
    
    id = Column(BigInteger, primary_key=True, autoincrement=True)
    agent_id = Column(String(255), ForeignKey("agents.agent_id", ondelete="CASCADE"), nullable=False)
    timestamp = Column(DateTime(timezone=True), nullable=False)
    machine_type = Column(String(50), nullable=False)
    machine_name = Column(String(255), nullable=False)
    metric_type = Column(String(20), nullable=False)
    
    # Общие числовые метрики
    usage_percent = Column(Float)
    temperature = Column(Float)
    total_bytes = Column(BigInteger)
    used_bytes = Column(BigInteger)
    free_bytes = Column(BigInteger)
    
    # Детальные данные в JSONB
    details = Column(JSONB)
    
    # Связи
    agent = relationship("Agent", back_populates="metrics")
    network_connections = relationship("MetricNetworkConnection", back_populates="metric", cascade="all, delete-orphan")
    
    # Ограничения
    __table_args__ = (
        CheckConstraint("machine_type IN ('physical', 'virtual')", name="check_machine_type"),
        CheckConstraint("metric_type IN ('cpu', 'memory', 'disk', 'network', 'gpu', 'hdd', 'user', 'inventory')", name="check_metric_type"),
    )

class MetricNetworkConnection(Base):
    """Модель сетевого соединения"""
    __tablename__ = "metrics_network_connections"
    
    id = Column(BigInteger, primary_key=True, autoincrement=True)
    metric_id = Column(BigInteger, ForeignKey("agent_metrics.id", ondelete="CASCADE"), nullable=False)
    local_ip = Column(String(45))
    local_port = Column(Integer)
    remote_ip = Column(String(45))
    remote_port = Column(Integer)
    protocol = Column(String(10))
    
    # Связи
    metric = relationship("AgentMetric", back_populates="network_connections")
    
    # Ограничения
    __table_args__ = (
        CheckConstraint("protocol IN ('TCP', 'UDP')", name="check_protocol"),
    )

