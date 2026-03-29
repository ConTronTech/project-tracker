#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct FileInfo {
    std::string filename;
    int size = 0;
    std::string updated_at;

    json toJson() const;
};

struct Project {
    int id = 0;
    std::string slug;
    std::string name;
    std::string status;
    std::string priority;
    std::string tags;
    std::string description;
    std::string repo_path;
    std::string created_at;
    std::string updated_at;
    int file_count = 0;
    std::vector<FileInfo> files;

    // List view (no files array)
    json toJson() const;

    // Detail view (with files array)
    json toJsonFull() const;

    // Parse from JSON object
    static Project fromJson(const json& j);
};
