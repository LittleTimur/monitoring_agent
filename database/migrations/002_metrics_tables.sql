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
    details JSONB,
    
    -- Индексы
    INDEX idx_agent_metrics_agent_id (agent_id),
    INDEX idx_agent_metrics_timestamp (timestamp),
    INDEX idx_agent_metrics_type (metric_type),
    INDEX idx_agent_metrics_agent_timestamp (agent_id, timestamp),
    INDEX idx_agent_metrics_usage (usage_percent) WHERE usage_percent IS NOT NULL,
    INDEX idx_agent_metrics_details_gin ON agent_metrics USING GIN (details)
);

-- Таблица для сетевых соединений (отдельно, т.к. может быть много записей)
CREATE TABLE metrics_network_connections (
    id BIGSERIAL PRIMARY KEY,
    metric_id BIGINT REFERENCES agent_metrics(id) ON DELETE CASCADE,
    local_ip VARCHAR(45),
    local_port INTEGER,
    remote_ip VARCHAR(45),
    remote_port INTEGER,
    protocol VARCHAR(10) CHECK (protocol IN ('TCP', 'UDP')),
    
    INDEX idx_network_connections_metric (metric_id),
    INDEX idx_network_connections_local (local_ip),
    INDEX idx_network_connections_remote (remote_ip)
);

-- Индексы для быстрого поиска
CREATE INDEX idx_agent_metrics_agent_id ON agent_metrics(agent_id);
CREATE INDEX idx_agent_metrics_timestamp ON agent_metrics(timestamp);
CREATE INDEX idx_agent_metrics_type ON agent_metrics(metric_type);
CREATE INDEX idx_agent_metrics_agent_timestamp ON agent_metrics(agent_id, timestamp);
CREATE INDEX idx_agent_metrics_usage ON agent_metrics(usage_percent) WHERE usage_percent IS NOT NULL;
CREATE INDEX idx_agent_metrics_details_gin ON agent_metrics USING GIN (details);

CREATE INDEX idx_network_connections_metric ON metrics_network_connections(metric_id);
CREATE INDEX idx_network_connections_local ON metrics_network_connections(local_ip);
CREATE INDEX idx_network_connections_remote ON metrics_network_connections(remote_ip);
