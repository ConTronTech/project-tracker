#pragma once

#include "db/models.h"
#include <string>
#include <vector>
#include <cstdint>

class ProjectDB {
public:
    explicit ProjectDB(const std::string& dbPath);

    // Projects — ALL queries parameterized
    std::vector<Project> getAll(const std::string& search = "",
                                const std::string& status = "",
                                int limit = 0, int offset = 0);
    Project getBySlug(const std::string& slug);
    bool create(const Project& p);
    bool update(const std::string& slug, const json& fields);
    bool remove(const std::string& slug);

    // Files — ALL queries parameterized
    std::vector<FileInfo> listFiles(const std::string& slug);
    std::vector<uint8_t> getFile(const std::string& slug, const std::string& filename);
    bool putFile(const std::string& slug, const std::string& filename,
                 const void* data, int size);
    bool deleteFile(const std::string& slug, const std::string& filename);
    bool patchFile(const std::string& slug, const std::string& filename,
                   const json& patch);

private:
    std::string dbPath_;

    // Get project ID by slug (parameterized)
    int getProjectId(const std::string& slug);
};
