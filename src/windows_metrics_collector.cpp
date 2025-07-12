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
#include <codecvt>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wbemuuid.lib")

namespace monitoring {

std::string detect_machine_type_windows();

/**
 * @class WindowsMetricsCollector
 * @brief Класс для сбора системных метрик в Windows
 * 
 * Реализует интерфейс MetricsCollector для сбора различных системных метрик
 * в Windows через Windows API, PDH и WMI. Использует различные системные
 * интерфейсы для получения точных метрик производительности.
 */

WindowsMetricsCollector::WindowsMetricsCollector() : is_initialized(true) {
    // Устанавливаем локаль
    setlocale(LC_ALL, "Russian");
    // Получаем количество процессоров
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    num_processors = sysInfo.dwNumberOfProcessors;
    
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
            PDH_HCOUNTER counter;
            if (PdhAddCounterW(cpu_query, counter_path_en.c_str(), 0, &counter) == ERROR_SUCCESS) {
                core_counters.push_back(counter);
                continue;
            }
            // 2. Пробуем русский вариант через MultiByteToWideChar
            char counter_path_ru_narrow[128];
            snprintf(counter_path_ru_narrow, sizeof(counter_path_ru_narrow), "\\Процессор(%lu)\\%% загруженности процессора", i);
            wchar_t counter_path_ru[256];
            int wlen = MultiByteToWideChar(CP_UTF8, 0, counter_path_ru_narrow, -1, counter_path_ru, 256);
            if (wlen > 0) {
                if (PdhAddCounterW(cpu_query, counter_path_ru, 0, &counter) == ERROR_SUCCESS) {
                    core_counters.push_back(counter);
                    continue;
                }
            }
            // Если оба не сработали, добавляем nullptr
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
    metrics.machine_type = detect_machine_type_windows();

    // --- Сбор инвентаризационных данных ---
    auto& inv = metrics.inventory;
    HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (SUCCEEDED(hres) || hres == RPC_E_CHANGED_MODE) {
        hres = CoInitializeSecurity(NULL, -1, NULL, NULL,
            RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
            NULL, EOAC_NONE, NULL);
        IWbemLocator *pLoc = NULL;
        if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID *)&pLoc))) {
            IWbemServices *pSvc = NULL;
            if (SUCCEEDED(pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc))) {
                CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                    RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                    NULL, EOAC_NONE);
                // Manufacturer, Model, Serial, UUID, DeviceType
                IEnumWbemClassObject* pEnum = NULL;
                if (SUCCEEDED(pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_ComputerSystem"),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnum))) {
                    IWbemClassObject *pObj = NULL;
                    ULONG uReturn = 0;
                    while (pEnum && pEnum->Next(WBEM_INFINITE, 1, &pObj, &uReturn) == S_OK) {
                        VARIANT vt;
                        if (SUCCEEDED(pObj->Get(L"Manufacturer", 0, &vt, 0, 0))) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                            inv.manufacturer = vt.bstrVal ? conv.to_bytes(vt.bstrVal) : "";
                            VariantClear(&vt);
                        }
                        if (SUCCEEDED(pObj->Get(L"Model", 0, &vt, 0, 0))) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                            inv.model = vt.bstrVal ? conv.to_bytes(vt.bstrVal) : "";
                            VariantClear(&vt);
                        }
                        if (SUCCEEDED(pObj->Get(L"SystemType", 0, &vt, 0, 0))) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                            inv.device_type = vt.bstrVal ? conv.to_bytes(vt.bstrVal) : "";
                            VariantClear(&vt);
                        }
                        pObj->Release();
                    }
                    pEnum->Release();
                }
                // Serial Number, UUID
                if (SUCCEEDED(pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_ComputerSystemProduct"),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnum))) {
                    IWbemClassObject *pObj = NULL;
                    ULONG uReturn = 0;
                    while (pEnum && pEnum->Next(WBEM_INFINITE, 1, &pObj, &uReturn) == S_OK) {
                        VARIANT vt;
                        if (SUCCEEDED(pObj->Get(L"IdentifyingNumber", 0, &vt, 0, 0))) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                            inv.serial_number = vt.bstrVal ? conv.to_bytes(vt.bstrVal) : "";
                            VariantClear(&vt);
                        }
                        if (SUCCEEDED(pObj->Get(L"UUID", 0, &vt, 0, 0))) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                            inv.uuid = vt.bstrVal ? conv.to_bytes(vt.bstrVal) : "";
                            VariantClear(&vt);
                        }
                        pObj->Release();
                    }
                    pEnum->Release();
                }
                // OS Name, Version
                if (SUCCEEDED(pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_OperatingSystem"),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnum))) {
                    IWbemClassObject *pObj = NULL;
                    ULONG uReturn = 0;
                    while (pEnum && pEnum->Next(WBEM_INFINITE, 1, &pObj, &uReturn) == S_OK) {
                        VARIANT vt;
                        if (SUCCEEDED(pObj->Get(L"Caption", 0, &vt, 0, 0))) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                            inv.os_name = vt.bstrVal ? conv.to_bytes(vt.bstrVal) : "";
                            VariantClear(&vt);
                        }
                        if (SUCCEEDED(pObj->Get(L"Version", 0, &vt, 0, 0))) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                            inv.os_version = vt.bstrVal ? conv.to_bytes(vt.bstrVal) : "";
                            VariantClear(&vt);
                        }
                        pObj->Release();
                    }
                    pEnum->Release();
                }
                // CPU Model, Frequency
                if (SUCCEEDED(pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_Processor"),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnum))) {
                    IWbemClassObject *pObj = NULL;
                    ULONG uReturn = 0;
                    while (pEnum && pEnum->Next(WBEM_INFINITE, 1, &pObj, &uReturn) == S_OK) {
                        VARIANT vt;
                        if (SUCCEEDED(pObj->Get(L"Name", 0, &vt, 0, 0))) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                            inv.cpu_model = vt.bstrVal ? conv.to_bytes(vt.bstrVal) : "";
                            VariantClear(&vt);
                        }
                        if (SUCCEEDED(pObj->Get(L"MaxClockSpeed", 0, &vt, 0, 0))) {
                            inv.cpu_frequency = std::to_string(vt.uintVal) + " MHz";
                            VariantClear(&vt);
                        }
                        pObj->Release();
                    }
                    pEnum->Release();
                }
                // Memory Type (по первому модулю)
                if (SUCCEEDED(pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_PhysicalMemory"),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnum))) {
                    IWbemClassObject *pObj = NULL;
                    ULONG uReturn = 0;
                    while (pEnum && pEnum->Next(WBEM_INFINITE, 1, &pObj, &uReturn) == S_OK) {
                        VARIANT vt;
                        if (SUCCEEDED(pObj->Get(L"MemoryType", 0, &vt, 0, 0))) {
                            // Преобразуем числовой тип памяти в текстовый
                            switch (vt.uintVal) {
                                case 0: inv.memory_type = "Unknown"; break;
                                case 1: inv.memory_type = "Other"; break;
                                case 2: inv.memory_type = "DRAM"; break;
                                case 3: inv.memory_type = "Synchronous DRAM"; break;
                                case 4: inv.memory_type = "Cache DRAM"; break;
                                case 5: inv.memory_type = "EDO"; break;
                                case 6: inv.memory_type = "EDRAM"; break;
                                case 7: inv.memory_type = "VRAM"; break;
                                case 8: inv.memory_type = "SRAM"; break;
                                case 9: inv.memory_type = "RAM"; break;
                                case 10: inv.memory_type = "ROM"; break;
                                case 11: inv.memory_type = "Flash"; break;
                                case 12: inv.memory_type = "EEPROM"; break;
                                case 13: inv.memory_type = "FEPROM"; break;
                                case 14: inv.memory_type = "EPROM"; break;
                                case 15: inv.memory_type = "CDRAM"; break;
                                case 16: inv.memory_type = "3DRAM"; break;
                                case 17: inv.memory_type = "SDRAM"; break;
                                case 18: inv.memory_type = "SGRAM"; break;
                                case 19: inv.memory_type = "RDRAM"; break;
                                case 20: inv.memory_type = "DDR"; break;
                                case 21: inv.memory_type = "DDR2"; break;
                                case 22: inv.memory_type = "DDR2 FB-DIMM"; break;
                                case 24: inv.memory_type = "DDR3"; break;
                                case 26: inv.memory_type = "FBD2"; break;
                                case 34: inv.memory_type = "DDR4"; break;
                                case 35: inv.memory_type = "LPDDR"; break;
                                case 36: inv.memory_type = "LPDDR2"; break;
                                case 37: inv.memory_type = "LPDDR3"; break;
                                case 38: inv.memory_type = "LPDDR4"; break;
                                default: inv.memory_type = "Unknown (" + std::to_string(vt.uintVal) + ")"; break;
                            }
                            VariantClear(&vt);
                        }
                        pObj->Release();
                        break; // только первый модуль
                    }
                    pEnum->Release();
                }
                // Disk Model, Type, Total Bytes (по первому диску)
                if (SUCCEEDED(pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_DiskDrive"),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnum))) {
                    IWbemClassObject *pObj = NULL;
                    ULONG uReturn = 0;
                    while (pEnum && pEnum->Next(WBEM_INFINITE, 1, &pObj, &uReturn) == S_OK) {
                        VARIANT vt;
                        if (SUCCEEDED(pObj->Get(L"Model", 0, &vt, 0, 0))) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                            inv.disk_model = vt.bstrVal ? conv.to_bytes(vt.bstrVal) : "";
                            VariantClear(&vt);
                        }
                        if (SUCCEEDED(pObj->Get(L"MediaType", 0, &vt, 0, 0))) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                            inv.disk_type = vt.bstrVal ? conv.to_bytes(vt.bstrVal) : "";
                            VariantClear(&vt);
                        }
                        if (SUCCEEDED(pObj->Get(L"Size", 0, &vt, 0, 0))) {
                            inv.disk_total_bytes = vt.ullVal;
                            VariantClear(&vt);
                        }
                        pObj->Release();
                        break; // только первый диск
                    }
                    pEnum->Release();
                }
                // GPU Model (по первому GPU)
                if (SUCCEEDED(pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_VideoController"),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnum))) {
                    IWbemClassObject *pObj = NULL;
                    ULONG uReturn = 0;
                    while (pEnum && pEnum->Next(WBEM_INFINITE, 1, &pObj, &uReturn) == S_OK) {
                        VARIANT vt;
                        if (SUCCEEDED(pObj->Get(L"Name", 0, &vt, 0, 0))) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                            inv.gpu_model = vt.bstrVal ? conv.to_bytes(vt.bstrVal) : "";
                            VariantClear(&vt);
                        }
                        pObj->Release();
                        break; // только первый GPU
                    }
                    pEnum->Release();
                }
                // MAC/IP адреса (по всем интерфейсам)
                if (SUCCEEDED(pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_NetworkAdapterConfiguration WHERE IPEnabled = TRUE"),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnum))) {
                    IWbemClassObject *pObj = NULL;
                    ULONG uReturn = 0;
                    while (pEnum && pEnum->Next(WBEM_INFINITE, 1, &pObj, &uReturn) == S_OK) {
                        VARIANT vt;
                        if (SUCCEEDED(pObj->Get(L"MACAddress", 0, &vt, 0, 0))) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                            if (vt.bstrVal) inv.mac_addresses.push_back(conv.to_bytes(vt.bstrVal));
                            VariantClear(&vt);
                        }
                        if (SUCCEEDED(pObj->Get(L"IPAddress", 0, &vt, 0, 0))) {
                            if (vt.vt & (VT_ARRAY | VT_BSTR) && vt.parray) {
                                LONG lbound, ubound;
                                SafeArrayGetLBound(vt.parray, 1, &lbound);
                                SafeArrayGetUBound(vt.parray, 1, &ubound);
                                for (LONG i = lbound; i <= ubound; ++i) {
                                    BSTR bstr;
                                    SafeArrayGetElement(vt.parray, &i, &bstr);
                                    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                                    inv.ip_addresses.push_back(conv.to_bytes(bstr));
                                }
                            }
                            VariantClear(&vt);
                        }
                        pObj->Release();
                    }
                    pEnum->Release();
                }
                // Список установленных программ (через Win32_Product — медленно, но просто)
                if (SUCCEEDED(pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_Product"),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnum))) {
                    IWbemClassObject *pObj = NULL;
                    ULONG uReturn = 0;
                    int count = 0;
                    while (pEnum && pEnum->Next(WBEM_INFINITE, 1, &pObj, &uReturn) == S_OK) {
                        VARIANT vt;
                        if (SUCCEEDED(pObj->Get(L"Name", 0, &vt, 0, 0))) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                            if (vt.bstrVal) inv.installed_software.push_back(conv.to_bytes(vt.bstrVal));
                            VariantClear(&vt);
                        }
                        pObj->Release();
                        if (++count > 1000) break; // ограничение для ускорения
                    }
                    pEnum->Release();
                }
                pSvc->Release();
            }
            pLoc->Release();
        }
        CoUninitialize();
    }
    // --- Конец сбора инвентаризационных данных ---

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
        return metrics;
    }

    // Получаем текущие значения времени
    FILETIME idle_time, kernel_time, user_time;
    if (!GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
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
        return metrics;
    }
    PIP_ADAPTER_INFO pAdapterInfo = (PIP_ADAPTER_INFO)malloc(bufferSize);
    if (pAdapterInfo == nullptr) {
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
        return metrics;
    }
    pAdapterInfo = (PIP_ADAPTER_INFO)malloc(bufferSize);
    if (pAdapterInfo == nullptr) {
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

    // === Сбор TCP соединений ===
    PMIB_TCPTABLE_OWNER_PID pTcpTable = nullptr;
    DWORD dwSize = 0;
    if (GetExtendedTcpTable(nullptr, &dwSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == ERROR_INSUFFICIENT_BUFFER) {
        pTcpTable = (PMIB_TCPTABLE_OWNER_PID)malloc(dwSize);
        if (pTcpTable && GetExtendedTcpTable(pTcpTable, &dwSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
            for (DWORD i = 0; i < pTcpTable->dwNumEntries; ++i) {
                const auto& row = pTcpTable->table[i];
                NetworkConnection conn;
                char localAddr[INET_ADDRSTRLEN] = {0};
                char remoteAddr[INET_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET, &row.dwLocalAddr, localAddr, sizeof(localAddr));
                inet_ntop(AF_INET, &row.dwRemoteAddr, remoteAddr, sizeof(remoteAddr));
                conn.local_ip = localAddr;
                conn.local_port = ntohs((u_short)row.dwLocalPort);
                conn.remote_ip = remoteAddr;
                conn.remote_port = ntohs((u_short)row.dwRemotePort);
                conn.protocol = "TCP";
                metrics.connections.push_back(conn);
            }
        }
        if (pTcpTable) free(pTcpTable);
    }
    // === Сбор UDP соединений ===
    PMIB_UDPTABLE_OWNER_PID pUdpTable = nullptr;
    dwSize = 0;
    if (GetExtendedUdpTable(nullptr, &dwSize, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == ERROR_INSUFFICIENT_BUFFER) {
        pUdpTable = (PMIB_UDPTABLE_OWNER_PID)malloc(dwSize);
        if (pUdpTable && GetExtendedUdpTable(pUdpTable, &dwSize, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
            for (DWORD i = 0; i < pUdpTable->dwNumEntries; ++i) {
                const auto& row = pUdpTable->table[i];
                NetworkConnection conn;
                char localAddr[INET_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET, &row.dwLocalAddr, localAddr, sizeof(localAddr));
                conn.local_ip = localAddr;
                conn.local_port = ntohs((u_short)row.dwLocalPort);
                conn.remote_ip = ""; // UDP не всегда имеет remote
                conn.remote_port = 0;
                conn.protocol = "UDP";
                metrics.connections.push_back(conn);
            }
        }
        if (pUdpTable) free(pUdpTable);
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

// Вспомогательная функция для определения виртуалка/физика
std::string detect_machine_type_windows() {
    HRESULT hres;
    std::string result = "physical";
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) return result;
    hres = CoInitializeSecurity(NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL);
    if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
        CoUninitialize();
        return result;
    }
    IWbemLocator *pLoc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID *)&pLoc);
    if (FAILED(hres)) {
        CoUninitialize();
        return result;
    }
    IWbemServices *pSvc = NULL;
    hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) {
        pLoc->Release();
        CoUninitialize();
        return result;
    }
    hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE);
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return result;
    }
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT Manufacturer, Model FROM Win32_ComputerSystem"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator);
    if (SUCCEEDED(hres)) {
        IWbemClassObject *pObj = NULL;
        ULONG uReturn = 0;
        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn);
            if (0 == uReturn) break;
            VARIANT vtManuf, vtModel;
            hr = pObj->Get(L"Manufacturer", 0, &vtManuf, 0, 0);
            hr = pObj->Get(L"Model", 0, &vtModel, 0, 0);
            if (SUCCEEDED(hr)) {
                std::wstring manuf = vtManuf.bstrVal ? vtManuf.bstrVal : L"";
                std::wstring model = vtModel.bstrVal ? vtModel.bstrVal : L"";
                std::wstring data = manuf + L" " + model;
                // Корректное преобразование в UTF-8
                std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                std::string data_str = conv.to_bytes(data);
                // Признаки виртуализации
                if (data_str.find("VirtualBox") != std::string::npos ||
                    data_str.find("VMware") != std::string::npos ||
                    data_str.find("KVM") != std::string::npos ||
                    data_str.find("QEMU") != std::string::npos ||
                    data_str.find("Xen") != std::string::npos ||
                    data_str.find("Microsoft Corporation Virtual Machine") != std::string::npos) {
                    result = "virtual";
                }
            }
            VariantClear(&vtManuf);
            VariantClear(&vtModel);
            pObj->Release();
        }
        pEnumerator->Release();
    }
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
    return result;
}

} // namespace monitoring  