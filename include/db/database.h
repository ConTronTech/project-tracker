#pragma once

#include "db/models.h"
#include <string>
#include <vector>
#include <cstdint>
#include <shared_mutex>

class ProjectDB {
public:
    explicit ProjectDB(const std::string& dbPath);

    // Projects — ALL queries parameterized
    // Read operations take shared lock (concurrent reads OK)
    std::vector<Project> getAll(const std::string& search = "",
                                const std::string& status = "",
                                int limit = 0, int offset = 0);
    Project getBySlug(const std::string& slug);

    // Write operations take exclusive lock (serialized)
    bool create(const Project& p);
    bool update(const std::string& slug, const json& fields);
    bool remove(const std::string& slug);

    // Files — ALL queries parameterized
    // Read operations — shared lock
    std::vector<FileInfo> listFiles(const std::string& slug);
    std::vector<uint8_t> getFile(const std::string& slug, const std::string& filename);

    // Write operations — exclusive lock
    bool putFile(const std::string& slug, const std::string& filename,
                 const void* data, int size);
    bool deleteFile(const std::string& slug, const std::string& filename);

    // Read-modify-write — exclusive lock for entire operation (atomic)
    bool patchFile(const std::string& slug, const std::string& filename,
                   const json& patch);

private:
    std::string dbPath_;

    // Read-many/write-one lock — the foundation pattern
    // Reads: std::shared_lock(db_mutex_) — concurrent, non-blocking
    // Writes: std::unique_lock(db_mutex_) — exclusive, serialized
    mutable std::shared_mutex db_mutex_;

    // Get project ID by slug (parameterized) — caller must hold lock
    int getProjectIdUnlocked(const std::string& slug);
};
