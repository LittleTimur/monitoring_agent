from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import time
import threading
import queue
import json
from datetime import datetime

PORT = 8080  # Укажите нужный порт
ALERT_TIMEOUT = 15 * 60  # 15 минут в секундах

data_queue = queue.Queue()
last_received_time = [time.time()]  # Используем список для мутабельности между потоками

def log_to_file(message):
    with open("metrics.log", "a", encoding="utf-8") as f:
        f.write(message + "\n")

def format_bytes(bytes_value):
    """Преобразует байты в человекочитаемый формат"""
    if bytes_value is None:
        return "N/A"
    for unit in ['B', 'KB', 'MB', 'GB', 'TB']:
        if bytes_value < 1024.0:
            return f"{bytes_value:.2f} {unit}"
        bytes_value /= 1024.0
    return f"{bytes_value:.2f} PB"

def format_temperature(temp):
    """Форматирует температуру"""
    if temp is None:
        return "N/A"
    return f"{temp:.1f}°C"

def format_percent(value):
    """Форматирует процентное значение"""
    if value is None:
        return "N/A"
    return f"{value:.1f}%"

def format_timestamp(timestamp):
    """Преобразует timestamp в читаемую дату"""
    try:
        dt = datetime.fromtimestamp(timestamp)
        return dt.strftime("%Y-%m-%d %H:%M:%S")
    except:
        return "N/A"

def json_to_human_readable(json_data):
    """Преобразует JSON метрики в человекочитаемый вид"""
    try:
        data = json.loads(json_data)
    except json.JSONDecodeError:
        return "Ошибка парсинга JSON"
    
    output = []
    output.append("=" * 80)
    output.append("ОТЧЕТ О СИСТЕМНЫХ МЕТРИКАХ")
    output.append("=" * 80)
    
    # Временная метка
    timestamp = format_timestamp(data.get('timestamp', 0))
    output.append(f"Время: {timestamp}")
    output.append(f"Тип машины: {data.get('machine_type', 'N/A')}")
    output.append("")
    
    # CPU
    cpu = data.get('cpu', {})
    output.append("ПРОЦЕССОР:")
    output.append(f"  Загрузка: {format_percent(cpu.get('usage_percent'))}")
    output.append(f"  Температура: {format_temperature(cpu.get('temperature'))}")
    
    # Температуры ядер
    core_temps = cpu.get('core_temperatures', [])
    if core_temps:
        output.append("  Температуры ядер:")
        for i, temp in enumerate(core_temps):
            output.append(f"    Ядро {i+1}: {format_temperature(temp)}")
    
    # Загрузка ядер
    core_usage = cpu.get('core_usage', [])
    if core_usage:
        output.append("  Загрузка ядер:")
        for i, usage in enumerate(core_usage):
            output.append(f"    Ядро {i+1}: {format_percent(usage)}")
    output.append("")
    
    # Память
    memory = data.get('memory', {})
    output.append("ПАМЯТЬ:")
    output.append(f"  Всего: {format_bytes(memory.get('total_bytes'))}")
    output.append(f"  Использовано: {format_bytes(memory.get('used_bytes'))}")
    output.append(f"  Свободно: {format_bytes(memory.get('free_bytes'))}")
    output.append(f"  Загрузка: {format_percent(memory.get('usage_percent'))}")
    output.append("")
    
    # Диски
    disk = data.get('disk', {})
    partitions = disk.get('partitions', [])
    if partitions:
        output.append("ДИСКИ:")
        for i, part in enumerate(partitions):
            output.append(f"  Раздел {i+1}:")
            output.append(f"    Точка монтирования: {part.get('mount_point', 'N/A')}")
            output.append(f"    Файловая система: {part.get('filesystem', 'N/A')}")
            output.append(f"    Всего: {format_bytes(part.get('total_bytes'))}")
            output.append(f"    Использовано: {format_bytes(part.get('used_bytes'))}")
            output.append(f"    Свободно: {format_bytes(part.get('free_bytes'))}")
            output.append(f"    Загрузка: {format_percent(part.get('usage_percent'))}")
        output.append("")
    
    # Сеть
    network = data.get('network', {})
    interfaces = network.get('interfaces', [])
    if interfaces:
        output.append("СЕТЕВЫЕ ИНТЕРФЕЙСЫ:")
        for iface in interfaces:
            output.append(f"  Интерфейс: {iface.get('name', 'N/A')}")
            output.append(f"    Отправлено: {format_bytes(iface.get('bytes_sent'))}")
            output.append(f"    Получено: {format_bytes(iface.get('bytes_received'))}")
            output.append(f"    Пакетов отправлено: {iface.get('packets_sent', 'N/A')}")
            output.append(f"    Пакетов получено: {iface.get('packets_received', 'N/A')}")
            output.append(f"    Скорость отправки: {format_bytes(iface.get('bandwidth_sent'))}/s")
            output.append(f"    Скорость получения: {format_bytes(iface.get('bandwidth_received'))}/s")
        output.append("")
    
    # Сетевые соединения
    connections = network.get('connections', [])
    if connections:
        output.append("СЕТЕВЫЕ СОЕДИНЕНИЯ:")
        for conn in connections:
            output.append(f"  {conn.get('local_ip', 'N/A')}:{conn.get('local_port', 'N/A')} -> {conn.get('remote_ip', 'N/A')}:{conn.get('remote_port', 'N/A')} ({conn.get('protocol', 'N/A')})")
        output.append("")
    
    # GPU
    gpu = data.get('gpu', {})
    if gpu.get('temperature') is not None or gpu.get('usage_percent') is not None:
        output.append("ГРАФИЧЕСКИЙ ПРОЦЕССОР:")
        output.append(f"  Температура: {format_temperature(gpu.get('temperature'))}")
        output.append(f"  Загрузка: {format_percent(gpu.get('usage_percent'))}")
        output.append(f"  Память использовано: {format_bytes(gpu.get('memory_used'))}")
        output.append(f"  Память всего: {format_bytes(gpu.get('memory_total'))}")
        output.append("")
    
    # HDD
    hdd = data.get('hdd', {})
    drives = hdd.get('drives', [])
    if drives:
        output.append("ЖЕСТКИЕ ДИСКИ:")
        for drive in drives:
            output.append(f"  Диск: {drive.get('name', 'N/A')}")
            output.append(f"    Температура: {format_temperature(drive.get('temperature'))}")
            output.append(f"    Время работы: {drive.get('power_on_hours', 'N/A')} часов")
            output.append(f"    Состояние: {drive.get('health_status', 'N/A')}")
        output.append("")
    
    # Инвентаризация
    inventory = data.get('inventory', {})
    if inventory:
        output.append("ИНВЕНТАРИЗАЦИЯ:")
        output.append(f"  Тип устройства: {inventory.get('device_type', 'N/A')}")
        output.append(f"  Производитель: {inventory.get('manufacturer', 'N/A')}")
        output.append(f"  Модель: {inventory.get('model', 'N/A')}")
        output.append(f"  Серийный номер: {inventory.get('serial_number', 'N/A')}")
        output.append(f"  UUID: {inventory.get('uuid', 'N/A')}")
        output.append(f"  ОС: {inventory.get('os_name', 'N/A')} {inventory.get('os_version', 'N/A')}")
        output.append(f"  Процессор: {inventory.get('cpu_model', 'N/A')} ({inventory.get('cpu_frequency', 'N/A')})")
        output.append(f"  Тип памяти: {inventory.get('memory_type', 'N/A')}")
        output.append(f"  Диск: {inventory.get('disk_model', 'N/A')} ({inventory.get('disk_type', 'N/A')}) - {format_bytes(inventory.get('disk_total_bytes'))}")
        output.append(f"  Видеокарта: {inventory.get('gpu_model', 'N/A')}")
        
        # MAC адреса
        mac_addresses = inventory.get('mac_addresses', [])
        if mac_addresses:
            output.append("  MAC адреса:")
            for mac in mac_addresses:
                output.append(f"    {mac}")
        
        # IP адреса
        ip_addresses = inventory.get('ip_addresses', [])
        if ip_addresses:
            output.append("  IP адреса:")
            for ip in ip_addresses:
                output.append(f"    {ip}")
        
        # Установленное ПО
        installed_software = inventory.get('installed_software', [])
        if installed_software:
            output.append("  Установленное ПО:")
            for software in installed_software:
                output.append(f"    {software}")
    
    output.append("=" * 80)
    output.append("")
    
    return "\n".join(output)

def log_human_readable_to_file(human_data):
    """Записывает человекочитаемые данные в отдельный файл"""
    with open("metrics_human_readable.txt", "a", encoding="utf-8") as f:
        f.write(human_data + "\n")

class MetricsHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path == '/metrics':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            data_queue.put(post_data.decode('utf-8'))
            last_received_time[0] = time.time()
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b'OK')
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        return  # Отключить стандартный лог

def file_writer():
    while True:
        data = data_queue.get()
        # Записываем исходный JSON
        log_to_file(f"Received metrics:\n{data}\n")
        # Преобразуем в человекочитаемый вид и записываем в отдельный файл
        human_data = json_to_human_readable(data)
        log_human_readable_to_file(human_data)
        data_queue.task_done()

def check_activity():
    alerted = False
    while True:
        time.sleep(10)
        if time.time() - last_received_time[0] > ALERT_TIMEOUT:
            if not alerted:
                log_to_file("АЛЕРТ: Нет данных от агентов более 15 минут!")
                print("АЛЕРТ: Нет данных от агентов более 15 минут!")
                alerted = True
        else:
            alerted = False

if __name__ == '__main__':
    # Запуск потока для записи в файл
    threading.Thread(target=file_writer, daemon=True).start()
    # Запуск потока для проверки активности
    threading.Thread(target=check_activity, daemon=True).start()

    server_address = ('', PORT)
    httpd = ThreadingHTTPServer(server_address, MetricsHandler)
    print(f"Listening on port {PORT}...")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        httpd.server_close()
        print("Server stopped")