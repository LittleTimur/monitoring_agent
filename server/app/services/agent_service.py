import requests
import json
from typing import List, Dict, Any, Optional
from datetime import datetime, timedelta
import asyncio
import aiohttp
import logging
from ..models.agent import (
    AgentInfo, AgentConfig, AgentStatus, AgentCommand, 
    AgentResponse, MetricsRequest, MetricType, AgentRegistration
)

# Настройка логирования
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class CommandStatus:
    """Статусы выполнения команд"""
    PENDING = "pending"
    IN_PROGRESS = "in_progress"
    COMPLETED = "completed"
    FAILED = "failed"
    TIMEOUT = "timeout"

class CommandExecution:
    """Информация о выполнении команды"""
    def __init__(self, command: AgentCommand, agent_id: str):
        self.command = command
        self.agent_id = agent_id
        self.status = CommandStatus.PENDING
        self.start_time = datetime.now()
        self.end_time: Optional[datetime] = None
        self.response: Optional[AgentResponse] = None
        self.error_message: Optional[str] = None
        self.retry_count = 0
        self.max_retries = 3

class AgentService:
    """Сервис для управления агентами"""
    
    def __init__(self):
        self.agents: Dict[str, AgentInfo] = {}
        self.command_queue: Dict[str, List[AgentCommand]] = {}
        self.command_executions: Dict[str, List[CommandExecution]] = {}
        self.agent_timeout = 300  # 5 минут - агент считается офлайн
        self.command_timeout = 120  # 120 секунд на выполнение команды
        self.max_concurrent_commands = 5  # Максимум одновременных команд на агента
    
    def register_agent(self, agent_id: str, registration_data: 'AgentRegistration') -> AgentInfo:
        """Регистрация нового агента"""
        logger.info(f"🔧 Регистрация агента {agent_id}")
        logger.info(f"   Machine name: {registration_data.machine_name}")
        logger.info(f"   Machine type: {registration_data.machine_type}")
        logger.info(f"   IP address: {registration_data.ip_address}")
        
        agent_info = AgentInfo(
            agent_id=agent_id,
            machine_name=registration_data.machine_name,
            machine_type=registration_data.machine_type,
            status=AgentStatus.ONLINE,
            last_seen=datetime.now(),
            config=AgentConfig(
                agent_id=agent_id,
                machine_name=registration_data.machine_name
            ),
            ip_address=registration_data.ip_address
        )
        
        self.agents[agent_id] = agent_info
        self.command_queue[agent_id] = []
        self.command_executions[agent_id] = []
        
        logger.info(f"✅ Агент {agent_id} успешно зарегистрирован")
        logger.info(f"   Всего агентов: {len(self.agents)}")
        
        return agent_info
    
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
            # Добавляем команду обновления конфигурации в очередь
            # Подготавливаем данные конфигурации для отправки
            config_data = config.dict()
            # Убираем поля, которые могут содержать datetime или None
            if 'agent_id' in config_data:
                del config_data['agent_id']
            if 'machine_name' in config_data:
                del config_data['machine_name']
            
            command = AgentCommand(
                command="update_config",
                data=config_data
            )
            self.command_queue[agent_id].append(command)
            # PUSH: отправляем команду агенту по HTTP (асинхронно)
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
    
    def get_command_executions(self, agent_id: str) -> List[CommandExecution]:
        """Получение истории выполнения команд для агента"""
        return self.command_executions.get(agent_id, [])
    
    def get_command_status(self, agent_id: str, command_index: int) -> Optional[CommandExecution]:
        """Получение статуса выполнения команды"""
        executions = self.command_executions.get(agent_id, [])
        if 0 <= command_index < len(executions):
            return executions[command_index]
        return None
    
    async def send_command_to_agent(self, agent_id: str, command: AgentCommand) -> AgentResponse:
        """Отправка команды агенту по HTTP с улучшенной обработкой"""
        agent = self.get_agent(agent_id)
        if not agent or not agent.ip_address:
            error_msg = f"Agent {agent_id} not found or no IP address"
            logger.error(error_msg)
            return AgentResponse(success=False, message=error_msg)
        
        # Создаем запись о выполнении команды
        execution = CommandExecution(command, agent_id)
        self.command_executions[agent_id].append(execution)
        execution_index = len(self.command_executions[agent_id]) - 1
        
        logger.info(f"🚀 Отправка команды '{command.command}' агенту {agent_id}")
        logger.info(f"   Команда #{execution_index}, данные: {command.data}")
        
        # Очищаем старые завершенные команды (старше 1 часа)
        now = datetime.now()
        self.command_executions[agent_id] = [
            ex for ex in self.command_executions[agent_id]
            if ex.status not in [CommandStatus.COMPLETED, CommandStatus.FAILED, CommandStatus.TIMEOUT] or
               (ex.end_time and (now - ex.end_time).total_seconds() < 3600)
        ]
        
        # Проверяем лимит одновременных команд
        active_commands = sum(1 for ex in self.command_executions[agent_id] 
                            if ex.status in [CommandStatus.PENDING, CommandStatus.IN_PROGRESS])
        
        if active_commands >= self.max_concurrent_commands:
            error_msg = f"Too many concurrent commands for agent {agent_id}"
            logger.warning(error_msg)
            execution.status = CommandStatus.FAILED
            execution.error_message = error_msg
            execution.end_time = datetime.now()
            return AgentResponse(success=False, message=error_msg)
        
        execution.status = CommandStatus.IN_PROGRESS
        
        try:
            # Предполагаем, что агент слушает на порту 8081
            agent_url = f"http://{agent.ip_address}:8081/command"
            
            # Подготавливаем данные для отправки, исключая datetime поля
            command_data = command.dict()
            # Убираем timestamp из данных команды для корректной сериализации
            if 'timestamp' in command_data:
                del command_data['timestamp']
            
            async with aiohttp.ClientSession() as session:
                async with session.post(
                    agent_url,
                    json=command_data,
                    headers={"Content-Type": "application/json"},
                    timeout=aiohttp.ClientTimeout(total=self.command_timeout)
                ) as response:
                    if response.status == 200:
                        data = await response.json()
                        
                        # Исправляем некорректный timestamp от агента
                        if 'timestamp' in data and (not data['timestamp'] or data['timestamp'] == ''):
                            data['timestamp'] = datetime.now().isoformat()
                        
                        try:
                            agent_response = AgentResponse(**data)
                        except Exception as validation_error:
                            # Если валидация не прошла, создаем ответ с текущим временем
                            logger.warning(f"⚠️ Ошибка валидации ответа от агента: {validation_error}")
                            logger.warning(f"   Полученные данные: {data}")
                            
                            # Создаем корректный ответ
                            agent_response = AgentResponse(
                                success=data.get('success', False),
                                message=data.get('message', 'Invalid response format'),
                                data=data.get('data'),
                                timestamp=datetime.now()
                            )
                        
                        if agent_response.success:
                            execution.status = CommandStatus.COMPLETED
                            logger.info(f"✅ Команда '{command.command}' успешно выполнена агентом {agent_id}")
                        else:
                            execution.status = CommandStatus.FAILED
                            logger.warning(f"⚠️ Команда '{command.command}' выполнена с ошибкой: {agent_response.message}")
                        
                        execution.response = agent_response
                        execution.end_time = datetime.now()
                        return agent_response
                    else:
                        error_msg = f"HTTP {response.status}: {await response.text()}"
                        logger.error(f"❌ Ошибка HTTP при выполнении команды: {error_msg}")
                        
                        # Попытка повторной отправки
                        if execution.retry_count < execution.max_retries:
                            execution.retry_count += 1
                            logger.info(f"🔄 Повторная попытка {execution.retry_count}/{execution.max_retries}")
                            await asyncio.sleep(2 ** execution.retry_count)  # Экспоненциальная задержка
                            # Не вызываем рекурсивно, а просто возвращаем ошибку
                            # Повторная попытка будет обработана на уровне выше
                            execution.status = CommandStatus.FAILED
                            execution.error_message = error_msg
                            execution.end_time = datetime.now()
                            return AgentResponse(success=False, message=error_msg)
                        else:
                            execution.status = CommandStatus.FAILED
                            execution.error_message = error_msg
                            execution.end_time = datetime.now()
                            return AgentResponse(success=False, message=error_msg)
                            
        except asyncio.TimeoutError:
            error_msg = f"Command timeout after {self.command_timeout} seconds"
            logger.error(f"⏰ Таймаут команды '{command.command}' для агента {agent_id}")
            
            if execution.retry_count < execution.max_retries:
                execution.retry_count += 1
                logger.info(f"🔄 Повторная попытка {execution.retry_count}/{execution.max_retries} после таймаута")
                await asyncio.sleep(2 ** execution.retry_count)
                # Не вызываем рекурсивно
                execution.status = CommandStatus.TIMEOUT
                execution.error_message = error_msg
                execution.end_time = datetime.now()
                return AgentResponse(success=False, message=error_msg)
            else:
                execution.status = CommandStatus.TIMEOUT
                execution.error_message = error_msg
                execution.end_time = datetime.now()
                return AgentResponse(success=False, message=error_msg)
                
        except Exception as e:
            error_msg = f"Error sending command: {str(e)}"
            logger.error(f"❌ Ошибка при отправке команды '{command.command}' агенту {agent_id}: {error_msg}")
            
            if execution.retry_count < execution.max_retries:
                execution.retry_count += 1
                logger.info(f"🔄 Повторная попытка {execution.retry_count}/{execution.max_retries} после ошибки")
                await asyncio.sleep(2 ** execution.retry_count)
                # Не вызываем рекурсивно
                execution.status = CommandStatus.FAILED
                execution.error_message = error_msg
                execution.end_time = datetime.now()
                return AgentResponse(success=False, message=error_msg)
            else:
                execution.status = CommandStatus.FAILED
                execution.error_message = error_msg
                execution.end_time = datetime.now()
                return AgentResponse(success=False, message=error_msg)
    
    def process_agent_metrics(self, agent_id: str, metrics_data: dict, client_ip: str) -> None:
        """Обработка метрик от агента - регистрация или обновление"""
        current_time = datetime.now()
        
        if agent_id not in self.agents:
            # Регистрируем нового агента (только один раз)
            self.agents[agent_id] = AgentInfo(
                agent_id=agent_id,
                machine_name=metrics_data.get("machine_name", "unknown"),
                machine_type=metrics_data.get("machine_type", "unknown"),
                status=AgentStatus.ONLINE,
                last_seen=current_time,
                config=AgentConfig(
                    agent_id=agent_id,
                    machine_name=metrics_data.get("machine_name", "unknown")
                ),
                ip_address=client_ip
            )
            self.command_queue[agent_id] = []
            self.command_executions[agent_id] = []
            logger.info(f" Новый агент {agent_id} зарегистрирован с IP {client_ip}")
        else:
            # Обновляем существующего агента
            agent = self.agents[agent_id]
            agent.status = AgentStatus.ONLINE
            agent.last_seen = current_time
            agent.ip_address = client_ip
            logger.info(f"📡 Агент {agent_id} обновлен (IP: {client_ip})")
    
    def get_agent_statistics(self) -> Dict[str, Any]:
        """Получение статистики по агентам"""
        self._check_agent_statuses()
        
        total = len(self.agents)
        online = sum(1 for agent in self.agents.values() if agent.status == AgentStatus.ONLINE)
        offline = sum(1 for agent in self.agents.values() if agent.status == AgentStatus.OFFLINE)
        error = sum(1 for agent in self.agents.values() if agent.status == AgentStatus.ERROR)
        
        # Статистика по командам
        total_commands = sum(len(executions) for executions in self.command_executions.values())
        completed_commands = sum(
            sum(1 for ex in executions if ex.status == CommandStatus.COMPLETED)
            for executions in self.command_executions.values()
        )
        failed_commands = sum(
            sum(1 for ex in executions if ex.status in [CommandStatus.FAILED, CommandStatus.TIMEOUT])
            for executions in self.command_executions.values()
        )
        
        return {
            "total_agents": total,
            "online_agents": online,
            "offline_agents": offline,
            "error_agents": error,
            "online_percentage": (online / total * 100) if total > 0 else 0,
            "commands": {
                "total": total_commands,
                "completed": completed_commands,
                "failed": failed_commands,
                "success_rate": (completed_commands / total_commands * 100) if total_commands > 0 else 0
            }
        }

# Глобальный экземпляр сервиса
agent_service = AgentService() 