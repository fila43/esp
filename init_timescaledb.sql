-- Inicializace schématu pro TimescaleDB
-- Aktivuj rozšíření TimescaleDB (spusť v každé nové DB)
CREATE EXTENSION IF NOT EXISTS timescaledb;

-- Tabulka zařízení
CREATE TABLE device (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL
);

-- Tabulka měření (time-series, bude hypertable)
CREATE TABLE device_measurement (
    id BIGSERIAL PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES device(id) ON DELETE CASCADE,
    measured_at TIMESTAMPTZ NOT NULL DEFAULT now(), -- časové razítko
    temperature DOUBLE PRECISION NOT NULL,          -- aktuální teplota
    target_temperature DOUBLE PRECISION NOT NULL,   -- cílová teplota
    is_active BOOLEAN NOT NULL,                     -- zda zařízení skutečně běželo (sepnuté)
    mode VARCHAR(8) NOT NULL                       -- 'ON', 'OFF', 'AUTO' (režim v době měření)
);

-- Z tabulky device_measurement udělej hypertable
SELECT create_hypertable('device_measurement', 'measured_at', if_not_exists => TRUE);

-- Index pro rychlé dotazy
CREATE INDEX IF NOT EXISTS idx_device_measurement_device_time ON device_measurement (device_id, measured_at DESC); 