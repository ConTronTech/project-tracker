#include "routes/projects.h"
#include "security/sanitizer.h"
#include "security/validator.h"
#include "http/middleware.h"
#include "utils/logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace routes {

void registerProjectRoutes(
    crow::App<http::SecurityMiddleware>& app,
    ProjectDB& db,
    security::Auth& auth
) {

    // GET /api/projects — list/search
    CROW_ROUTE(app, "/api/projects").methods(crow::HTTPMethod::GET)
    ([&](const crow::request& req) {
        // Sanitize query params
        std::string search = security::Validator::safeParamString(
            req.url_params.get("search"), 256);
        std::string status = security::Validator::safeParamString(
            req.url_params.get("status"), 32);
        int limit = security::Validator::safeParamInt(
            req.url_params.get("limit"), 0, 0, 1000);
        int offset = security::Validator::safeParamInt(
            req.url_params.get("offset"), 0, 0, 100000);

        // Validate status if provided
        if (!status.empty() && !security::Sanitizer::isValidStatus(status)) {
            return http::jsonError(400, "invalid status");
        }

        // Check search string is clean
        if (!search.empty() && !security::Sanitizer::isCleanString(search, 256)) {
            return http::jsonError(400, "invalid search query");
        }

        auto list = db.getAll(search, status, limit, offset);

        json arr = json::array();
        for (const auto& p : list) {
            arr.push_back(p.toJson());
        }

        auto res = crow::response(arr.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("X-Total-Count", std::to_string(list.size()));
        return res;
    });

    // GET /api/projects/<slug> — detail
    CROW_ROUTE(app, "/api/projects/<string>").methods(crow::HTTPMethod::GET)
    ([&](const std::string& slug) {
        // Validate slug
        if (!security::Sanitizer::isValidSlug(slug)) {
            return http::jsonError(400, "invalid slug");
        }

        auto p = db.getBySlug(slug);
        if (p.slug.empty()) {
            return http::jsonError(404, "not found");
        }

        auto res = crow::response(p.toJsonFull().dump());
        res.add_header("Content-Type", "application/json");
        return res;
    });

    // POST /api/projects — create
    CROW_ROUTE(app, "/api/projects").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req) {
        // Auth
        if (!auth.isAuthorized(req.get_header_value("X-API-Key"), req.get_header_value("Cookie"), req.remote_ip_address)) {
            return http::jsonError(401, "unauthorized");
        }

        // Body size
        if (!security::Validator::isBodySizeValid(req.body.size(), 65536)) {
            return http::jsonError(413, "body too large");
        }

        // Parse JSON
        auto body = json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            return http::jsonError(400, "invalid json");
        }

        // Validate slug
        if (!body.contains("slug") || !body["slug"].is_string()) {
            return http::jsonError(400, "slug required");
        }
        std::string slug = body["slug"].get<std::string>();
        if (!security::Sanitizer::isValidSlug(slug)) {
            return http::jsonError(400, "invalid slug format");
        }

        // Validate optional fields
        if (body.contains("status") && body["status"].is_string()) {
            if (!security::Sanitizer::isValidStatus(body["status"].get<std::string>())) {
                return http::jsonError(400, "invalid status");
            }
        }
        if (body.contains("priority") && body["priority"].is_string()) {
            if (!security::Sanitizer::isValidPriority(body["priority"].get<std::string>())) {
                return http::jsonError(400, "invalid priority");
            }
        }

        // Check string fields are clean
        for (const auto& field : {"name", "tags", "description", "repo_path"}) {
            if (body.contains(field) && body[field].is_string()) {
                if (!security::Sanitizer::isCleanString(body[field].get<std::string>(), 1024)) {
                    return http::jsonError(400, std::string("invalid ") + field);
                }
            }
        }

        Project p = Project::fromJson(body);
        if (db.create(p)) {
            return http::jsonSuccess("{\"success\":true}");
        }
        return http::jsonError(500, "create failed");
    });

    // PUT /api/projects/<slug> — update
    CROW_ROUTE(app, "/api/projects/<string>").methods(crow::HTTPMethod::PUT)
    ([&](const crow::request& req, const std::string& slug) {
        // Auth
        if (!auth.isAuthorized(req.get_header_value("X-API-Key"), req.get_header_value("Cookie"), req.remote_ip_address)) {
            return http::jsonError(401, "unauthorized");
        }

        // Validate slug
        if (!security::Sanitizer::isValidSlug(slug)) {
            return http::jsonError(400, "invalid slug");
        }

        // Body size
        if (!security::Validator::isBodySizeValid(req.body.size(), 65536)) {
            return http::jsonError(413, "body too large");
        }

        // Parse JSON
        auto body = json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            return http::jsonError(400, "invalid json");
        }

        // Validate fields if present
        if (body.contains("status") && body["status"].is_string()) {
            if (!security::Sanitizer::isValidStatus(body["status"].get<std::string>())) {
                return http::jsonError(400, "invalid status");
            }
        }
        if (body.contains("priority") && body["priority"].is_string()) {
            if (!security::Sanitizer::isValidPriority(body["priority"].get<std::string>())) {
                return http::jsonError(400, "invalid priority");
            }
        }

        for (const auto& field : {"name", "tags", "description", "repo_path"}) {
            if (body.contains(field) && body[field].is_string()) {
                if (!security::Sanitizer::isCleanString(body[field].get<std::string>(), 1024)) {
                    return http::jsonError(400, std::string("invalid ") + field);
                }
            }
        }

        if (db.update(slug, body)) {
            return http::jsonSuccess("{\"success\":true}");
        }
        return http::jsonError(500, "update failed");
    });

    // DELETE /api/projects/<slug> — delete
    CROW_ROUTE(app, "/api/projects/<string>").methods(crow::HTTPMethod::DELETE)
    ([&](const crow::request& req, const std::string& slug) {
        // Auth
        if (!auth.isAuthorized(req.get_header_value("X-API-Key"), req.get_header_value("Cookie"), req.remote_ip_address)) {
            return http::jsonError(401, "unauthorized");
        }

        // Validate slug
        if (!security::Sanitizer::isValidSlug(slug)) {
            return http::jsonError(400, "invalid slug");
        }

        if (db.remove(slug)) {
            return http::jsonSuccess("{\"success\":true}");
        }
        return http::jsonError(500, "delete failed");
    });
}

} // namespace routes
