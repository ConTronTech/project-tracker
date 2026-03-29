#include "db/models.h"

// ---- FileInfo ----

json FileInfo::toJson() const {
    return {
        {"filename", filename},
        {"size", size},
        {"updated_at", updated_at}
    };
}

// ---- Project (list view) ----

json Project::toJson() const {
    return {
        {"id", id},
        {"slug", slug},
        {"name", name},
        {"status", status},
        {"priority", priority},
        {"tags", tags},
        {"description", description},
        {"file_count", file_count}
    };
}

// ---- Project (detail view with files) ----

json Project::toJsonFull() const {
    json filesArray = json::array();
    for (const auto& f : files) {
        filesArray.push_back(f.toJson());
    }

    return {
        {"id", id},
        {"slug", slug},
        {"name", name},
        {"status", status},
        {"priority", priority},
        {"tags", tags},
        {"description", description},
        {"repo_path", repo_path},
        {"created_at", created_at},
        {"updated_at", updated_at},
        {"files", filesArray}
    };
}

// ---- Parse from JSON ----

Project Project::fromJson(const json& j) {
    Project p;

    if (j.contains("slug") && j["slug"].is_string())
        p.slug = j["slug"].get<std::string>();
    if (j.contains("name") && j["name"].is_string())
        p.name = j["name"].get<std::string>();
    if (j.contains("status") && j["status"].is_string())
        p.status = j["status"].get<std::string>();
    if (j.contains("priority") && j["priority"].is_string())
        p.priority = j["priority"].get<std::string>();
    if (j.contains("tags") && j["tags"].is_string())
        p.tags = j["tags"].get<std::string>();
    if (j.contains("description") && j["description"].is_string())
        p.description = j["description"].get<std::string>();
    if (j.contains("repo_path") && j["repo_path"].is_string())
        p.repo_path = j["repo_path"].get<std::string>();

    return p;
}
