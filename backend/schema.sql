CREATE TABLE IF NOT EXISTS players (
  id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin PRIMARY KEY,
  username VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
  password_hash VARCHAR(180) CHARACTER SET ascii NOT NULL,
  unlocked_stage VARCHAR(64) CHARACTER SET ascii NOT NULL,
  wallet JSON NOT NULL,
  revision INT UNSIGNED NOT NULL DEFAULT 0,
  imported BOOLEAN NOT NULL DEFAULT FALSE,
  created_at TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS sessions (
  token_hash BINARY(32) PRIMARY KEY,
  player_id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  expires_at DATETIME NOT NULL,
  INDEX (expires_at),
  FOREIGN KEY (player_id) REFERENCES players(id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS level_progress (
  player_id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  stage_id VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  best_score INT UNSIGNED NOT NULL,
  best_stars TINYINT UNSIGNED NOT NULL CHECK (best_stars <= 3),
  PRIMARY KEY (player_id, stage_id),
  FOREIGN KEY (player_id) REFERENCES players(id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS player_events (
  player_id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  request_id VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  operation VARCHAR(32) CHARACTER SET ascii NOT NULL,
  fingerprint BINARY(32) NOT NULL,
  result JSON NOT NULL,
  wallet_changes JSON NOT NULL,
  created_at TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  PRIMARY KEY (player_id, request_id),
  FOREIGN KEY (player_id) REFERENCES players(id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS provider_receipts (
  provider VARCHAR(8) CHARACTER SET ascii NOT NULL,
  reference_hash BINARY(32) NOT NULL,
  player_id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  product VARCHAR(80) CHARACTER SET ascii NOT NULL,
  quantity INT UNSIGNED NOT NULL,
  created_at TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  PRIMARY KEY (provider, reference_hash),
  FOREIGN KEY (player_id) REFERENCES players(id) ON DELETE CASCADE
) ENGINE=InnoDB;
