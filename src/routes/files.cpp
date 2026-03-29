#include "routes/files.h"
#include "security/sanitizer.h"
#include "security/validator.h"
#include "http/middleware.h"
#include "utils/logger.h"
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

namespace routes {

void registerFileRoutes(
    crow::App<http::SecurityMiddleware>& app,
    ProjectDB& db,
    security::Auth& auth
) {

    // GET /api/projects/<slug>/files/<filename> — read file
    CROW_ROUTE(app, "/api/projects/<string>/files/<string>").methods(crow::HTTPMethod::GET)
    ([&](const crow::request& req, const std::string& slug, const std::string& filename) {
        // Validate slug
        if (!security::Sanitizer::isValidSlug(slug)) {
            return http::jsonError(400, "invalid slug");
        }

        // Validate filename (relaxed — allow reading any existing file)
        if (filename.empty() || filename.length() > 128 ||
            security::Sanitizer::containsNullBytes(filename) ||
            filename.find("..") != std::string::npos ||
            filename.find('/') != std::string::npos) {
            return http::jsonError(400, "invalid filename");
        }

        auto data = db.getFile(slug, filename);
        if (data.empty()) {
            return http::jsonError(404, "file not found");
        }

        std::string content(reinterpret_cast<char*>(data.data()), data.size());

        // Range support (line-based)
        const char* from_p = req.url_params.get("from");
        const char* to_p = req.url_params.get("to");

        if (from_p || to_p) {
            std::vector<std::string> lines;
            std::istringstream ss(content);
            std::string line;
            while (std::getline(ss, line)) lines.push_back(line);

            int from = security::Validator::safeParamInt(from_p, 1, 1, static_cast<int>(lines.size()));
            int to = security::Validator::safeParamInt(to_p, static_cast<int>(lines.size()), 1, static_cast<int>(lines.size()));

            std::string result;
            for (int i = from - 1; i < to && i < static_cast<int>(lines.size()); i++) {
                result += lines[i] + "\n";
            }

            auto res = crow::response(result);
            res.add_header("Content-Type", "text/plain; charset=utf-8");
            res.add_header("X-Lines-Total", std::to_string(lines.size()));
            return res;
        }

        auto res = crow::response(content);
        res.add_header("Content-Type", "text/plain; charset=utf-8");
        return res;
    });

    // PUT /api/projects/<slug>/files/<filename> — write/overwrite file
    CROW_ROUTE(app, "/api/projects/<string>/files/<string>").methods(crow::HTTPMethod::PUT)
    ([&](const crow::request& req, const std::string& slug, const std::string& filename) {
        // Auth
        if (!auth.isAuthorized(req.get_header_value("X-API-Key"), req.get_header_value("Cookie"), req.remote_ip_address)) {
            return http::jsonError(401, "unauthorized");
        }

        // Validate slug
        if (!security::Sanitizer::isValidSlug(slug)) {
            return http::jsonError(400, "invalid slug");
        }

        // Validate filename (strict — must pass full validation for writes)
        if (!security::Sanitizer::isValidFilename(filename)) {
            return http::jsonError(400, "invalid filename");
        }

        // Body size check (10MB max for files)
        if (!security::Validator::isBodySizeValid(req.body.size(), 10 * 1024 * 1024)) {
            return http::jsonError(413, "file too large");
        }

        if (db.putFile(slug, filename, req.body.data(), static_cast<int>(req.body.size()))) {
            json result = {
                {"success", true},
                {"filename", filename},
                {"size", req.body.size()}
            };
            return http::jsonSuccess(result.dump());
        }
        return http::jsonError(500, "write failed");
    });

    // DELETE /api/projects/<slug>/files/<filename> — delete file
    CROW_ROUTE(app, "/api/projects/<string>/files/<string>").methods(crow::HTTPMethod::DELETE)
    ([&](const crow::request& req, const std::string& slug, const std::string& filename) {
        // Auth
        if (!auth.isAuthorized(req.get_header_value("X-API-Key"), req.get_header_value("Cookie"), req.remote_ip_address)) {
            return http::jsonError(401, "unauthorized");
        }

        // Validate slug
        if (!security::Sanitizer::isValidSlug(slug)) {
            return http::jsonError(400, "invalid slug");
        }

        // Validate filename
        if (filename.empty() || filename.length() > 128 ||
            security::Sanitizer::containsNullBytes(filename) ||
            filename.find("..") != std::string::npos) {
            return http::jsonError(400, "invalid filename");
        }

        if (db.deleteFile(slug, filename)) {
            return http::jsonSuccess("{\"success\":true}");
        }
        return http::jsonError(500, "delete failed");
    });

    // PATCH /api/projects/<slug>/files/<filename> — edit file
    CROW_ROUTE(app, "/api/projects/<string>/files/<string>").methods(crow::HTTPMethod::PATCH)
    ([&](const crow::request& req, const std::string& slug, const std::string& filename) {
        // Auth
        if (!auth.isAuthorized(req.get_header_value("X-API-Key"), req.get_header_value("Cookie"), req.remote_ip_address)) {
            return http::jsonError(401, "unauthorized");
        }

        // Validate slug
        if (!security::Sanitizer::isValidSlug(slug)) {
            return http::jsonError(400, "invalid slug");
        }

        // Validate filename
        if (filename.empty() || filename.length() > 128 ||
            security::Sanitizer::containsNullBytes(filename) ||
            filename.find("..") != std::string::npos) {
            return http::jsonError(400, "invalid filename");
        }

        // Body size
        if (!security::Validator::isBodySizeValid(req.body.size(), 65536)) {
            return http::jsonError(413, "patch body too large");
        }

        // Parse JSON
        auto body = json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            return http::jsonError(400, "invalid json");
        }

        if (!body.contains("op") || !body["op"].is_string()) {
            return http::jsonError(400, "op required");
        }

        if (db.patchFile(slug, filename, body)) {
            // Get updated file info
            auto data = db.getFile(slug, filename);
            json result = {
                {"success", true},
                {"size", data.size()}
            };
            return http::jsonSuccess(result.dump());
        }

        // Differentiate between not found and invalid op
        auto data = db.getFile(slug, filename);
        if (data.empty()) {
            return http::jsonError(404, "file not found");
        }
        return http::jsonError(400, "patch failed");
    });

    // GET /api/projects/<slug>/files/<filename>/download — force download
    CROW_ROUTE(app, "/api/projects/<string>/files/<string>/download").methods(crow::HTTPMethod::GET)
    ([&](const crow::request& req, const std::string& slug, const std::string& filename) {
        // Validate slug
        if (!security::Sanitizer::isValidSlug(slug)) {
            return http::jsonError(400, "invalid slug");
        }

        // Validate filename
        if (filename.empty() || filename.length() > 128 ||
            security::Sanitizer::containsNullBytes(filename) ||
            filename.find("..") != std::string::npos) {
            return http::jsonError(400, "invalid filename");
        }

        auto data = db.getFile(slug, filename);
        if (data.empty()) {
            return http::jsonError(404, "file not found");
        }

        // Sanitize filename for Content-Disposition
        std::string safeName = security::Sanitizer::safeDispositionName(filename);

        auto res = crow::response(std::string(reinterpret_cast<char*>(data.data()), data.size()));
        res.add_header("Content-Type", "application/octet-stream");
        res.add_header("Content-Disposition", "attachment; filename=\"" + safeName + "\"");
        return res;
    });
}

} // namespace routes
