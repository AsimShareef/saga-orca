CREATE TABLE IF NOT EXISTS saga_event_log (
    id SERIAL PRIMARY KEY,
    tx_id VARCHAR(255) NOT NULL,
    current_state VARCHAR(50) NOT NULL,
    payload JSONB,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX idx_tx_id ON saga_event_log(tx_id);
CREATE INDEX idx_state ON saga_event_log(current_state);