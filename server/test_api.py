#!/usr/bin/env python3
"""
Тестовый скрипт для проверки API Monitoring Server
"""

import requests
import json
import time

BASE_URL = "http://localhost:8000"

def test_health():
    """Тест проверки здоровья сервера"""
    print("🔍 Тестирование /health...")
    try:
        response = requests.get(f"{BASE_URL}/health")
        print(f"✅ Статус: {response.status_code}")
        print(f"📊 Данные: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка: {e}")
        return False

def test_agents():
    """Тест получения списка агентов"""
    print("\n📊 Тестирование /api/agents...")
    try:
        response = requests.get(f"{BASE_URL}/api/agents")
        print(f"✅ Статус: {response.status_code}")
        print(f"📊 Данные: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка: {e}")
        return False

def test_metrics():
    """Тест отправки метрик"""
    print("\n📈 Тестирование /metrics...")
    
    # Тестовые данные метрик
    test_metrics = {
        "timestamp": time.time(),
        "machine_type": "Windows",
        "cpu": {
            "usage_percent": 45.2,
            "temperature": 65.0,
            "core_temperatures": [62.0, 64.0, 66.0, 68.0],
            "core_usage": [40.0, 45.0, 50.0, 55.0]
        },
        "memory": {
            "total_bytes": 17179869184,  # 16 GB
            "used_bytes": 8589934592,     # 8 GB
            "free_bytes": 8589934592,     # 8 GB
            "usage_percent": 50.0
        },
        "disk": {
            "partitions": [
                {
                    "mount_point": "C:",
                    "filesystem": "NTFS",
                    "total_bytes": 256060514304,  # 256 GB
                    "used_bytes": 128030257152,   # 128 GB
                    "free_bytes": 128030257152,   # 128 GB
                    "usage_percent": 50.0
                }
            ]
        },
        "network": {
            "interfaces": [
                {
                    "name": "Ethernet",
                    "bytes_sent": 1024000,
                    "bytes_received": 2048000,
                    "packets_sent": 1000,
                    "packets_received": 2000,
                    "bandwidth_sent": 1024,
                    "bandwidth_received": 2048
                }
            ]
        }
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/metrics",
            json=test_metrics,
            headers={"Content-Type": "application/json"}
        )
        print(f"✅ Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка: {e}")
        return False

def test_config():
    """Тест конфигурации агента"""
    print("\n⚙️ Тестирование конфигурации агента...")
    
    agent_id = "test_agent_123"
    
    # Тест получения конфигурации
    try:
        response = requests.get(f"{BASE_URL}/api/agents/{agent_id}/config")
        print(f"✅ Получение конфигурации - Статус: {response.status_code}")
        print(f"📊 Конфигурация: {response.json()}")
    except Exception as e:
        print(f"❌ Ошибка получения конфигурации: {e}")
    
    # Тест обновления конфигурации
    new_config = {
        "update_frequency": 30,
        "enabled_metrics": ["cpu", "memory", "disk"],
        "custom_scripts": ["script1.sh", "script2.ps1"]
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/agents/{agent_id}/config",
            json=new_config,
            headers={"Content-Type": "application/json"}
        )
        print(f"✅ Обновление конфигурации - Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка обновления конфигурации: {e}")
        return False

def main():
    """Основная функция тестирования"""
    print("🧪 Тестирование Monitoring Server API")
    print("=" * 50)
    
    # Проверяем, что сервер запущен
    if not test_health():
        print("❌ Сервер не отвечает. Убедитесь, что он запущен на http://localhost:8000")
        return
    
    # Тестируем остальные endpoints
    test_agents()
    test_metrics()
    test_config()
    
    print("\n" + "=" * 50)
    print("✅ Тестирование завершено!")
    print("📖 Полная документация API: http://localhost:8000/docs")

if __name__ == "__main__":
    main() 