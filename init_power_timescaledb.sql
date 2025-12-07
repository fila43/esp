-- TimescaleDB Power Monitoring Database - Simple Structure
-- Single table with autoincrement ID, device_id, power consumption, timestamp

-- Create TimescaleDB extension
CREATE EXTENSION IF NOT EXISTS timescaledb;

-- Drop existing table if exists
DROP TABLE IF EXISTS power_measurements;

-- Create power measurements table
CREATE TABLE power_measurements (
    id BIGSERIAL,                                -- Auto-increment ID (6.3M records/year capacity)
    device_id INTEGER NOT NULL,                  -- 32-bit ESP Chip ID 
    power_consumption REAL NOT NULL,             -- Power consumption in Watts
    measured_at TIMESTAMPTZ NOT NULL DEFAULT NOW(), -- Timestamp
    PRIMARY KEY (id, measured_at)                -- Composite key for TimescaleDB
);

-- Create TimescaleDB hypertable
SELECT create_hypertable('power_measurements', 'measured_at', if_not_exists => TRUE);