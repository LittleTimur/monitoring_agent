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
