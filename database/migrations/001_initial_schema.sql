-- Миграция 001: Основные таблицы агентов
-- Создано на основе бд.txt

-- 1. Таблица agents (Основная сущность)
CREATE TABLE agents (
    agent_id VARCHAR(255) PRIMARY KEY,
    machine_name VARCHAR(255) NOT NULL,
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
