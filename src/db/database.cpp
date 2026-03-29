#include "db/database.h"
#include "utils/logger.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <sstream>

// ==== Constructor — Init DB with WAL mode ====

ProjectDB::ProjectDB(const std::string& dbPath) : dbPath_(dbPath) {
    try {
        SQLite::Database db(dbPath_, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        db.exec("PRAGMA journal_mode=WAL;");
        db.exec("PRAGMA busy_timeout=5000;");
        db.exec("PRAGMA foreign_keys=ON;");

        db.exec(R"(
            CREATE TABLE IF NOT EXISTS projects (
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
            CREATE TABLE IF NOT EXISTS files (
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
            CREATE INDEX IF NOT EXISTS idx_slug ON projects(slug);
            CREATE INDEX IF NOT EXISTS idx_files ON files(project_id, filename);
        )");

        utils::Logger::get_instance().info("Database initialized: " + dbPath);
    } catch (const std::exception& e) {
        utils::Logger::get_instance().fatal("Database init failed: " + std::string(e.what()));
        throw;
    }
}

// ==== Internal: get project ID by slug (parameterized) ====

int ProjectDB::getProjectId(const std::string& slug) {
    try {
        SQLite::Database db(dbPath_, SQLite::OPEN_READONLY);
        SQLite::Statement q(db, "SELECT id FROM projects WHERE slug = ?");
        q.bind(1, slug);
        if (q.executeStep()) {
            return q.getColumn(0).getInt();
        }
    } catch (const std::exception& e) {
        utils::Logger::get_instance().error("getProjectId: " + std::string(e.what()));
    }
    return -1;
}

// ==== Projects ====

std::vector<Project> ProjectDB::getAll(const std::string& search,
                                        const std::string& status,
                                        int limit, int offset) {
    std::vector<Project> out;

    try {
        SQLite::Database db(dbPath_, SQLite::OPEN_READONLY);

        // Build query with bind params — NO string concatenation
        std::string sql = R"(
            SELECT p.id, p.slug, p.name, p.status, p.priority, p.tags,
                   p.description, p.repo_path, p.created_at, p.updated_at,
                   (SELECT COUNT(*) FROM files f WHERE f.project_id = p.id)
            FROM projects p
            WHERE 1=1
        )";

        std::vector<std::string> params;

        if (!search.empty()) {
            sql += " AND (p.slug LIKE ? OR p.name LIKE ? OR p.tags LIKE ? OR p.description LIKE ?)";
            std::string term = "%" + search + "%";
            params.push_back(term);
            params.push_back(term);
            params.push_back(term);
            params.push_back(term);
        }

        if (!status.empty()) {
            sql += " AND p.status = ?";
            params.push_back(status);
        }

        sql += " ORDER BY p.updated_at DESC";

        if (limit > 0) {
            sql += " LIMIT ? OFFSET ?";
        }

        SQLite::Statement q(db, sql);

        // Bind all params
        int idx = 1;
        for (const auto& param : params) {
            q.bind(idx++, param);
        }
        if (limit > 0) {
            q.bind(idx++, limit);
            q.bind(idx++, offset);
        }

        while (q.executeStep()) {
            Project p;
            p.id = q.getColumn(0).getInt();
            p.slug = q.getColumn(1).getText();
            p.name = q.getColumn(2).getText();
            p.status = q.getColumn(3).getText();
            p.priority = q.getColumn(4).getText();
            p.tags = q.getColumn(5).getText();
            p.description = q.getColumn(6).getText();
            p.repo_path = q.getColumn(7).getText();
            p.created_at = q.getColumn(8).getText();
            p.updated_at = q.getColumn(9).getText();
            p.file_count = q.getColumn(10).getInt();
            out.push_back(p);
        }
    } catch (const std::exception& e) {
        utils::Logger::get_instance().error("getAll: " + std::string(e.what()));
    }

    return out;
}

Project ProjectDB::getBySlug(const std::string& slug) {
    try {
        SQLite::Database db(dbPath_, SQLite::OPEN_READONLY);
        SQLite::Statement q(db, R"(
            SELECT p.id, p.slug, p.name, p.status, p.priority, p.tags,
                   p.description, p.repo_path, p.created_at, p.updated_at
            FROM projects p WHERE p.slug = ?
        )");
        q.bind(1, slug);

        if (!q.executeStep()) return Project{};

        Project p;
        p.id = q.getColumn(0).getInt();
        p.slug = q.getColumn(1).getText();
        p.name = q.getColumn(2).getText();
        p.status = q.getColumn(3).getText();
        p.priority = q.getColumn(4).getText();
        p.tags = q.getColumn(5).getText();
        p.description = q.getColumn(6).getText();
        p.repo_path = q.getColumn(7).getText();
        p.created_at = q.getColumn(8).getText();
        p.updated_at = q.getColumn(9).getText();
        p.files = listFiles(slug);
        p.file_count = static_cast<int>(p.files.size());
        return p;
    } catch (const std::exception& e) {
        utils::Logger::get_instance().error("getBySlug: " + std::string(e.what()));
    }
    return Project{};
}

bool ProjectDB::create(const Project& p) {
    try {
        SQLite::Database db(dbPath_, SQLite::OPEN_READWRITE);
        SQLite::Statement q(db, R"(
            INSERT INTO projects(slug, name, status, priority, tags, description, repo_path)
            VALUES(?, ?, ?, ?, ?, ?, ?)
        )");
        q.bind(1, p.slug);
        q.bind(2, p.name);
        q.bind(3, p.status.empty() ? "active" : p.status);
        q.bind(4, p.priority.empty() ? "medium" : p.priority);
        q.bind(5, p.tags);
        q.bind(6, p.description);
        q.bind(7, p.repo_path);
        return q.exec() > 0;
    } catch (const std::exception& e) {
        utils::Logger::get_instance().error("create: " + std::string(e.what()));
        return false;
    }
}

bool ProjectDB::update(const std::string& slug, const json& fields) {
    try {
        // Get current values first
        auto current = getBySlug(slug);
        if (current.slug.empty()) return false;

        SQLite::Database db(dbPath_, SQLite::OPEN_READWRITE);
        SQLite::Statement q(db, R"(
            UPDATE projects SET
                name = ?, status = ?, priority = ?, tags = ?,
                description = ?, repo_path = ?, updated_at = CURRENT_TIMESTAMP
            WHERE slug = ?
        )");

        // Use provided value or keep current
        auto getField = [&](const std::string& key, const std::string& current_val) -> std::string {
            if (fields.contains(key) && fields[key].is_string()) {
                std::string val = fields[key].get<std::string>();
                return val.empty() ? current_val : val;
            }
            return current_val;
        };

        q.bind(1, getField("name", current.name));
        q.bind(2, getField("status", current.status));
        q.bind(3, getField("priority", current.priority));
        q.bind(4, getField("tags", current.tags));
        q.bind(5, getField("description", current.description));
        q.bind(6, getField("repo_path", current.repo_path));
        q.bind(7, slug);

        return q.exec() > 0;
    } catch (const std::exception& e) {
        utils::Logger::get_instance().error("update: " + std::string(e.what()));
        return false;
    }
}

bool ProjectDB::remove(const std::string& slug) {
    try {
        int id = getProjectId(slug);
        if (id < 0) return false;

        SQLite::Database db(dbPath_, SQLite::OPEN_READWRITE);

        // Delete files first (parameterized)
        SQLite::Statement delFiles(db, "DELETE FROM files WHERE project_id = ?");
        delFiles.bind(1, id);
        delFiles.exec();

        // Delete project (parameterized)
        SQLite::Statement delProject(db, "DELETE FROM projects WHERE id = ?");
        delProject.bind(1, id);
        return delProject.exec() > 0;
    } catch (const std::exception& e) {
        utils::Logger::get_instance().error("remove: " + std::string(e.what()));
        return false;
    }
}

// ==== Files ====

std::vector<FileInfo> ProjectDB::listFiles(const std::string& slug) {
    std::vector<FileInfo> out;
    int id = getProjectId(slug);
    if (id < 0) return out;

    try {
        SQLite::Database db(dbPath_, SQLite::OPEN_READONLY);
        SQLite::Statement q(db, "SELECT filename, size, updated_at FROM files WHERE project_id = ? ORDER BY filename");
        q.bind(1, id);

        while (q.executeStep()) {
            FileInfo f;
            f.filename = q.getColumn(0).getText();
            f.size = q.getColumn(1).getInt();
            f.updated_at = q.getColumn(2).getText();
            out.push_back(f);
        }
    } catch (const std::exception& e) {
        utils::Logger::get_instance().error("listFiles: " + std::string(e.what()));
    }

    return out;
}

std::vector<uint8_t> ProjectDB::getFile(const std::string& slug, const std::string& filename) {
    int id = getProjectId(slug);
    if (id < 0) return {};

    try {
        SQLite::Database db(dbPath_, SQLite::OPEN_READONLY);
        SQLite::Statement q(db, "SELECT content FROM files WHERE project_id = ? AND filename = ?");
        q.bind(1, id);
        q.bind(2, filename);

        if (q.executeStep()) {
            SQLite::Column col = q.getColumn(0);
            if (!col.isNull()) {
                const void* blob = col.getBlob();
                int sz = col.getBytes();
                return std::vector<uint8_t>(
                    static_cast<const uint8_t*>(blob),
                    static_cast<const uint8_t*>(blob) + sz
                );
            }
        }
    } catch (const std::exception& e) {
        utils::Logger::get_instance().error("getFile: " + std::string(e.what()));
    }

    return {};
}

bool ProjectDB::putFile(const std::string& slug, const std::string& filename,
                         const void* data, int size) {
    int id = getProjectId(slug);
    if (id < 0) return false;

    try {
        SQLite::Database db(dbPath_, SQLite::OPEN_READWRITE);
        SQLite::Statement q(db, R"(
            INSERT INTO files(project_id, filename, content, size)
            VALUES(?, ?, ?, ?)
            ON CONFLICT(project_id, filename)
            DO UPDATE SET content = excluded.content, size = excluded.size,
                         updated_at = CURRENT_TIMESTAMP
        )");
        q.bind(1, id);
        q.bind(2, filename);
        q.bind(3, data, size);
        q.bind(4, size);
        return q.exec() > 0;
    } catch (const std::exception& e) {
        utils::Logger::get_instance().error("putFile: " + std::string(e.what()));
        return false;
    }
}

bool ProjectDB::deleteFile(const std::string& slug, const std::string& filename) {
    int id = getProjectId(slug);
    if (id < 0) return false;

    try {
        SQLite::Database db(dbPath_, SQLite::OPEN_READWRITE);
        SQLite::Statement q(db, "DELETE FROM files WHERE project_id = ? AND filename = ?");
        q.bind(1, id);
        q.bind(2, filename);
        return q.exec() > 0;
    } catch (const std::exception& e) {
        utils::Logger::get_instance().error("deleteFile: " + std::string(e.what()));
        return false;
    }
}

bool ProjectDB::patchFile(const std::string& slug, const std::string& filename,
                           const json& patch) {
    // Get current content
    auto data = getFile(slug, filename);
    if (data.empty()) return false;

    std::string content(reinterpret_cast<char*>(data.data()), data.size());

    // Parse into lines
    std::vector<std::string> lines;
    std::istringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }

    // Get operation
    if (!patch.contains("op") || !patch["op"].is_string()) return false;
    std::string op = patch["op"].get<std::string>();

    std::string text;
    if (patch.contains("content") && patch["content"].is_string()) {
        text = patch["content"].get<std::string>();
    }

    int linenum = 0;
    if (patch.contains("line") && patch["line"].is_number_integer()) {
        linenum = patch["line"].get<int>();
    }

    bool ok = false;

    if (op == "replace_line" && linenum > 0 && linenum <= static_cast<int>(lines.size())) {
        lines[linenum - 1] = text;
        ok = true;
    } else if (op == "insert_line" && linenum >= 0 && linenum <= static_cast<int>(lines.size())) {
        lines.insert(lines.begin() + linenum, text);
        ok = true;
    } else if (op == "delete_line" && linenum > 0 && linenum <= static_cast<int>(lines.size())) {
        lines.erase(lines.begin() + (linenum - 1));
        ok = true;
    } else if (op == "append") {
        lines.push_back(text);
        ok = true;
    } else if (op == "find_replace") {
        if (!patch.contains("find") || !patch["find"].is_string()) return false;
        std::string find_str = patch["find"].get<std::string>();
        std::string with_str;
        if (patch.contains("with") && patch["with"].is_string()) {
            with_str = patch["with"].get<std::string>();
        }

        // Rejoin, find, replace
        std::string joined;
        for (size_t i = 0; i < lines.size(); i++) {
            if (i > 0) joined += "\n";
            joined += lines[i];
        }

        auto pos = joined.find(find_str);
        if (pos == std::string::npos) return false;

        joined.replace(pos, find_str.length(), with_str);

        // Re-split
        lines.clear();
        std::istringstream s2(joined);
        while (std::getline(s2, line)) {
            lines.push_back(line);
        }
        ok = true;
    }

    if (!ok) return false;

    // Rejoin and save
    std::string result;
    for (size_t i = 0; i < lines.size(); i++) {
        if (i > 0) result += "\n";
        result += lines[i];
    }
    result += "\n";

    return putFile(slug, filename, result.data(), static_cast<int>(result.size()));
}
