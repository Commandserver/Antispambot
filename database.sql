CREATE SCHEMA IF NOT EXISTS antispambot;

USE antispambot;

CREATE TABLE IF NOT EXISTS guilds
(
    id                INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    created_at        TIMESTAMP       NOT NULL DEFAULT CURRENT_TIMESTAMP(),
    left_at           TIMESTAMP       NULL COMMENT 'Timestamp when the bot was removed from the guild',
    guild_id          BIGINT UNSIGNED NOT NULL COMMENT 'Discord ID of the guild',
    thread_rate_limit INT                      DEFAULT 0 COMMENT '-1 for disabling that feature, 0 to inherit the value from the channel. Anything above as seconds',
    log_channel_id    BIGINT UNSIGNED NULL COMMENT 'Discord ID of the log channel in the guild',
    UNIQUE (guild_id)
) COMMENT 'The Guild data for multi guild ability';

CREATE TABLE IF NOT EXISTS bypasses
(
    guild_id       INT UNSIGNED                     NOT NULL,
    FOREIGN KEY (guild_id) REFERENCES guilds (id)
        ON DELETE CASCADE
        ON UPDATE RESTRICT,
    id             INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    type           ENUM ('user', 'role', 'channel') NOT NULL,
    mentionable_id BIGINT UNSIGNED                  NOT NULL COMMENT 'Discord ID of the user, role or channel',
    UNIQUE (guild_id, mentionable_id)
) COMMENT 'excluded users, roles or specific channels/categories that are not affected by the antispam';
CREATE INDEX bypass_guild_id_index ON bypasses (guild_id);

CREATE TABLE IF NOT EXISTS forbidden_file_extensions
(
    guild_id INT UNSIGNED NOT NULL,
    FOREIGN KEY (guild_id) REFERENCES guilds (id)
        ON DELETE CASCADE
        ON UPDATE RESTRICT,
    id       INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    value    VARCHAR(63)  NOT NULL COMMENT 'The file extension',
    UNIQUE (guild_id, value)
) COMMENT 'Forbidden file extensions in text channels';
CREATE INDEX forbidden_file_extension_guild_id_index ON forbidden_file_extensions (guild_id);

CREATE TABLE IF NOT EXISTS recently_joined_users
(
    guild_id  INT UNSIGNED    NOT NULL,
    FOREIGN KEY (guild_id) REFERENCES guilds (id)
        ON DELETE CASCADE
        ON UPDATE RESTRICT,
    id        INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    joined_at TIMESTAMP       NOT NULL,
    user_id   BIGINT UNSIGNED NOT NULL COMMENT 'The Discord user ID',
    username  VARCHAR(32)     NOT NULL COMMENT 'The Discord username'
) COMMENT 'Cached members joined in the last few minutes for raid detection';
CREATE INDEX recently_joined_user_guild_id_index ON recently_joined_users (guild_id);

CREATE TABLE IF NOT EXISTS mass_actions
(
    guild_id   INT UNSIGNED         NOT NULL,
    FOREIGN KEY (guild_id) REFERENCES guilds (id)
        ON DELETE CASCADE
        ON UPDATE RESTRICT,
    id         INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    created_at TIMESTAMP            NOT NULL DEFAULT CURRENT_TIMESTAMP() COMMENT 'When the command was triggered',
    type       ENUM ('ban', 'kick') NOT NULL COMMENT 'The type of mass action that got triggered',
    author_id  BIGINT UNSIGNED      NOT NULL COMMENT 'The Discord ID of the user that triggered it'
) COMMENT 'Mass ban or mass kick that got triggered';

CREATE TABLE IF NOT EXISTS mass_actioned_users
(
    mass_action_fk INT UNSIGNED    NOT NULL,
    FOREIGN KEY (mass_action_fk) REFERENCES mass_actions (id)
        ON DELETE CASCADE
        ON UPDATE RESTRICT,
    id             INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    joined_at      TIMESTAMP       NULL COMMENT 'Optional: Timestamp when the user last joined before the mass ban',
    user_id        BIGINT UNSIGNED NOT NULL COMMENT 'The Discord user ID',
    username       VARCHAR(32)     NOT NULL COMMENT 'The Discord username',
    punished_at    TIMESTAMP       NOT NULL DEFAULT CURRENT_TIMESTAMP() COMMENT 'When the user got banned or kicked',
    succeeded      BOOLEAN         NOT NULL COMMENT 'Whether the action was successful'
) COMMENT 'Log of punished users during a mass action';