#!/usr/bin/env python3
"""
Тестирование API для работы со скриптами
"""

import requests
import json
import time
from typing import Dict, Any, Optional

class ScriptAPITester:
    def __init__(self, base_url: str = "http://localhost:8000", agent_id: str = None):
        self.base_url = base_url
        self.agent_id = agent_id or self._get_first_agent_id()
        self.session = requests.Session()
        
    def _get_first_agent_id(self) -> str:
        """Получает ID первого доступного агента"""
        try:
            response = self.session.get(f"{self.base_url}/api/agents/")
            if response.status_code == 200:
                agents = response.json()
                if agents:
                    agent_id = agents[0]["agent_id"]
                    print(f"✅ Используем агента: {agent_id}")
                    return agent_id
                else:
                    raise Exception("Нет доступных агентов")
            else:
                raise Exception(f"Ошибка получения списка агентов: {response.status_code}")
        except Exception as e:
            print(f"❌ Ошибка получения ID агента: {e}")
            return "test_agent"
    
    def test_basic_script(self) -> bool:
        """Тест базового скрипта"""
        print("\n🔧 Тест 1: Базовый скрипт echo")
        
        data = {
            "script": "echo Привет!",
            "interpreter": "cmd"
        }
        
        return self._send_script_request(data, "Базовый скрипт")
    
    def test_powershell_script(self) -> bool:
        """Тест PowerShell скрипта"""
        print("\n🔧 Тест 2: PowerShell скрипт")
        
        data = {
            "script": "Write-Host \"Привет из PowerShell!\"",
            "interpreter": "powershell"
        }
        
        return self._send_script_request(data, "PowerShell скрипт")
    
    def test_python_script(self) -> bool:
        """Тест Python скрипта"""
        print("\n🔧 Тест 3: Python скрипт")
        
        data = {
            "script": "print('Привет из Python!')\nprint('Текущее время:', __import__('datetime').datetime.now())",
            "interpreter": "python"
        }
        
        return self._send_script_request(data, "Python скрипт")
    
    def test_script_with_args(self) -> bool:
        """Тест скрипта с параметрами"""
        print("\n🔧 Тест 4: Скрипт с параметрами")
        
        data = {
            "script": "echo Параметр 1: %1 && echo Параметр 2: %2",
            "interpreter": "cmd",
            "args": ["Hello", "World"]
        }
        
        return self._send_script_request(data, "Скрипт с параметрами")
    
    def test_script_with_timeout(self) -> bool:
        """Тест скрипта с таймаутом"""
        print("\n🔧 Тест 5: Скрипт с таймаутом")
        
        data = {
            "script": "ping -n 3 google.com",
            "interpreter": "cmd",
            "timeout_sec": 10
        }
        
        return self._send_script_request(data, "Скрипт с таймаутом")
    
    def test_system_info_script(self) -> bool:
        """Тест системной информации"""
        print("\n🔧 Тест 6: Системная информация")
        
        data = {
            "script": "systeminfo | findstr /B /C:\"OS Name\" /C:\"OS Version\"",
            "interpreter": "cmd",
            "capture_output": True
        }
        
        return self._send_script_request(data, "Системная информация")
    
    def test_disk_info_script(self) -> bool:
        """Тест информации о дисках"""
        print("\n🔧 Тест 7: Информация о дисках")
        
        data = {
            "script": "wmic logicaldisk get size,freespace,caption",
            "interpreter": "cmd",
            "capture_output": True
        }
        
        return self._send_script_request(data, "Информация о дисках")
    
    def test_network_script(self) -> bool:
        """Тест сетевой информации"""
        print("\n🔧 Тест 8: Сетевая информация")
        
        data = {
            "script": "ipconfig | findstr IPv4",
            "interpreter": "cmd",
            "capture_output": True
        }
        
        return self._send_script_request(data, "Сетевая информация")
    
    def test_background_script(self) -> bool:
        """Тест фонового скрипта"""
        print("\n🔧 Тест 9: Фоновый скрипт")
        
        data = {
            "script": "start /B notepad.exe",
            "interpreter": "cmd",
            "capture_output": False,
            "background": True
        }
        
        return self._send_script_request(data, "Фоновый скрипт")
    
    def test_error_script(self) -> bool:
        """Тест скрипта с ошибкой"""
        print("\n🔧 Тест 10: Скрипт с ошибкой")
        
        data = {
            "script": "nonexistent_command",
            "interpreter": "cmd"
        }
        
        return self._send_script_request(data, "Скрипт с ошибкой", expect_error=True)
    
    def _send_script_request(self, data: Dict[str, Any], test_name: str, expect_error: bool = False) -> bool:
        """Отправляет запрос на выполнение скрипта"""
        try:
            url = f"{self.base_url}/api/agents/{self.agent_id}/run_script"
            
            print(f"   📤 Отправка запроса: {test_name}")
            print(f"   📋 Данные: {json.dumps(data, indent=2, ensure_ascii=False)}")
            
            response = self.session.post(url, json=data, timeout=30)
            
            print(f"   📥 Статус ответа: {response.status_code}")
            
            if response.status_code == 200:
                result = response.json()
                print(f"   ✅ Ответ: {json.dumps(result, indent=2, ensure_ascii=False)}")
                
                if expect_error:
                    print(f"   ⚠️  Ожидалась ошибка, но получен успех")
                    return False
                
                return True
            else:
                print(f"   ❌ Ошибка HTTP: {response.status_code}")
                print(f"   📄 Текст ответа: {response.text}")
                
                if not expect_error:
                    print(f"   ⚠️  Неожиданная ошибка HTTP")
                    return False
                
                return True
                
        except requests.exceptions.Timeout:
            print(f"   ⏰ Таймаут запроса")
            return False
        except requests.exceptions.RequestException as e:
            print(f"   ❌ Ошибка запроса: {e}")
            return False
        except Exception as e:
            print(f"   ❌ Неожиданная ошибка: {e}")
            return False
    
    def test_all(self) -> None:
        """Запускает все тесты"""
        print(f"🚀 Запуск тестирования API скриптов")
        print(f"📍 Сервер: {self.base_url}")
        print(f"🆔 Агент: {self.agent_id}")
        
        tests = [
            self.test_basic_script,
            self.test_powershell_script,
            self.test_python_script,
            self.test_script_with_args,
            self.test_script_with_timeout,
            self.test_system_info_script,
            self.test_disk_info_script,
            self.test_network_script,
            self.test_background_script,
            self.test_error_script
        ]
        
        passed = 0
        total = len(tests)
        
        for test in tests:
            try:
                if test():
                    passed += 1
                time.sleep(1)  # Пауза между тестами
            except Exception as e:
                print(f"   ❌ Ошибка в тесте: {e}")
        
        print(f"\n📊 Результаты тестирования:")
        print(f"   ✅ Успешно: {passed}/{total}")
        print(f"   ❌ Провалено: {total - passed}/{total}")
        print(f"   📈 Процент успеха: {(passed/total)*100:.1f}%")

def main():
    """Главная функция"""
    import argparse
    
    parser = argparse.ArgumentParser(description="Тестирование API для работы со скриптами")
    parser.add_argument("--url", default="http://localhost:8000", help="URL сервера")
    parser.add_argument("--agent", help="ID агента для тестирования")
    
    args = parser.parse_args()
    
    tester = ScriptAPITester(args.url, args.agent)
    tester.test_all()

if __name__ == "__main__":
    main()

