CREATE TABLE IF NOT EXISTS users (
    id          SERIAL PRIMARY KEY,
    username    VARCHAR(50)  UNIQUE NOT NULL,
    email       VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(256) NOT NULL,
    rating      INT DEFAULT 1000,
    created_at  TIMESTAMP DEFAULT NOW(),
    updated_at  TIMESTAMP DEFAULT NOW()
);

CREATE INDEX idx_users_username ON users(username);
CREATE INDEX idx_users_email    ON users(email);

CREATE TABLE IF NOT EXISTS problems (
    id          SERIAL PRIMARY KEY,
    title       VARCHAR(255) NOT NULL,
    description TEXT NOT NULL,
    difficulty  INT DEFAULT 1,
    time_limit  INT DEFAULT 1000,
    memory_limit INT DEFAULT 256,
    created_by  INT REFERENCES users(id),
    created_at  TIMESTAMP DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS test_cases (
    id          SERIAL PRIMARY KEY,
    problem_id  INT REFERENCES problems(id) ON DELETE CASCADE,
    input       TEXT NOT NULL,
    expected    TEXT NOT NULL,
    is_sample   BOOLEAN DEFAULT FALSE,
    sort_order  INT DEFAULT 0
);

CREATE TABLE IF NOT EXISTS submissions (
    id          SERIAL PRIMARY KEY,
    user_id     INT REFERENCES users(id),
    problem_id  INT REFERENCES problems(id),
    language    VARCHAR(20) NOT NULL,
    code        TEXT NOT NULL,
    status      VARCHAR(20) DEFAULT 'pending',
    runtime_ms  INT,
    memory_kb   INT,
    created_at  TIMESTAMP DEFAULT NOW()
);

CREATE INDEX idx_submissions_user    ON submissions(user_id);
CREATE INDEX idx_submissions_problem ON submissions(problem_id);
CREATE INDEX idx_submissions_status  ON submissions(status);

CREATE TABLE IF NOT EXISTS competitions (
    id          SERIAL PRIMARY KEY,
    title       VARCHAR(255) NOT NULL,
    start_time  TIMESTAMP NOT NULL,
    end_time    TIMESTAMP NOT NULL,
    created_by  INT REFERENCES users(id),
    created_at  TIMESTAMP DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS competition_participants (
    competition_id INT REFERENCES competitions(id) ON DELETE CASCADE,
    user_id        INT REFERENCES users(id),
    score          INT DEFAULT 0,
    rank           INT,
    PRIMARY KEY (competition_id, user_id)
);
