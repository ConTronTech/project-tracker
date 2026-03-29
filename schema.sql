DROP TABLE IF EXISTS files;
DROP TABLE IF EXISTS projects;

CREATE TABLE projects (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    slug TEXT UNIQUE NOT NULL,
    name TEXT NOT NULL,
    status TEXT DEFAULT 'active',
    priority TEXT DEFAULT 'medium',
    tags TEXT DEFAULT '',
    description TEXT DEFAULT '',
    repo_path TEXT DEFAULT '',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id INTEGER NOT NULL,
    filename TEXT NOT NULL,
    content BLOB,
    size INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE,
    UNIQUE(project_id, filename)
);

CREATE INDEX idx_slug ON projects(slug);
CREATE INDEX idx_status ON projects(status);
CREATE INDEX idx_files ON files(project_id, filename);

INSERT INTO projects (slug, name, status, priority, tags, description, repo_path) VALUES
('space_game', 'Space Game', 'active', 'high', 'c++,sdl2,opengl', '6DOF space exploration game', '/root/clawd/projects/space-game/'),
('netman', 'NetMan', 'active', 'high', 'c++,gtk4,networking', 'Network manager GUI', '/root/clawd/projects/netman/'),
('graphiti', 'Graphiti Knowledge Graph', 'completed', 'high', 'python,neo4j,ai', 'Temporal knowledge graph', '/root/clawd/skills/graphiti/'),
('fire7_root', 'Fire 7 Root', 'abandoned', 'high', 'android,exploit,mediatek', 'CVE-2022-38181 Mali exploit', '/root/clawd/projects/AmazonFire7/'),
('scav_modding', 'Scav Mod Manager', 'paused', 'medium', 'unity,c#,modding', 'Mod framework for Scav Prototype', '/root/clawd/projects/scav/'),
('starbound_dungeons', 'Starbound Dungeons', 'paused', 'medium', 'starbound,modding', 'Procedural dungeon generation', '/root/clawd/projects/Starbound - Unstable/');
