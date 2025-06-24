# Monitoring Agent

Кроссплатформенный агент мониторинга системы (CPU, память, диски, сеть, GPU, HDD).
Работает на Windows и Linux. Аналог Zabbix Agent, но проще в установке и использовании.

---

## Собираемые метрики

- **CPU:** загрузка, температура, по ядрам
- **Память:** общий/свободный/использованный объем, процент использования
- **Диски:** разделы, файловые системы, свободное/занятое место
- **Сеть:** интерфейсы, байты/пакеты, скорость
- **GPU:** температура, загрузка, память (NVIDIA/AMD/Intel)
- **HDD:** температура, время работы, статус здоровья

---

## Требования

### Общие

- **CMake** ≥ 3.15
- **C++17** компилятор (GCC, Clang, MSVC)
- **git** (для автоматической загрузки зависимостей)

### Linux

- **Компилятор:** GCC ≥ 7.0 или Clang ≥ 6.0
- **curl** (dev-пакет для сборки CPR)
- **pkg-config** (для поиска библиотек)
- **make** (или ninja)
- **smartmontools** (для HDD-метрик)
- **nvidia-smi** (для NVIDIA GPU, опционально)
- **rocm-smi** (для AMD GPU, опционально)

#### Установка зависимостей (Ubuntu/Debian):

```bash
sudo apt update
sudo apt install -y build-essential cmake git pkg-config libcurl4-openssl-dev smartmontools
# Для GPU-метрик:
# sudo apt install nvidia-smi # если NVIDIA
# sudo apt install rocm-smi   # если AMD
```

### Windows

- **Visual Studio 2019** или новее (Desktop development with C++)
- **CMake** ≥ 3.15 (https://cmake.org/download/)
- **git** (https://git-scm.com/download/win)
- **curl** (устанавливается автоматически через CPR, но иногда требуется вручную: https://curl.se/windows/)

> **Важно:**
> CPR и curl скачиваются автоматически через CMake (FetchContent).
> Если возникнут ошибки с curl, убедитесь, что установлен Visual Studio Build Tools и переменные среды настроены.

---

## Сборка

### Linux

```bash
git clone https://github.com/yourname/monitoring_agent.git
cd monitoring_agent
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Windows (PowerShell)

```powershell
git clone https://github.com/yourname/monitoring_agent.git
cd monitoring_agent
mkdir build
cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release
```

> **Где искать бинарник:**
> - Linux: `build/bin/Release/monitoring_agent`
> - Windows: `build/bin/Release/monitoring_agent.exe`

---

## Запуск

```bash
# Linux
./build/bin/Release/monitoring_agent

# Windows (PowerShell)
.\build\bin\Release\monitoring_agent.exe
```

---

## Возможные проблемы

- **CPR/curl не собирается на Windows:**
  - Убедитесь, что установлен Visual Studio 2019+ и выбран профиль x64.
  - Иногда помогает ручная установка curl и добавление его в PATH.
  - Если CMake не находит curl, попробуйте удалить папку `build` и пересобрать проект.

- **Нет smartctl/nvidia-smi/rocm-smi:**
  - Для HDD-метрик нужен smartmontools (`sudo apt install smartmontools`).
  - Для GPU-метрик нужны соответствующие утилиты.

---

## Дополнительная информация

- **Зависимости:**
  - [CPR (C++ Requests)](https://github.com/libcpr/cpr) — HTTP-клиент, подтягивается автоматически через CMake.
  - [nlohmann/json](https://github.com/nlohmann/json) — уже включён в проект.

- **Docker:**
  Dockerfile для Linux можно добавить при необходимости. Для Windows поддержка Docker ограничена.

---

## Лицензия

MIT License

---

