#!/bin/bash

APP_NAME="monitoring_agent"
APP_BIN="./monitoring_agent"  # бинарник должен лежать рядом со скриптом
INSTALL_DIR="/opt/monitoring_agent"
SERVICE_FILE="/etc/systemd/system/monitoring_agent.service"
SMARTCTL_URL="https://github.com/smartmontools/smartmontools/releases/download/RELEASE_7_3/smartmontools-7.3.tar.gz"

# 1. Установка зависимостей
if ! command -v curl >/dev/null 2>&1; then
    echo "Устанавливаю curl..."
    if command -v apt >/dev/null 2>&1; then
        sudo apt update && sudo apt install -y curl
    elif command -v yum >/dev/null 2>&1; then
        sudo yum install -y curl
    fi
fi

# 2. Установка smartmontools, если не установлен
if ! command -v smartctl >/dev/null 2>&1; then
    echo "Устанавливаю smartmontools..."
    if command -v apt >/dev/null 2>&1; then
        sudo apt update && sudo apt install -y smartmontools
    elif command -v yum >/dev/null 2>&1; then
        sudo yum install -y smartmontools
    else
        # Fallback: скачать и собрать вручную
        TMPDIR=$(mktemp -d)
        cd "$TMPDIR"
        curl -L "$SMARTCTL_URL" | tar xz
        cd smartmontools-*
        ./configure && make
        sudo make install
        cd /
        rm -rf "$TMPDIR"
    fi
fi

# 3. Копирование бинарника
if sudo -n true 2>/dev/null; then
    echo "Установка с root-правами..."
    sudo mkdir -p "$INSTALL_DIR"
    sudo cp "$APP_BIN" "$INSTALL_DIR/"
    sudo chmod +x "$INSTALL_DIR/$APP_NAME"
    # Создаём config.json с адресом сервера
    echo "{\"server_url\": \"$SERVER_URL\"}" | sudo tee "$INSTALL_DIR/config.json" > /dev/null

    # 4. Создание systemd-сервиса
    sudo bash -c "cat > $SERVICE_FILE" <<EOF
[Unit]
Description=Monitoring Agent

[Service]
ExecStart=$INSTALL_DIR/$APP_NAME
Restart=always

[Install]
WantedBy=multi-user.target
EOF

    sudo systemctl daemon-reload
    sudo systemctl enable monitoring_agent
    sudo systemctl start monitoring_agent
    echo "Monitoring Agent установлен как systemd-сервис и будет запускаться с root-правами."
else
    echo "Установка без root-прав..."
    mkdir -p "$HOME/.local/bin"
    cp "$APP_BIN" "$HOME/.local/bin/"
    chmod +x "$HOME/.local/bin/$APP_NAME"
    # Создаём config.json с адресом сервера
    echo "{\"server_url\": \"$SERVER_URL\"}" > "$HOME/.local/bin/config.json"

    # 4. Добавление в автозапуск (GUI)
    mkdir -p "$HOME/.config/autostart"
    cat > "$HOME/.config/autostart/monitoring_agent.desktop" <<EOF
[Desktop Entry]
Type=Application
Exec=$HOME/.local/bin/$APP_NAME
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
Name=Monitoring Agent
Comment=Start Monitoring Agent on login
EOF

    # 5. Для headless — добавить в ~/.profile
    if ! grep -q "$APP_NAME" "$HOME/.profile"; then
        echo "$HOME/.local/bin/$APP_NAME &" >> "$HOME/.profile"
    fi

    echo "Monitoring Agent добавлен в автозапуск для текущего пользователя."
fi

# 6. Запуск приложения сразу после установки
if sudo -n true 2>/dev/null; then
    sudo "$INSTALL_DIR/$APP_NAME" &
else
    "$HOME/.local/bin/$APP_NAME" &
fi

echo "Установка завершена!" 