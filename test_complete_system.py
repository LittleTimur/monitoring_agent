#!/usr/bin/env python3
"""
Полный тест системы мониторинга
Проверяет: агент, сервер, отправку команд, фильтрацию метрик
"""

import requests
import json
import time
import subprocess
import sys
import os
from typing import Dict, Any

BASE_URL = "http://localhost:8000"

def check_server_running():
    """Проверяет, запущен ли сервер"""
    try:
        response = requests.get(f"{BASE_URL}/health", timeout=5)
        if response.status_code == 200:
            print("✅ Сервер запущен и отвечает")
            return True
        else:
            print(f"❌ Сервер отвечает с ошибкой: {response.status_code}")
            return False
    except Exception as e:
        print(f"❌ Сервер не отвечает: {e}")
        return False

def test_agent_registration():
    """Тест регистрации агента"""
    print("\n🔧 Тестирование регистрации агента...")
    
    # Симулируем отправку метрик от агента
    metrics_data = {
        "timestamp": time.time(),
        "machine_type": "Windows",
        "agent_id": "test_agent_auto_detect",
        "machine_name": "Test Machine",
        "cpu": {
            "usage_percent": 45.2,
            "temperature": 65.0
        },
        "memory": {
            "usage_percent": 60.1,
            "total_bytes": 8589934592
        }
    }
    
    try:
        response = requests.post(f"{BASE_URL}/metrics", json=metrics_data)
        print(f"✅ Отправка метрик - Статус: {response.status_code}")
        print(f"📊 Ответ: {response.json()}")
        return True
    except Exception as e:
        print(f"❌ Ошибка отправки метрик: {e}")
        return False

def test_agent_listing():
    """Тест получения списка агентов"""
    print("\n📋 Тестирование получения списка агентов...")
    
    try:
        response = requests.get(f"{BASE_URL}/api/agents")
        print(f"✅ Получение списка агентов - Статус: {response.status_code}")
        agents = response.json()
        print(f"📊 Найдено агентов: {len(agents)}")
        for agent in agents:
            print(f"   - {agent['agent_id']} ({agent['machine_name']}) - {agent['status']}")
        return True
    except Exception as e:
        print(f"❌ Ошибка получения списка агентов: {e}")
        return False

def test_metrics_configuration():
    """Тест конфигурации метрик с новой структурой"""
    print("\n⚙️ Тестирование конфигурации метрик...")
    
    agent_id = "test_agent_auto_detect"
    
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
    
    agent_id = "test_agent_auto_detect"
    
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
    
    agent_id = "test_agent_auto_detect"
    
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
    
    agent_id = "test_agent_auto_detect"
    
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

def test_metrics_filtering():
    """Тест сбора метрик с фильтрацией"""
    print("\n🔍 Тестирование сбора метрик с фильтрацией...")
    
    agent_id = "test_agent_auto_detect"
    
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

def test_agent_statistics():
    """Тест статистики агентов"""
    print("\n📊 Тестирование статистики агентов...")
    
    try:
        response = requests.get(f"{BASE_URL}/api/agents/statistics")
        print(f"✅ Получение статистики - Статус: {response.status_code}")
        stats = response.json()
        print(f"📊 Статистика: {stats}")
        return True
    except Exception as e:
        print(f"❌ Ошибка получения статистики: {e}")
        return False

def main():
    """Основная функция тестирования"""
    print("🧪 Полное тестирование системы мониторинга")
    print("=" * 60)
    
    # Проверяем, что сервер запущен
    if not check_server_running():
        print("\n❌ Сервер не запущен. Запустите сервер командой:")
        print("cd server && python run.py")
        return
    
    # Тестируем все функции
    tests = [
        ("Регистрация агента", test_agent_registration),
        ("Список агентов", test_agent_listing),
        ("Конфигурация метрик", test_metrics_configuration),
        ("Запрос метрик с флагами", test_metrics_request_with_flags),
        ("Обратная совместимость", test_backward_compatibility),
        ("Команды агента", test_agent_commands),
        ("Фильтрация метрик", test_metrics_filtering),
        ("Статистика агентов", test_agent_statistics),
    ]
    
    results = []
    for test_name, test_func in tests:
        try:
            success = test_func()
            results.append((test_name, success))
        except Exception as e:
            print(f"❌ Ошибка в тесте '{test_name}': {e}")
            results.append((test_name, False))
    
    # Выводим результаты
    print("\n" + "=" * 60)
    print("📋 Результаты тестирования:")
    print("=" * 60)
    
    passed = 0
    total = len(results)
    
    for test_name, success in results:
        status = "✅ ПРОЙДЕН" if success else "❌ ПРОВАЛЕН"
        print(f"{status} - {test_name}")
        if success:
            passed += 1
    
    print("\n" + "=" * 60)
    print(f"📊 Итого: {passed}/{total} тестов пройдено")
    
    if passed == total:
        print("🎉 Все тесты пройдены успешно!")
    else:
        print("⚠️  Некоторые тесты не пройдены")
    
    print("\n📖 Документация API: http://localhost:8000/docs")
    print("🔧 Для запуска агента: ./monitoring_agent_new.exe")

if __name__ == "__main__":
    main() 