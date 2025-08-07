import requests
import json
from typing import List, Dict, Any, Optional
from datetime import datetime, timedelta
import asyncio
import aiohttp
from ..models.agent import (
    AgentInfo, AgentConfig, AgentStatus, AgentCommand, 
    AgentResponse, MetricsRequest, MetricType
)

class AgentService:
    """Сервис для управления агентами"""
    
    def __init__(self):
        self.agents: Dict[str, AgentInfo] = {}
        self.command_queue: Dict[str, List[AgentCommand]] = {}
        self.agent_timeout = 300  # 5 минут - агент считается офлайн
    
    def register_agent(self, agent_id: str, machine_name: str, machine_type: str, 
                      ip_address: Optional[str] = None) -> AgentInfo:
        """Регистрация нового агента"""
        agent_info = AgentInfo(
            agent_id=agent_id,
            machine_name=machine_name,
            machine_type=machine_type,
            status=AgentStatus.ONLINE,
            last_seen=datetime.now(),
            config=AgentConfig(
                agent_id=agent_id,
                machine_name=machine_name
            ),
            ip_address=ip_address
        )
        
        self.agents[agent_id] = agent_info
        self.command_queue[agent_id] = []
        return agent_info
    
    def update_agent_status(self, agent_id: str, status: AgentStatus = AgentStatus.ONLINE):
        """Обновление статуса агента"""
        if agent_id in self.agents:
            self.agents[agent_id].status = status
            self.agents[agent_id].last_seen = datetime.now()
    
    def get_agent(self, agent_id: str) -> Optional[AgentInfo]:
        """Получение информации об агенте"""
        return self.agents.get(agent_id)
    
    def get_all_agents(self) -> List[AgentInfo]:
        """Получение списка всех агентов"""
        # Проверяем статус агентов
        self._check_agent_statuses()
        return list(self.agents.values())
    
    def _check_agent_statuses(self):
        """Проверка статуса агентов по времени последнего получения метрик"""
        now = datetime.now()
        for agent_id, agent in self.agents.items():
            # Получаем интервал сбора метрик (по конфигу агента, либо дефолт 60 сек)
            interval = getattr(agent.config, 'update_frequency', 60)
            # Если метрики не приходили дольше, чем (интервал + 5 минут), агент OFFLINE
            if (now - agent.last_seen).total_seconds() > (interval + 300):
                agent.status = AgentStatus.OFFLINE
            else:
                agent.status = AgentStatus.ONLINE
    
    def update_agent_config(self, agent_id: str, config: AgentConfig) -> bool:
        """Обновление конфигурации агента с push на агент"""
        if agent_id in self.agents:
            self.agents[agent_id].config = config
            # Добавляем команду обновления конфигурации в очередь (для совместимости)
            command = AgentCommand(
                command="update_config",
                data=config.dict()
            )
            self.command_queue[agent_id].append(command)
            # PUSH: отправляем команду агенту по HTTP (асинхронно, не блокируя основной поток)
            import asyncio
            asyncio.create_task(self.send_command_to_agent(agent_id, command))
            return True
        return False
    
    def request_metrics(self, agent_id: str, metrics: List[MetricType] = None, 
                       immediate: bool = True) -> bool:
        """Запрос метрик от агента"""
        if agent_id not in self.agents:
            return False
        
        command = AgentCommand(
            command="collect_metrics",
            data={
                "metrics": [m.value for m in (metrics or [])],
                "immediate": immediate
            }
        )
        self.command_queue[agent_id].append(command)
        return True
    
    def request_metrics_from_all(self, metrics: List[MetricType] = None, 
                                immediate: bool = True) -> Dict[str, bool]:
        """Запрос метрик от всех агентов"""
        results = {}
        for agent_id in self.agents.keys():
            results[agent_id] = self.request_metrics(agent_id, metrics, immediate)
        return results
    
    def restart_agent(self, agent_id: str) -> bool:
        """Перезапуск агента"""
        if agent_id not in self.agents:
            return False
        
        command = AgentCommand(command="restart")
        self.command_queue[agent_id].append(command)
        return True
    
    def stop_agent(self, agent_id: str) -> bool:
        """Остановка агента"""
        if agent_id not in self.agents:
            return False
        
        command = AgentCommand(command="stop")
        self.command_queue[agent_id].append(command)
        return True
    
    def get_pending_commands(self, agent_id: str) -> List[AgentCommand]:
        """Получение ожидающих команд для агента"""
        return self.command_queue.get(agent_id, [])
    
    def remove_command(self, agent_id: str, command_index: int):
        """Удаление выполненной команды"""
        if agent_id in self.command_queue and command_index < len(self.command_queue[agent_id]):
            self.command_queue[agent_id].pop(command_index)
    
    async def send_command_to_agent(self, agent_id: str, command: AgentCommand) -> AgentResponse:
        """Отправка команды агенту по HTTP"""
        agent = self.get_agent(agent_id)
        if not agent or not agent.ip_address:
            return AgentResponse(
                success=False,
                message=f"Agent {agent_id} not found or no IP address"
            )
        
        try:
            # Предполагаем, что агент слушает на порту 8081
            agent_url = f"http://{agent.ip_address}:8081/command"
            
            async with aiohttp.ClientSession() as session:
                async with session.post(
                    agent_url,
                    json=command.dict(),
                    headers={"Content-Type": "application/json"},
                    timeout=aiohttp.ClientTimeout(total=10)
                ) as response:
                    if response.status == 200:
                        data = await response.json()
                        return AgentResponse(**data)
                    else:
                        return AgentResponse(
                            success=False,
                            message=f"HTTP {response.status}: {await response.text()}"
                        )
        except Exception as e:
            return AgentResponse(
                success=False,
                message=f"Error sending command: {str(e)}"
            )
    
    def get_agent_statistics(self) -> Dict[str, Any]:
        """Получение статистики по агентам"""
        self._check_agent_statuses()
        
        total = len(self.agents)
        online = sum(1 for agent in self.agents.values() if agent.status == AgentStatus.ONLINE)
        offline = sum(1 for agent in self.agents.values() if agent.status == AgentStatus.OFFLINE)
        error = sum(1 for agent in self.agents.values() if agent.status == AgentStatus.ERROR)
        
        return {
            "total_agents": total,
            "online_agents": online,
            "offline_agents": offline,
            "error_agents": error,
            "online_percentage": (online / total * 100) if total > 0 else 0
        }

# Глобальный экземпляр сервиса
agent_service = AgentService() 