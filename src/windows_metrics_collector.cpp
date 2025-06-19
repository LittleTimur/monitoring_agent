/**
 * @file windows_metrics_collector.cpp
 * @brief Реализация сбора системных метрик для Windows
 * 
 * Этот файл содержит реализацию класса WindowsMetricsCollector, который собирает
 * различные системные метрики в Windows через Windows API, PDH (Performance Data Helper)
 * и WMI (Windows Management Instrumentation). Метрики включают CPU, память, диски,
 * сеть, GPU и HDD.
 */

#include "../include/windows_metrics_collector.hpp"
#include <windows.h>
#include <pdh.h>
#include <psapi.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <wininet.h>
#include <iptypes.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <thread>
#include <iostream>
#include <map>
#include <comdef.h>
#include <Wbemidl.h>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wbemuuid.lib")

namespace monitoring {

/**
 * @class WindowsMetricsCollector
 * @brief Класс для сбора системных метрик в Windows
 * 
 * Реализует интерфейс MetricsCollector для сбора различных системных метрик
 * в Windows через Windows API, PDH и WMI. Использует различные системные
 * интерфейсы для получения точных метрик производительности.
 */

WindowsMetricsCollector::WindowsMetricsCollector() : is_initialized(true) {
    // Получаем количество процессоров
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    num_processors = sysInfo.dwNumberOfProcessors;
    std::cerr << "Number of processors: " << num_processors << std::endl;
    
    // Инициализируем предыдущие значения времени
    last_idle_time = 0;
    last_kernel_time = 0;
    last_user_time = 0;
    last_collection_time = std::chrono::steady_clock::now();
    
    // Получаем начальные значения
    FILETIME idle_time, kernel_time, user_time;
    if (GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        last_idle_time = ((uint64_t)idle_time.dwHighDateTime << 32) | idle_time.dwLowDateTime;
        last_kernel_time = ((uint64_t)kernel_time.dwHighDateTime << 32) | kernel_time.dwLowDateTime;
        last_user_time = ((uint64_t)user_time.dwHighDateTime << 32) | user_time.dwLowDateTime;
    }
}

WindowsMetricsCollector::~WindowsMetricsCollector() {
    // Ничего не нужно освобождать
}

SystemMetrics WindowsMetricsCollector::collect() {
    SystemMetrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();
    
    metrics.cpu = collect_cpu_metrics();
    metrics.memory = collect_memory_metrics();
    metrics.disk = collect_disk_metrics();
    metrics.network = collect_network_metrics();
    metrics.gpu = collect_gpu_metrics();
    metrics.hdd = collect_hdd_metrics();
    
    return metrics;
}

CpuMetrics WindowsMetricsCollector::collect_cpu_metrics() {
    CpuMetrics metrics;
    metrics.usage_percent = 0.0;
    metrics.temperature = 0.0;
    metrics.core_temperatures.resize(num_processors, 0.0);
    metrics.core_usage.resize(num_processors, 0.0);
    
    if (!is_initialized) {
        std::cerr << "Collector not initialized" << std::endl;
        return metrics;
    }

    // Получаем текущие значения времени
    FILETIME idle_time, kernel_time, user_time;
    if (!GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        std::cerr << "Failed to get system times. Error: " << GetLastError() << std::endl;
        return metrics;
    }

    // Конвертируем FILETIME в uint64_t
    uint64_t current_idle_time = ((uint64_t)idle_time.dwHighDateTime << 32) | idle_time.dwLowDateTime;
    uint64_t current_kernel_time = ((uint64_t)kernel_time.dwHighDateTime << 32) | kernel_time.dwLowDateTime;
    uint64_t current_user_time = ((uint64_t)user_time.dwHighDateTime << 32) | user_time.dwLowDateTime;

    // Вычисляем разницу во времени
    uint64_t idle_time_diff = current_idle_time - last_idle_time;
    uint64_t kernel_time_diff = current_kernel_time - last_kernel_time;
    uint64_t user_time_diff = current_user_time - last_user_time;

    // Вычисляем общее время процессора
    uint64_t total_time_diff = kernel_time_diff + user_time_diff;
    
    if (total_time_diff > 0) {
        // Вычисляем процент использования CPU
        double idle_percent = (double)idle_time_diff / total_time_diff * 100.0;
        metrics.usage_percent = 100.0 - idle_percent;
        
        // Для каждого ядра используем то же значение (GetSystemTimes не предоставляет информацию по ядрам)
        for (size_t i = 0; i < num_processors; ++i) {
            metrics.core_usage[i] = metrics.usage_percent;
        }
        
        std::cerr << "CPU usage: " << metrics.usage_percent << "%" << std::endl;
    }

    // Обновляем предыдущие значения
    last_idle_time = current_idle_time;
    last_kernel_time = current_kernel_time;
    last_user_time = current_user_time;
    last_collection_time = std::chrono::steady_clock::now();

    return metrics;
}

MemoryMetrics WindowsMetricsCollector::collect_memory_metrics() {
    MemoryMetrics metrics;
    
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        metrics.total_bytes = memInfo.ullTotalPhys;
        metrics.free_bytes = memInfo.ullAvailPhys;
        metrics.used_bytes = metrics.total_bytes - metrics.free_bytes;
        metrics.usage_percent = static_cast<double>(memInfo.dwMemoryLoad);
    } else {
        metrics.total_bytes = 0;
        metrics.free_bytes = 0;
        metrics.used_bytes = 0;
        metrics.usage_percent = 0.0;
    }
    
    return metrics;
}

DiskMetrics WindowsMetricsCollector::collect_disk_metrics() {
    DiskMetrics metrics;
    
    // Получаем список дисков
    DWORD drives = GetLogicalDrives();
    char driveLetter = 'A';
    
    while (drives) {
        if (drives & 1) {
            std::string rootPath = std::string(1, driveLetter) + ":\\";
            UINT driveType = GetDriveTypeA(rootPath.c_str());
            
            if (driveType == DRIVE_FIXED) {
                ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
                if (GetDiskFreeSpaceExA(rootPath.c_str(), &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
                    DiskPartition partition;
                    partition.mount_point = rootPath;
                    partition.filesystem = "NTFS"; // TODO: Получить реальный тип файловой системы
                    partition.total_bytes = totalBytes.QuadPart;
                    partition.free_bytes = freeBytesAvailable.QuadPart;
                    partition.used_bytes = totalBytes.QuadPart - freeBytesAvailable.QuadPart;
                    partition.usage_percent = (partition.used_bytes * 100.0) / partition.total_bytes;
                    
                    metrics.partitions.push_back(partition);
                }
            }
        }
        drives >>= 1;
        driveLetter++;
    }
    
    return metrics;
}

NetworkMetrics WindowsMetricsCollector::collect_network_metrics() {
    NetworkMetrics metrics;
    
    ULONG bufferSize = 0;
    // Первый вызов — узнаём размер буфера
    if (GetAdaptersInfo(nullptr, &bufferSize) != ERROR_BUFFER_OVERFLOW) {
        std::cerr << "Failed to get network adapters buffer size" << std::endl;
        return metrics;
    }
    PIP_ADAPTER_INFO pAdapterInfo = (PIP_ADAPTER_INFO)malloc(bufferSize);
    if (pAdapterInfo == nullptr) {
        std::cerr << "Failed to allocate memory for network adapters" << std::endl;
        return metrics;
    }
    if (GetAdaptersInfo(pAdapterInfo, &bufferSize) == NO_ERROR) {
        PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
        while (pAdapter) {
            // Фильтруем только активные физические адаптеры
            if (pAdapter->Type == MIB_IF_TYPE_ETHERNET || 
                pAdapter->Type == MIB_IF_TYPE_PPP ||
                pAdapter->Type == 71) { // IF_TYPE_IEEE80211 = 71
                // Получаем статистику интерфейса через MIB API
                MIB_IFROW ifRow;
                memset(&ifRow, 0, sizeof(ifRow));
                ifRow.dwIndex = pAdapter->Index;
                if (GetIfEntry(&ifRow) == NO_ERROR) {
                    if (ifRow.dwOperStatus == IF_OPER_STATUS_OPERATIONAL) {
                        NetworkInterface netif;
                        netif.name = std::string(pAdapter->Description);
                        netif.bytes_sent = ifRow.dwOutOctets;
                        netif.bytes_received = ifRow.dwInOctets;
                        netif.packets_sent = ifRow.dwOutUcastPkts + ifRow.dwOutNUcastPkts;
                        netif.packets_received = ifRow.dwInUcastPkts + ifRow.dwInNUcastPkts;
                        static std::map<DWORD, uint64_t> lastBytesSent;
                        static std::map<DWORD, uint64_t> lastBytesReceived;
                        static std::map<DWORD, std::chrono::steady_clock::time_point> lastTime;
                        auto now = std::chrono::steady_clock::now();
                        if (lastTime.find(ifRow.dwIndex) != lastTime.end()) {
                            auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime[ifRow.dwIndex]).count();
                            if (timeDiff > 0) {
                                uint64_t bytesSentDiff = netif.bytes_sent - lastBytesSent[ifRow.dwIndex];
                                uint64_t bytesReceivedDiff = netif.bytes_received - lastBytesReceived[ifRow.dwIndex];
                                netif.bandwidth_sent = (bytesSentDiff * 1000) / timeDiff;
                                netif.bandwidth_received = (bytesReceivedDiff * 1000) / timeDiff;
                            } else {
                                netif.bandwidth_sent = 0;
                                netif.bandwidth_received = 0;
                            }
                        } else {
                            netif.bandwidth_sent = 0;
                            netif.bandwidth_received = 0;
                        }
                        lastBytesSent[ifRow.dwIndex] = netif.bytes_sent;
                        lastBytesReceived[ifRow.dwIndex] = netif.bytes_received;
                        lastTime[ifRow.dwIndex] = now;
                        if (netif.bytes_sent > 0 || netif.bytes_received > 0 || 
                            netif.bandwidth_sent > 0 || netif.bandwidth_received > 0) {
                            metrics.interfaces.push_back(netif);
                        }
                    }
                }
            }
            pAdapter = pAdapter->Next;
        }
    } else {
        std::cerr << "Failed to get network adapters info" << std::endl;
    }
    free(pAdapterInfo);
    return metrics;
}

GpuMetrics WindowsMetricsCollector::collect_gpu_metrics() {
    GpuMetrics metrics;
    metrics.temperature = 0.0;
    metrics.usage_percent = 0.0;
    metrics.memory_used = 0;
    metrics.memory_total = 0;

    // Получаем информацию о GPU через WMI
    // TODO: Реализовать сбор метрик GPU через WMI или NVML
    // Пока возвращаем нулевые значения
    
    return metrics;
}

HddMetrics WindowsMetricsCollector::collect_hdd_metrics() {
    HddMetrics metrics;
    HRESULT hres;

    // 1. Инициализация COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        std::cerr << "Failed to initialize COM library. Error code = 0x" << std::hex << hres << std::endl;
        return metrics;
    }

    // 2. Установить общие уровни безопасности
    hres = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL);
    if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
        std::cerr << "Failed to initialize security. Error code = 0x" << std::hex << hres << std::endl;
        CoUninitialize();
        return metrics;
    }

    // 3. Получить указатель на IWbemLocator
    IWbemLocator *pLoc = NULL;
    hres = CoCreateInstance(
        CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID *)&pLoc);
    if (FAILED(hres)) {
        std::cerr << "Failed to create IWbemLocator object. Error code = 0x" << std::hex << hres << std::endl;
        CoUninitialize();
        return metrics;
    }

    // 4. Подключиться к WMI namespace
    IWbemServices *pSvc = NULL;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) {
        std::cerr << "Could not connect to WMI. Error code = 0x" << std::hex << hres << std::endl;
        pLoc->Release();
        CoUninitialize();
        return metrics;
    }

    // 5. Установить прокси-безопасность
    hres = CoSetProxyBlanket(
        pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE);
    if (FAILED(hres)) {
        std::cerr << "Could not set proxy blanket. Error code = 0x" << std::hex << hres << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return metrics;
    }

    // 6. Выполнить WMI-запрос к Win32_DiskDrive
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT Model, SerialNumber, Status FROM Win32_DiskDrive"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator);
    if (FAILED(hres)) {
        std::cerr << "WMI query failed. Error code = 0x" << std::hex << hres << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return metrics;
    }

    // 7. Обработка результатов
    IWbemClassObject *pclsObj = NULL;
    ULONG uReturn = 0;
    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) break;
        HddDrive drive;
        // Model
        VARIANT vtProp;
        hr = pclsObj->Get(L"Model", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
            drive.name = _bstr_t(vtProp.bstrVal);
        } else {
            drive.name = "Unknown";
        }
        VariantClear(&vtProp);
        // SerialNumber
        hr = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
            drive.health_status = _bstr_t(vtProp.bstrVal);
        } else {
            drive.health_status = "Unknown";
        }
        VariantClear(&vtProp);
        // Status
        hr = pclsObj->Get(L"Status", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
            drive.health_status += " (";
            drive.health_status += _bstr_t(vtProp.bstrVal);
            drive.health_status += ")";
        }
        VariantClear(&vtProp);
        // Температура и PowerOnHours через WMI/SMART — не всегда доступны
        drive.temperature = 0.0;
        drive.power_on_hours = 0;
        metrics.drives.push_back(drive);
        pclsObj->Release();
    }
    // 8. Очистка
    if (pEnumerator) pEnumerator->Release();
    if (pSvc) pSvc->Release();
    if (pLoc) pLoc->Release();
    CoUninitialize();
    return metrics;
}

} // namespace monitoring  