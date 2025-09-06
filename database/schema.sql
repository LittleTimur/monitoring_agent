-- Объединенная схема базы данных
-- Создано на основе бд.txt и бд-доп.txt

-- Миграция 001: Основные таблицы агентов
-- Создано на основе бд.txt

-- 1. Таблица agents (Основная сущность)
CREATE TABLE agents (
    agent_id VARCHAR(255) PRIMARY KEY,
    machine_name VARCHAR(255) NOT NULL,
    agent_ip INET,  -- Реальный IP адрес агента
    auto_detect_id BOOLEAN DEFAULT TRUE,
    auto_detect_name BOOLEAN DEFAULT TRUE,
    command_server_host INET DEFAULT '0.0.0.0',
    command_server_port INTEGER DEFAULT 8081,
    command_server_url TEXT,
    server_url TEXT NOT NULL,
    scripts_dir TEXT DEFAULT 'scripts',
    audit_log_enabled BOOLEAN DEFAULT FALSE,
    audit_log_path TEXT,
    enable_inline_commands BOOLEAN DEFAULT TRUE,
    enable_user_parameters BOOLEAN DEFAULT TRUE,
    job_retention_seconds INTEGER DEFAULT 3600,
    max_buffer_size INTEGER DEFAULT 10,
    max_concurrent_jobs INTEGER DEFAULT 3,
    max_output_bytes BIGINT DEFAULT 1000000,
    max_script_timeout_sec INTEGER DEFAULT 60,
    send_timeout_ms INTEGER DEFAULT 2000,
    update_frequency INTEGER DEFAULT 60,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    last_heartbeat TIMESTAMPTZ
);

-- 2. Таблица interpreters (Справочная)
CREATE TABLE interpreters (
    name VARCHAR(50) PRIMARY KEY
);

-- 3. Таблица metric_types (Справочная)
CREATE TABLE metric_types (
    name VARCHAR(50) PRIMARY KEY
);

-- 4. Таблица agent_allowed_interpreters (Связь многие-ко-многим)
CREATE TABLE agent_allowed_interpreters (
    agent_id VARCHAR(255) REFERENCES agents(agent_id) ON DELETE CASCADE,
    interpreter_name VARCHAR(50) REFERENCES interpreters(name) ON DELETE CASCADE,
    PRIMARY KEY (agent_id, interpreter_name)
);

-- 5. Таблица agent_enabled_metrics (Связь многие-ко-многим)
CREATE TABLE agent_enabled_metrics (
    agent_id VARCHAR(255) REFERENCES agents(agent_id) ON DELETE CASCADE,
    metric_name VARCHAR(50) REFERENCES metric_types(name) ON DELETE CASCADE,
    PRIMARY KEY (agent_id, metric_name)
);

-- 6. Таблица agent_user_parameters (Пользовательские параметры)
CREATE TABLE agent_user_parameters (
    id SERIAL PRIMARY KEY,
    agent_id VARCHAR(255) NOT NULL REFERENCES agents(agent_id) ON DELETE CASCADE,
    parameter_key VARCHAR(255) NOT NULL,
    command TEXT NOT NULL,
    UNIQUE(agent_id, parameter_key)
);

-- Индексы для быстрого поиска
CREATE INDEX idx_agents_machine_name ON agents(machine_name);
CREATE INDEX idx_agents_last_heartbeat ON agents(last_heartbeat);
CREATE INDEX idx_agents_created_at ON agents(created_at);
CREATE INDEX idx_agent_user_parameters_agent_id ON agent_user_parameters(agent_id);
CREATE INDEX idx_agent_user_parameters_key ON agent_user_parameters(parameter_key);

-- Миграция 002: Таблицы метрик
-- Создано на основе бд-доп.txt

-- Основная таблица для хранения метрик
CREATE TABLE agent_metrics (
    id BIGSERIAL PRIMARY KEY,
    agent_id VARCHAR(255) NOT NULL REFERENCES agents(agent_id) ON DELETE CASCADE,
    timestamp TIMESTAMPTZ NOT NULL,
    machine_type VARCHAR(50) NOT NULL CHECK (machine_type IN ('physical', 'virtual')),
    machine_name VARCHAR(255) NOT NULL,
    metric_type VARCHAR(20) NOT NULL CHECK (metric_type IN (
        'cpu', 'memory', 'disk', 'network', 'gpu', 'hdd', 'user', 'inventory'
    )),
    
    -- Общие числовые метрики (оптимизированы для запросов)
    usage_percent FLOAT,
    temperature FLOAT,
    total_bytes BIGINT,
    used_bytes BIGINT,
    free_bytes BIGINT,
    
    -- Детальные данные в JSONB
    details JSONB
);

-- Таблица для сетевых соединений (отдельно, т.к. может быть много записей)
CREATE TABLE metrics_network_connections (
    id BIGSERIAL PRIMARY KEY,
    metric_id BIGINT REFERENCES agent_metrics(id) ON DELETE CASCADE,
    local_ip VARCHAR(45),
    local_port INTEGER,
    remote_ip VARCHAR(45),
    remote_port INTEGER,
    protocol VARCHAR(10) CHECK (protocol IN ('TCP', 'UDP'))
);

-- Индексы для быстрого поиска метрик
CREATE INDEX idx_agent_metrics_agent_id ON agent_metrics(agent_id);
CREATE INDEX idx_agent_metrics_timestamp ON agent_metrics(timestamp);
CREATE INDEX idx_agent_metrics_type ON agent_metrics(metric_type);
CREATE INDEX idx_agent_metrics_agent_timestamp ON agent_metrics(agent_id, timestamp);
CREATE INDEX idx_agent_metrics_usage ON agent_metrics(usage_percent) WHERE usage_percent IS NOT NULL;
CREATE INDEX idx_agent_metrics_details_gin ON agent_metrics USING GIN (details);

CREATE INDEX idx_network_connections_metric ON metrics_network_connections(metric_id);
CREATE INDEX idx_network_connections_local ON metrics_network_connections(local_ip);
CREATE INDEX idx_network_connections_remote ON metrics_network_connections(remote_ip);

-- Заполнение справочных данных
-- Тестовые данные для справочных таблиц

-- Заполнение таблицы interpreters (интерпретаторы)
INSERT INTO interpreters (name) VALUES 
    ('bash'),
    ('powershell'),
    ('cmd'),
    ('python'),
    ('sh')
ON CONFLICT (name) DO NOTHING;

-- Заполнение таблицы metric_types (типы метрик)
INSERT INTO metric_types (name) VALUES 
    ('cpu'),
    ('memory'),
    ('disk'),
    ('network'),
    ('gpu'),
    ('hdd'),
    ('inventory'),
    ('user')
ON CONFLICT (name) DO NOTHING;