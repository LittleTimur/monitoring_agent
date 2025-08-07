#!/usr/bin/env python3
"""
Тестовый скрипт для проверки управления агентами
"""

import requests
import json
import time
from typing import Dict, Any

BASE_URL = "http://localhost:8000"

def test_agent_registration():
    """Тест регистрации агентов"""
    print("🔧 Тестирование регистрации агентов...")
    
    # Регистрируем тестового агента
    agent_data = {
        "agent_id": "test_agent_001",
        "machine_name": "Test Machine 1",
        "machine_type": "Windows",
        "ip_address": "192.168.1.100"
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/agents/test_agent_001/register",
            params=agent_data
        )
        print(f"✅ Регистрация агента - Статус: {response.status_code}")
        print(f"📊 Данные: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка регистрации: {e}")
        return False

def test_agent_statistics():
    """Тест получения статистики агентов"""
    print("\n📊 Тестирование статистики агентов...")
    
    try:
        response = requests.get(f"{BASE_URL}/api/agents/statistics")
        print(f"✅ Статистика агентов - Статус: {response.status_code}")
        print(f"📊 Данные: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка получения статистики: {e}")
        return False

def test_agent_list():
    """Тест получения списка агентов"""
    print("\n📋 Тестирование списка агентов...")
    
    try:
        response = requests.get(f"{BASE_URL}/api/agents/")
        print(f"✅ Список агентов - Статус: {response.status_code}")
        print(f"📊 Количество агентов: {len(response.json())}")
        return True
    except Exception as e:
        print(f"❌ Ошибка получения списка: {e}")
        return False

def test_agent_config():
    """Тест конфигурации агента"""
    print("\n⚙️ Тестирование конфигурации агента...")
    
    agent_id = "test_agent_001"
    
    # Получаем текущую конфигурацию
    try:
        response = requests.get(f"{BASE_URL}/api/agents/{agent_id}/config")
        print(f"✅ Получение конфигурации - Статус: {response.status_code}")
        print(f"📊 Конфигурация: {response.json()}")
    except Exception as e:
        print(f"❌ Ошибка получения конфигурации: {e}")
    
    # Обновляем конфигурацию
    new_config = {
        "update_frequency": 30,
        "enabled_metrics": ["cpu", "memory", "disk"],
        "server_url": "http://localhost:8000/metrics",
        "agent_id": agent_id,
        "machine_name": "Test Machine 1"
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/agents/{agent_id}/config",
            json=new_config
        )
        print(f"✅ Обновление конфигурации - Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка обновления конфигурации: {e}")
        return False

def test_request_metrics():
    """Тест запроса метрик от агента"""
    print("\n📈 Тестирование запроса метрик...")
    
    agent_id = "test_agent_001"
    
    # Запрос метрик от конкретного агента
    metrics_request = {
        "metrics": ["cpu", "memory", "disk"],
        "immediate": True
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/agents/{agent_id}/request_metrics",
            json=metrics_request
        )
        print(f"✅ Запрос метрик от агента - Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
    except Exception as e:
        print(f"❌ Ошибка запроса метрик: {e}")
    
    # Запрос метрик от всех агентов
    try:
        response = requests.post(
            f"{BASE_URL}/api/agents/request_metrics_from_all",
            json=metrics_request
        )
        print(f"✅ Запрос метрик от всех агентов - Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка запроса метрик от всех: {e}")
        return False

def test_agent_commands():
    """Тест отправки команд агентам"""
    print("\n🎮 Тестирование команд агентам...")
    
    agent_id = "test_agent_001"
    
    # Команда перезапуска
    try:
        response = requests.post(f"{BASE_URL}/api/agents/{agent_id}/restart")
        print(f"✅ Команда перезапуска - Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
    except Exception as e:
        print(f"❌ Ошибка команды перезапуска: {e}")
    
    # Команда остановки
    try:
        response = requests.post(f"{BASE_URL}/api/agents/{agent_id}/stop")
        print(f"✅ Команда остановки - Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
    except Exception as e:
        print(f"❌ Ошибка команды остановки: {e}")
    
    # Произвольная команда
    command = {
        "command": "test_command",
        "data": {"test_param": "test_value"},
        "timestamp": "2024-01-01T00:00:00"
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/agents/{agent_id}/command",
            json=command
        )
        print(f"✅ Произвольная команда - Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка произвольной команды: {e}")
        return False

def test_metrics_reception():
    """Тест получения метрик от агента"""
    print("\n📥 Тестирование получения метрик...")
    
    # Симулируем отправку метрик от агента
    metrics_data = {
        "timestamp": time.time(),
        "machine_type": "Windows",
        "agent_id": "test_agent_001",
        "machine_name": "Test Machine 1",
        "cpu": {
            "usage_percent": 45.2,
            "temperature": 65.0,
            "core_temperatures": [62.0, 64.0, 66.0, 68.0],
            "core_usage": [40.0, 45.0, 50.0, 55.0]
        },
        "memory": {
            "total_bytes": 17179869184,
            "used_bytes": 8589934592,
            "free_bytes": 8589934592,
            "usage_percent": 50.0
        }
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/metrics",
            json=metrics_data,
            headers={"Content-Type": "application/json"}
        )
        print(f"✅ Получение метрик - Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка получения метрик: {e}")
        return False

def main():
    """Основная функция тестирования"""
    print("🧪 Тестирование управления агентами")
    print("=" * 50)
    
    # Проверяем, что сервер запущен
    try:
        response = requests.get(f"{BASE_URL}/health")
        if response.status_code != 200:
            print("❌ Сервер не отвечает. Убедитесь, что он запущен на http://localhost:8000")
            return
    except Exception as e:
        print(f"❌ Сервер не отвечает: {e}")
        return
    
    # Тестируем все функции
    test_agent_registration()
    test_agent_statistics()
    test_agent_list()
    test_agent_config()
    test_request_metrics()
    test_agent_commands()
    test_metrics_reception()
    
    print("\n" + "=" * 50)
    print("✅ Тестирование управления агентами завершено!")
    print("📖 Полная документация API: http://localhost:8000/docs")

if __name__ == "__main__":
    main() 