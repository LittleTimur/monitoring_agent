#!/usr/bin/env python3
"""
Тест для проверки работы с русским текстом в командах агента
"""

import requests
import json
import time
import sys

# URL сервера
SERVER_URL = "http://localhost:8000"
AGENT_ID = "test_agent_001"  # ID тестового агента

def test_russian_text_commands():
    """Тестирует команды с русским текстом"""
    
    print("🧪 Тестирование команд с русским текстом...")
    
    # 1. Регистрируем тестового агента
    print("\n1️⃣ Регистрация тестового агента...")
    registration_data = {
        "machine_name": "ТестоваяМашина",
        "machine_type": "Тест",
        "ip_address": "127.0.0.1",
        "version": "1.0.0"
    }
    
    try:
        response = requests.post(f"{SERVER_URL}/api/agents/{AGENT_ID}/register", json=registration_data)
        if response.status_code == 200:
            print("✅ Агент зарегистрирован успешно")
        else:
            print(f"⚠️ Ошибка регистрации: {response.status_code}")
    except Exception as e:
        print(f"❌ Ошибка при регистрации: {e}")
        return False
    
    # 2. Тестируем команду PowerShell с русским текстом
    print("\n2️⃣ Тестирование PowerShell команды с русским текстом...")
    
    # Команда для вывода "Привет, мир!" через PowerShell
    powershell_command = {
        "command": "run_script",
        "data": {
            "script": 'Write-Host "Привет, мир!" -ForegroundColor Green',
            "interpreter": "powershell",
            "capture_output": True,
            "timeout_sec": 30
        }
    }
    
    try:
        response = requests.post(f"{SERVER_URL}/api/agents/{AGENT_ID}/command", json=powershell_command)
        if response.status_code == 200:
            print("✅ PowerShell команда отправлена успешно")
            result = response.json()
            print(f"   Результат: {result}")
        else:
            print(f"❌ Ошибка отправки PowerShell команды: {response.status_code}")
            print(f"   Ответ: {response.text}")
    except Exception as e:
        print(f"❌ Ошибка при отправке PowerShell команды: {e}")
    
    # 3. Тестируем команду cmd с русским текстом
    print("\n3️⃣ Тестирование cmd команды с русским текстом...")
    
    # Команда для вывода "Привет, мир!" через cmd
    cmd_command = {
        "command": "run_script",
        "data": {
            "script": 'echo Привет, мир!',
            "interpreter": "cmd",
            "capture_output": True,
            "timeout_sec": 30
        }
    }
    
    try:
        response = requests.post(f"{SERVER_URL}/api/agents/{AGENT_ID}/command", json=cmd_command)
        if response.status_code == 200:
            print("✅ CMD команда отправлена успешно")
            result = response.json()
            print(f"   Результат: {result}")
        else:
            print(f"❌ Ошибка отправки CMD команды: {response.status_code}")
            print(f"   Ответ: {response.text}")
    except Exception as e:
        print(f"❌ Ошибка при отправке CMD команды: {e}")
    
    # 4. Тестируем команду с русскими параметрами
    print("\n4️⃣ Тестирование команды с русскими параметрами...")
    
    # Команда с русскими параметрами
    russian_params_command = {
        "command": "run_script",
        "data": {
            "script": 'Write-Host "Параметр: $args[0]" -ForegroundColor Yellow',
            "interpreter": "powershell",
            "args": ["ТестовыйПараметр"],
            "capture_output": True,
            "timeout_sec": 30
        }
    }
    
    try:
        response = requests.post(f"{SERVER_URL}/api/agents/{AGENT_ID}/command", json=russian_params_command)
        if response.status_code == 200:
            print("✅ Команда с русскими параметрами отправлена успешно")
            result = response.json()
            print(f"   Результат: {result}")
        else:
            print(f"❌ Ошибка отправки команды с параметрами: {response.status_code}")
            print(f"   Ответ: {response.text}")
    except Exception as e:
        print(f"❌ Ошибка при отправке команды с параметрами: {e}")
    
    # 5. Проверяем статус выполнения команд
    print("\n5️⃣ Проверка статуса выполнения команд...")
    
    try:
        response = requests.get(f"{SERVER_URL}/api/agents/{AGENT_ID}/command-executions")
        if response.status_code == 200:
            executions = response.json()
            print(f"✅ Получена история команд: {len(executions.get('executions', []))} команд")
            for i, execution in enumerate(executions.get('executions', [])):
                print(f"   Команда {i}: {execution.get('command')} - {execution.get('status')}")
        else:
            print(f"❌ Ошибка получения истории команд: {response.status_code}")
    except Exception as e:
        print(f"❌ Ошибка при получении истории команд: {e}")
    
    print("\n🎯 Тестирование завершено!")
    return True

def test_simple_hello_world():
    """Простой тест команды 'привет мир'"""
    
    print("\n🔍 Простой тест команды 'привет мир'...")
    
    # Простая команда для вывода "привет мир"
    hello_command = {
        "command": "run_script",
        "data": {
            "script": 'echo "привет мир"',
            "interpreter": "cmd",
            "capture_output": True,
            "timeout_sec": 30
        }
    }
    
    try:
        response = requests.post(f"{SERVER_URL}/api/agents/{AGENT_ID}/command", json=hello_command)
        if response.status_code == 200:
            print("✅ Команда 'привет мир' отправлена успешно")
            result = response.json()
            print(f"   Результат: {result}")
            
            # Ждем немного и проверяем результат
            time.sleep(2)
            
            # Проверяем последнюю команду
            response = requests.get(f"{SERVER_URL}/api/agents/{AGENT_ID}/command-executions")
            if response.status_code == 200:
                executions = response.json()
                if executions.get('executions'):
                    last_execution = executions['executions'][-1]
                    print(f"   Статус выполнения: {last_execution.get('status')}")
                    if last_execution.get('response'):
                        print(f"   Ответ агента: {last_execution['response']}")
            
        else:
            print(f"❌ Ошибка отправки команды: {response.status_code}")
            print(f"   Ответ: {response.text}")
    except Exception as e:
        print(f"❌ Ошибка при отправке команды: {e}")

if __name__ == "__main__":
    print("🚀 Запуск теста работы с русским текстом")
    print("=" * 50)
    
    # Проверяем, что сервер запущен
    try:
        response = requests.get(f"{SERVER_URL}/health")
        if response.status_code == 200:
            print("✅ Сервер доступен")
        else:
            print("❌ Сервер недоступен")
            sys.exit(1)
    except Exception as e:
        print(f"❌ Не удается подключиться к серверу: {e}")
        print("Убедитесь, что сервер запущен на http://localhost:8000")
        sys.exit(1)
    
    # Запускаем тесты
    test_russian_text_commands()
    test_simple_hello_world()
    
    print("\n" + "=" * 50)
    print("🎉 Тестирование завершено!")
    print("\n💡 Для проверки работы агента:")
    print("   1. Убедитесь, что агент запущен и слушает на порту 8081")
    print("   2. Проверьте логи агента на предмет ошибок кодировки")
    print("   3. Проверьте, что PowerShell поддерживает UTF-8")
