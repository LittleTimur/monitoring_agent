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
#include <cstdio>
#include <memory>
#include <array>
#include <pdhmsg.h>
#include <regex>
#include <fstream>
#include <locale.h>

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
    // Установим и выведем текущую локаль
    std::cout << "Current locale: " << setlocale(LC_ALL, NULL) << std::endl;
    setlocale(LC_ALL, "Russian");
    std::cout << "Locale after setlocale: " << setlocale(LC_ALL, NULL) << std::endl;
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

    // PDH: инициализация счетчиков по ядрам
    if (PdhOpenQuery(NULL, 0, &cpu_query) == ERROR_SUCCESS) {
        for (DWORD i = 0; i < num_processors; ++i) {
            // 1. Пробуем английский вариант
            std::wstring counter_path_en = L"\\Processor(" + std::to_wstring(i) + L")\\% Processor Time";
            std::wcerr << L"Trying to add EN counter: [" << counter_path_en << L"]" << std::endl;
            PDH_HCOUNTER counter;
            if (PdhAddCounterW(cpu_query, counter_path_en.c_str(), 0, &counter) == ERROR_SUCCESS) {
                std::wcerr << L"Successfully added EN counter for core " << i << L"!" << std::endl;
                core_counters.push_back(counter);
                continue;
            }
            // 2. Пробуем русский вариант через MultiByteToWideChar
            char counter_path_ru_narrow[128];
            snprintf(counter_path_ru_narrow, sizeof(counter_path_ru_narrow), "\\Процессор(%lu)\\%% загруженности процессора", i);
            wchar_t counter_path_ru[256];
            int wlen = MultiByteToWideChar(CP_UTF8, 0, counter_path_ru_narrow, -1, counter_path_ru, 256);
            if (wlen > 0) {
                std::wcerr << L"Trying to add RU counter: [" << counter_path_ru << L"]" << std::endl;
                if (PdhAddCounterW(cpu_query, counter_path_ru, 0, &counter) == ERROR_SUCCESS) {
                    std::wcerr << L"Successfully added RU counter for core " << i << L"!" << std::endl;
                    core_counters.push_back(counter);
                    continue;
                } else {
                    std::cerr << "Failed to add RU CPU counter for core " << i << ". Error code: " << GetLastError() << std::endl;
                }
            } else {
                std::cerr << "MultiByteToWideChar failed for core " << i << "!" << std::endl;
            }
            // Если оба не сработали
            std::cerr << "Failed to add CPU counter for core " << i << " (EN and RU)." << std::endl;
            core_counters.push_back(nullptr);
        }
        // Первый сбор данных
        PdhCollectQueryData(cpu_query);
        // Короткая задержка
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        // Второй сбор данных
        PdhCollectQueryData(cpu_query);
    }
}

WindowsMetricsCollector::~WindowsMetricsCollector() {
    if (cpu_query) {
        PdhCloseQuery(cpu_query);
        cpu_query = nullptr;
    }
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

double get_cpu_temperature_wmi() {
    HRESULT hres;
    double max_temperature = 0.0;
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) return 0.0;
    hres = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL);
    if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
        CoUninitialize();
        return 0.0;
    }
    IWbemLocator *pLoc = NULL;
    hres = CoCreateInstance(
        CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID *)&pLoc);
    if (FAILED(hres)) {
        CoUninitialize();
        return 0.0;
    }
    IWbemServices *pSvc = NULL;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\WMI"),
        NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) {
        pLoc->Release();
        CoUninitialize();
        return 0.0;
    }
    hres = CoSetProxyBlanket(
        pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE);
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return 0.0;
    }
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM MSAcpi_ThermalZoneTemperature WHERE Active=TRUE"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator);
    if (SUCCEEDED(hres)) {
        IWbemClassObject *pObj = NULL;
        ULONG uReturn = 0;
        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn);
            if (0 == uReturn) break;
            VARIANT vtProp;
            hr = pObj->Get(L"CurrentTemperature", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && (vtProp.vt == VT_UINT || vtProp.vt == VT_I4)) {
                double temp = (vtProp.vt == VT_UINT) ? vtProp.uintVal : vtProp.lVal;
                double celsius = (temp / 10.0) - 273.15;
                if (celsius > max_temperature) max_temperature = celsius;
            }
            VariantClear(&vtProp);
            pObj->Release();
        }
        pEnumerator->Release();
    }
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
    return max_temperature;
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
        std::cerr << "CPU usage: " << metrics.usage_percent << "%" << std::endl;
    }

    // Сбор загрузки по каждому ядру через PDH
    if (cpu_query && !core_counters.empty()) {
        PdhCollectQueryData(cpu_query);
        for (size_t i = 0; i < core_counters.size(); ++i) {
            if (core_counters[i]) {
                PDH_FMT_COUNTERVALUE counterVal;
                PDH_STATUS status = PdhGetFormattedCounterValue(core_counters[i], PDH_FMT_DOUBLE, NULL, &counterVal);
                if (status == ERROR_SUCCESS) {
                    metrics.core_usage[i] = counterVal.doubleValue;
                } else {
                    std::cerr << "PDH error for core " << i << ": " << status << std::endl;
                    metrics.core_usage[i] = NAN;
                }
            } else {
                metrics.core_usage[i] = NAN;
            }
        }
    } else {
        // fallback: для каждого ядра используем общее значение
        for (size_t i = 0; i < num_processors; ++i) {
            metrics.core_usage[i] = metrics.usage_percent;
        }
    }

    // Получение температуры CPU только через WMI
    double wmi_temp = get_cpu_temperature_wmi();
    if (wmi_temp > 0.0) {
        metrics.temperature = wmi_temp;
    }

    // Обновляем предыдущие значения
    last_idle_time = current_idle_time;
    last_kernel_time = current_kernel_time;
    last_user_time = current_user_time;
    last_collection_time = std::chrono::steady_clock::now();

    // Температура по ядрам CPU на Windows невозможна без сторонних утилит (LibreHardwareMonitor, HWiNFO и др.)
    // metrics.core_temperatures всегда 0.0
    //
    // В collect_cpu_metrics:
    // Температура по ядрам CPU на Windows невозможна без сторонних утилит (LibreHardwareMonitor, HWiNFO и др.)
    // metrics.core_temperatures всегда 0.0
    //
    // В collect_gpu_metrics:
    // Для AMD/Intel GPU без сторонних утилит сбор метрик невозможен, возвращаем usage_percent = -1.0
    //
    // В collect_hdd_metrics:
    // Используется только smartctl, старый WMI-код удалён (или закомментирован как fallback)
    //
    // В collect_network_metrics:
    // Bandwidth считается через два замера с задержкой, корректно работает для stateless-агента
    //
    // Для всех метрик добавлены комментарии о поддержке и ограничениях
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
    // Первый замер
    std::map<DWORD, std::pair<uint64_t, uint64_t>> stats0;
    ULONG bufferSize = 0;
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
            if (pAdapter->Type == MIB_IF_TYPE_ETHERNET || 
                pAdapter->Type == MIB_IF_TYPE_PPP ||
                pAdapter->Type == 71) { // IF_TYPE_IEEE80211 = 71
                MIB_IFROW ifRow;
                memset(&ifRow, 0, sizeof(ifRow));
                ifRow.dwIndex = pAdapter->Index;
                if (GetIfEntry(&ifRow) == NO_ERROR) {
                    if (ifRow.dwOperStatus == IF_OPER_STATUS_OPERATIONAL) {
                        stats0[ifRow.dwIndex] = {ifRow.dwOutOctets, ifRow.dwInOctets};
                    }
                }
            }
            pAdapter = pAdapter->Next;
        }
    }
    free(pAdapterInfo);
    // Задержка 1 секунда
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // Второй замер
    std::map<DWORD, std::pair<uint64_t, uint64_t>> stats1;
    bufferSize = 0;
    if (GetAdaptersInfo(nullptr, &bufferSize) != ERROR_BUFFER_OVERFLOW) {
        std::cerr << "Failed to get network adapters buffer size (2)" << std::endl;
        return metrics;
    }
    pAdapterInfo = (PIP_ADAPTER_INFO)malloc(bufferSize);
    if (pAdapterInfo == nullptr) {
        std::cerr << "Failed to allocate memory for network adapters (2)" << std::endl;
        return metrics;
    }
    if (GetAdaptersInfo(pAdapterInfo, &bufferSize) == NO_ERROR) {
        PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
        while (pAdapter) {
            if (pAdapter->Type == MIB_IF_TYPE_ETHERNET || 
                pAdapter->Type == MIB_IF_TYPE_PPP ||
                pAdapter->Type == 71) {
                MIB_IFROW ifRow;
                memset(&ifRow, 0, sizeof(ifRow));
                ifRow.dwIndex = pAdapter->Index;
                if (GetIfEntry(&ifRow) == NO_ERROR) {
                    if (ifRow.dwOperStatus == IF_OPER_STATUS_OPERATIONAL) {
                        stats1[ifRow.dwIndex] = {ifRow.dwOutOctets, ifRow.dwInOctets};
                    }
                }
            }
            pAdapter = pAdapter->Next;
        }
    }
    free(pAdapterInfo);
    // Формируем метрики
    for (const auto& [idx, val0] : stats0) {
        NetworkInterface netif;
        // Имя интерфейса
        ULONG bufferSizeName = sizeof(MIB_IFROW);
        MIB_IFROW ifRow;
        memset(&ifRow, 0, sizeof(ifRow));
        ifRow.dwIndex = idx;
        if (GetIfEntry(&ifRow) == NO_ERROR) {
            netif.name = std::string(reinterpret_cast<char*>(ifRow.bDescr), ifRow.dwDescrLen);
        } else {
            netif.name = "Unknown";
        }
        netif.bytes_sent = val0.first;
        netif.bytes_received = val0.second;
        netif.packets_sent = 0; // Можно доработать при необходимости
        netif.packets_received = 0;
        if (stats1.count(idx)) {
            netif.bandwidth_sent = stats1[idx].first > val0.first ? stats1[idx].first - val0.first : 0;
            netif.bandwidth_received = stats1[idx].second > val0.second ? stats1[idx].second - val0.second : 0;
        } else {
            netif.bandwidth_sent = 0;
            netif.bandwidth_received = 0;
        }
        metrics.interfaces.push_back(netif);
    }
    return metrics;
}

GpuMetrics WindowsMetricsCollector::collect_gpu_metrics() {
    GpuMetrics metrics;
    metrics.temperature = 0.0;
    metrics.usage_percent = -1.0; // По умолчанию - не поддерживается
    metrics.memory_used = 0;
    metrics.memory_total = 0;

    // 1. NVIDIA: nvidia-smi
    {
        std::array<char, 256> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(
            "nvidia-smi --query-gpu=temperature.gpu,utilization.gpu,memory.used,memory.total --format=csv,noheader,nounits 2>&1", "r"), _pclose);
        if (pipe && fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result = buffer.data();
            if (result.find("not recognized") == std::string::npos &&
                result.find("command not found") == std::string::npos &&
                result.find("No devices were found") == std::string::npos &&
                result.find("NVIDIA-SMI has failed") == std::string::npos) {
                std::istringstream iss(result);
                double temp = 0, usage = 0, mem_used = 0, mem_total = 0;
                char comma;
                iss >> temp >> comma >> usage >> comma >> mem_used >> comma >> mem_total;
                metrics.temperature = temp;
                metrics.usage_percent = usage;
                metrics.memory_used = static_cast<uint64_t>(mem_used) * 1024 * 1024; // MB -> bytes
                metrics.memory_total = static_cast<uint64_t>(mem_total) * 1024 * 1024; // MB -> bytes
                return metrics;
            }
        }
    }
    // Для AMD/Intel GPU без сторонних утилит сбор метрик невозможен
    // metrics.usage_percent = -1.0; // уже по умолчанию
    // Остальные поля остаются по умолчанию
    return metrics;
}

HddMetrics WindowsMetricsCollector::collect_hdd_metrics() {
    HddMetrics metrics;
    // Требуется установленный smartmontools и права администратора!
    // Получаем список устройств через smartctl --scan
    std::vector<std::pair<std::string, std::string>> devices;
    FILE* pipe = _popen("smartctl --scan", "r");
    if (pipe) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            std::string line(buffer);
            std::smatch match;
            std::regex re(R"((/dev/\S+) -d (\S+))");
            if (std::regex_search(line, match, re)) {
                devices.emplace_back(match[1], match[2]);
            }
        }
        _pclose(pipe);
    }
    for (const auto& [dev, dtype] : devices) {
        HddDrive drive;
        drive.name = dev;
        drive.temperature = 0.0;
        drive.power_on_hours = 0;
        drive.health_status = "Unknown";
        // smartctl -A -d ...
        std::string cmd = "smartctl -A -d " + dtype + " " + dev + " 2>&1";
        FILE* pipeA = _popen(cmd.c_str(), "r");
        if (pipeA) {
            char buffer[512];
            std::string output;
            while (fgets(buffer, sizeof(buffer), pipeA)) output += buffer;
            _pclose(pipeA);
            std::istringstream iss(output);
            std::string line;
            while (std::getline(iss, line)) {
                // ATA/SATA: Temperature_Celsius или Temperature
                if (line.find("Temperature_Celsius") != std::string::npos ||
                    (line.find("Temperature") != std::string::npos && line.find("Celsius") == std::string::npos)) {
                    std::istringstream lss(line);
                    std::string tmp;
                    int temp = 0;
                    while (lss >> tmp) {
                        try { temp = std::stoi(tmp); } catch (...) {}
                    }
                    if (temp > 0 && temp < 100) drive.temperature = temp;
                }
                // NVMe: Temperature: ... Celsius
                if (line.find("Temperature:") != std::string::npos && line.find("Celsius") != std::string::npos) {
                    std::smatch match;
                    std::regex re(R"(Temperature:\s+(\d+) Celsius)");
                    if (std::regex_search(line, match, re)) {
                        drive.temperature = std::stoi(match[1]);
                    }
                }
                // NVMe: Temperature Sensor N: ... Celsius
                if (line.find("Temperature Sensor") != std::string::npos && line.find("Celsius") != std::string::npos) {
                    std::smatch match;
                    std::regex re(R"(Temperature Sensor \d+:\s+(\d+) Celsius)");
                    if (std::regex_search(line, match, re)) {
                        // Можно добавить в массив температур по сенсорам, если нужно
                        drive.temperature = std::stoi(match[1]);
                    }
                }
                // ATA/SATA: Power_On_Hours
                if (line.find("Power_On_Hours") != std::string::npos) {
                    std::istringstream lss(line);
                    std::string tmp;
                    int hours = 0;
                    while (lss >> tmp) {
                        try { hours = std::stoi(tmp); } catch (...) {}
                    }
                    if (hours > 0) drive.power_on_hours = hours;
                }
                // NVMe: Power On Hours
                if (line.find("Power On Hours:") != std::string::npos) {
                    std::smatch match;
                    std::regex re(R"(Power On Hours:\s+(\d+))");
                    if (std::regex_search(line, match, re)) {
                        drive.power_on_hours = std::stoi(match[1]);
                    }
                }
            }
        }
        // smartctl -H -d ...
        cmd = "smartctl -H -d " + dtype + " " + dev + " 2>&1";
        FILE* pipeH = _popen(cmd.c_str(), "r");
        if (pipeH) {
            char buffer[256];
            std::string output;
            while (fgets(buffer, sizeof(buffer), pipeH)) output += buffer;
            _pclose(pipeH);
            if (output.find("PASSED") != std::string::npos)
                drive.health_status = "PASSED";
            else if (output.find("FAILED") != std::string::npos)
                drive.health_status = "FAILED";
            else if (output.find("Warning") != std::string::npos)
                drive.health_status = "Warning";
        }
        metrics.drives.push_back(drive);
    }
    // // Fallback: WMI (оставить закомментированным)
    // ... (старый WMI-код) ...
    return metrics;
}

} // namespace monitoring  