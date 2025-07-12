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
- **HDD:** температура, время работы, статус здоровья (S.M.A.R.T.)
- **Инвентарь:** производитель, модель, серийный номер, UUID, ОС, установленное ПО

---

## Требования для СБОРКИ

### Общие требования для сборки

- **CMake** ≥ 3.15
- **C++17** компилятор (GCC, Clang, MSVC)
- **git** (для автоматической загрузки зависимостей)

### Windows (сборка)

- **Visual Studio 2019** или новее (Desktop development with C++)
- **CMake** ≥ 3.15 (https://cmake.org/download/)
- **git** (https://git-scm.com/download/win)

#### Установка зависимостей для сборки Windows:

```powershell
# Установка Chocolatey (если не установлен)
Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))

# Установка CMake и Git
choco install cmake git

# Установка Visual Studio 2019+ с C++ компонентами
# Скачать с: https://visualstudio.microsoft.com/downloads/
# Выбрать: "Desktop development with C++"
```

### Linux (сборка)

- **Компилятор:** GCC ≥ 7.0 или Clang ≥ 6.0
- **curl** (dev-пакет для сборки CPR)
- **pkg-config** (для поиска библиотек)
- **make** (или ninja)

#### Установка зависимостей для сборки Linux:

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y build-essential cmake git pkg-config libcurl4-openssl-dev
```

**RHEL/CentOS/Fedora:**
```bash
sudo yum groupinstall "Development Tools"
sudo yum install cmake git pkg-config libcurl-devel
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake git pkg-config curl
```

---

## Сборка проекта

### Windows (PowerShell)

```powershell
git clone https://github.com/yourname/monitoring_agent.git
cd monitoring_agent
mkdir build
cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release
```

### Linux

```bash
git clone https://github.com/yourname/monitoring_agent.git
cd monitoring_agent
mkdir build && cd build
cmake ..
make -j$(nproc)
```

**Где найти готовые бинарники:**
- Linux: `./build/bin/monitoring_agent`
- Windows: `./build/bin/Release/monitoring_agent.exe`

---

## Требования для ЗАПУСКА

### Windows (запуск)

**Обязательно:** Ничего дополнительно не нужно!

**Опционально (для полной функциональности):**
- **smartmontools** - для сбора HDD метрик (температура, здоровье)

#### Установка smartmontools для Windows:

```powershell
# Через Chocolatey
choco install smartmontools

# Или скачать вручную с: https://www.smartmontools.org/
```

### Linux (запуск)

**Обязательно:** Ничего дополнительно не нужно!

**Опционально (для полной функциональности):**
- **smartmontools** - для сбора HDD метрик
- **nvidia-smi** - для метрик NVIDIA GPU
- **rocm-smi** - для метрик AMD GPU

#### Установка опциональных утилит для Linux:

**Ubuntu/Debian:**
```bash
# Для HDD метрик
sudo apt install smartmontools

# Для GPU метрик (устанавливаются с драйверами)
# nvidia-smi - с драйверами NVIDIA
# rocm-smi - с ROCm (AMD GPU drivers)
```

**RHEL/CentOS/Fedora:**
```bash
# Для HDD метрик
sudo yum install smartmontools

# Для GPU метрик (устанавливаются с драйверами)
# nvidia-smi - с драйверами NVIDIA
# rocm-smi - с ROCm (AMD GPU drivers)
```

**Arch Linux:**
```bash
# Для HDD метрик
sudo pacman -S smartmontools

# Для GPU метрик (устанавливаются с драйверами)
# nvidia-smi - с драйверами NVIDIA
# rocm-smi - с ROCm (AMD GPU drivers)
```

---

## Запуск

### Windows

```powershell
# Простой запуск
.\monitoring_agent.exe

# Запуск с выводом в файл
.\monitoring_agent.exe > metrics.log 2>&1

# Запуск в фоне
Start-Process -FilePath ".\monitoring_agent.exe" -WindowStyle Hidden
```

### Linux

```bash
# Простой запуск
./monitoring_agent

# Запуск с выводом в файл
./monitoring_agent > metrics.log 2>&1

# Запуск в фоне
nohup ./monitoring_agent > metrics.log 2>&1 &

# Запуск как служба (systemd)
sudo cp monitoring_agent /usr/local/bin/
sudo tee /etc/systemd/system/monitoring-agent.service << EOF
[Unit]
Description=Monitoring Agent
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/monitoring_agent
Restart=always
User=root

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable monitoring-agent
sudo systemctl start monitoring-agent
```

---

## Что происходит без опциональных утилит

### Без smartmontools:
- HDD метрики (температура, здоровье) не собираются
- Программа работает нормально, остальные метрики собираются

### Без nvidia-smi/rocm-smi:
- GPU метрики не собираются
- Программа работает нормально, остальные метрики собираются

### Без lspci/ip/dpkg/rpm:
- Некоторые инвентарные данные могут быть неполными
- Программа работает нормально, основные метрики собираются

## Что происходит без прав администратора

### Windows (без прав администратора):
- **HDD метрики:** температура и S.M.A.R.T. данные не собираются
- **GPU метрики:** могут быть ограничены (особенно температура)
- **Некоторые инвентарные данные:** могут быть неполными
- **Сетевые соединения:** могут быть ограничены
- **CPU температура:** может не собираться

### Linux (без прав root):
- **HDD метрики:** температура и S.M.A.R.T. данные не собираются
- **GPU метрики:** могут быть ограничены
- **CPU температура:** может не собираться
- **Некоторые системные метрики:** могут быть ограничены

### Рекомендации:
- **Для полной функциональности:** запускайте программу с правами администратора/root
- **Для базового мониторинга:** программа работает и без прав администратора, собирая основные метрики

---

## Возможные проблемы

### Сборка

**CPR/curl не собирается на Windows:**
- Убедитесь, что установлен Visual Studio 2019+ и выбран профиль x64
- Иногда помогает ручная установка curl и добавление его в PATH
- Если CMake не находит curl, попробуйте удалить папку `build` и пересобрать проект

**Ошибки компиляции на Linux:**
- Убедитесь, что установлены все dev-пакеты: `sudo apt install build-essential libcurl4-openssl-dev`

### Запуск

**Программа не запускается:**
- Убедитесь, что бинарник имеет права на выполнение: `chmod +x monitoring_agent`
- Проверьте, что все необходимые библиотеки доступны

**HDD метрики не собираются:**
- Установите smartmontools: `sudo apt install smartmontools` (Linux) или `choco install smartmontools` (Windows)
- Убедитесь, что у пользователя есть права на доступ к S.M.A.R.T. данным

**GPU метрики не собираются:**
- Установите соответствующие драйверы GPU (NVIDIA/AMD)
- Убедитесь, что nvidia-smi или rocm-smi доступны в PATH

---

## Дополнительная информация

### Зависимости (автоматически устанавливаются при сборке)
- **[CPR (C++ Requests)](https://github.com/libcpr/cpr)** — HTTP-клиент, подтягивается автоматически через CMake
- **[nlohmann/json](https://github.com/nlohmann/json)** — уже включён в проект

### Сторонние утилиты (нужны для запуска)
- **smartctl** (smartmontools) - для HDD метрик
- **nvidia-smi** - для NVIDIA GPU метрик
- **rocm-smi** - для AMD GPU метрик
- **lspci** - для определения GPU модели
- **ip** - для получения сетевых адресов
- **dpkg/rpm** - для списка установленного ПО

---

## Лицензия

MIT License

