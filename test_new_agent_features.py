#!/usr/bin/env python3
"""
Тестовый скрипт для проверки новых возможностей агента
"""

import requests
import json
import time
from typing import Dict, Any

BASE_URL = "http://localhost:8000"

def test_agent_registration_with_auto_detection():
    """Тест регистрации агента с автоматическим определением ID/имени"""
    print("🔧 Тестирование регистрации агента с автоопределением...")
    
    # Регистрируем агента с пустыми ID/имя (должны определиться автоматически)
    agent_data = {
        "machine_name": "",  # Будет определено автоматически
        "machine_type": "Windows",
        "ip_address": "127.0.0.1"
    }
    
    try:
        # Используем временный ID, который будет заменен автоматически
        temp_agent_id = "temp_agent_" + str(int(time.time()))
        response = requests.post(
            f"{BASE_URL}/api/agents/{temp_agent_id}/register",
            params=agent_data
        )
        print(f"✅ Регистрация агента - Статус: {response.status_code}")
        print(f"📊 Данные: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка регистрации: {e}")
        return False

def test_metrics_configuration():
    """Тест конфигурации метрик с новой структурой"""
    print("\n⚙️ Тестирование конфигурации метрик...")
    
    agent_id = "test_agent_001"
    
    # Новая структура конфигурации с флагами метрик
    new_config = {
        "enabled_metrics": {
            "cpu": True,
            "memory": True,
            "disk": False,  # Отключаем диск
            "network": True,
            "gpu": False,
            "hdd": False,
            "inventory": True
        },
        "heartbeat_interval_seconds": 30
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/agents/{agent_id}/config",
            json=new_config
        )
        print(f"✅ Обновление конфигурации метрик - Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка обновления конфигурации: {e}")
        return False

def test_metrics_request_with_flags():
    """Тест запроса метрик с новой структурой флагов"""
    print("\n📈 Тестирование запроса метрик с флагами...")
    
    agent_id = "test_agent_001"
    
    # Запрос метрик с новой структурой (объект с флагами)
    metrics_request = {
        "metrics": {
            "cpu": True,
            "memory": True,
            "disk": False,  # Не собираем диск
            "network": True,
            "gpu": False,
            "hdd": False,
            "inventory": True
        },
        "immediate": True
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/agents/{agent_id}/request_metrics",
            json=metrics_request
        )
        print(f"✅ Запрос метрик с флагами - Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка запроса метрик: {e}")
        return False

def test_backward_compatibility():
    """Тест обратной совместимости со старым форматом"""
    print("\n🔄 Тестирование обратной совместимости...")
    
    agent_id = "test_agent_001"
    
    # Старый формат (массив строк)
    old_format_request = {
        "metrics": ["cpu", "memory", "disk"],
        "immediate": True
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/agents/{agent_id}/request_metrics",
            json=old_format_request
        )
        print(f"✅ Запрос метрик (старый формат) - Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка запроса метрик (старый формат): {e}")
        return False

def test_agent_commands():
    """Тест команд агента с новой структурой"""
    print("\n🎮 Тестирование команд агента...")
    
    agent_id = "test_agent_001"
    
    # Команда обновления конфигурации с новой структурой
    config_command = {
        "command": "update_config",
        "data": {
            "enabled_metrics": {
                "cpu": True,
                "memory": True,
                "disk": False,
                "network": True,
                "gpu": False,
                "hdd": False,
                "inventory": True
            },
            "heartbeat_interval_seconds": 45
        },
        "timestamp": "2024-01-01T00:00:00"
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/agents/{agent_id}/command",
            json=config_command
        )
        print(f"✅ Команда обновления конфигурации - Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка команды обновления конфигурации: {e}")
        return False

def test_metrics_collection_with_filtering():
    """Тест сбора метрик с фильтрацией"""
    print("\n🔍 Тестирование сбора метрик с фильтрацией...")
    
    agent_id = "test_agent_001"
    
    # Запрос только CPU и памяти
    filtered_request = {
        "metrics": {
            "cpu": True,
            "memory": True,
            "disk": False,
            "network": False,
            "gpu": False,
            "hdd": False,
            "inventory": False
        },
        "immediate": True
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/agents/{agent_id}/request_metrics",
            json=filtered_request
        )
        print(f"✅ Запрос отфильтрованных метрик - Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка запроса отфильтрованных метрик: {e}")
        return False

def main():
    """Основная функция тестирования"""
    print("🧪 Тестирование новых возможностей агента")
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
    
    # Тестируем все новые функции
    test_agent_registration_with_auto_detection()
    test_metrics_configuration()
    test_metrics_request_with_flags()
    test_backward_compatibility()
    test_agent_commands()
    test_metrics_collection_with_filtering()
    
    print("\n" + "=" * 50)
    print("✅ Тестирование новых возможностей агента завершено!")
    print("📖 Полная документация API: http://localhost:8000/docs")

if __name__ == "__main__":
    main() 