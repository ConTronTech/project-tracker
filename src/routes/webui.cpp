#include "routes/webui.h"
#include "security/sanitizer.h"
#include "utils/logger.h"
#include "http/middleware.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>

using json = nlohmann::json;

namespace routes {

// Helper to read a file from disk
static std::string readStaticFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "";
    std::ostringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

// Extract session token from cookie header
static std::string getSessionToken(const crow::request& req) {
    return security::Auth::extractSessionToken(req.get_header_value("Cookie"));
}

void registerWebUIRoutes(
    crow::App<http::SecurityMiddleware>& app,
    security::Auth& auth,
    const std::string& webuiDir
) {
    // ===== Login page =====
    CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::GET)
    ([&webuiDir](const crow::request& /* req */) {
        std::string content = readStaticFile(webuiDir + "/html/login.html");
        if (content.empty()) return http::jsonError(500, "login page not found");
        auto res = crow::response(content);
        res.add_header("Content-Type", "text/html; charset=utf-8");
        return res;
    });

    // ===== Login endpoint =====
    CROW_ROUTE(app, "/webui/login").methods(crow::HTTPMethod::POST)
    ([&auth](const crow::request& req) {
        auto body = json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("key") || !body["key"].is_string()) {
            return http::jsonError(400, "invalid request");
        }

        std::string key = body["key"].get<std::string>();
        if (!auth.validateApiKey(key)) {
            utils::Logger::get_instance().warn("Failed login attempt from " + req.remote_ip_address);
            return http::jsonError(401, "invalid key");
        }

        // Create session
        std::string token = auth.createSession(req.remote_ip_address);
        if (token.empty()) {
            return http::jsonError(503, "too many sessions");
        }

        auto res = crow::response(200);
        res.add_header("Content-Type", "application/json");
        res.add_header("Set-Cookie", "session=" + token + 
            "; HttpOnly; SameSite=Strict; Path=/; Max-Age=86400");
        res.write("{\"success\":true}");
        return res;
    });

    // ===== Logout endpoint =====
    CROW_ROUTE(app, "/webui/logout").methods(crow::HTTPMethod::POST)
    ([&auth](const crow::request& req) {
        std::string token = getSessionToken(req);
        if (!token.empty()) {
            auth.destroySession(token);
        }
        auto res = crow::response(200);
        res.add_header("Set-Cookie", "session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0");
        res.add_header("Content-Type", "application/json");
        res.write("{\"success\":true}");
        return res;
    });

    // ===== Main app (requires session) =====
    CROW_ROUTE(app, "/").methods(crow::HTTPMethod::GET)
    ([&auth, &webuiDir](const crow::request& req) {
        std::string token = getSessionToken(req);
        if (!auth.validateSession(token, req.remote_ip_address)) {
            // Redirect to login
            auto res = crow::response(302);
            res.add_header("Location", "/login");
            return res;
        }

        std::string content = readStaticFile(webuiDir + "/html/index.html");
        if (content.empty()) return http::jsonError(500, "app not found");
        auto res = crow::response(content);
        res.add_header("Content-Type", "text/html; charset=utf-8");
        return res;
    });

    // ===== Static files — explicit routes for CSS and JS =====
    // Using explicit two-segment routes to avoid Crow <path> conflicts

    CROW_ROUTE(app, "/static/css/<string>").methods(crow::HTTPMethod::GET)
    ([&webuiDir](const std::string& filename) {
        if (filename.find("..") != std::string::npos || filename.find('/') != std::string::npos) {
            return http::jsonError(400, "invalid path");
        }
        namespace fs = std::filesystem;
        try {
            fs::path base = fs::canonical(webuiDir);
            std::string fullPath = webuiDir + "/css/" + filename;
            fs::path requested = fs::canonical(fullPath);
            if (requested.string().find(base.string()) != 0) {
                return http::jsonError(403, "forbidden");
            }
            std::string content = readStaticFile(requested.string());
            if (content.empty()) return http::jsonError(404, "not found");
            auto res = crow::response(content);
            res.add_header("Content-Type", "text/css");
            res.add_header("Cache-Control", "public, max-age=3600");
            return res;
        } catch (...) { return http::jsonError(404, "not found"); }
    });

    CROW_ROUTE(app, "/static/js/<string>").methods(crow::HTTPMethod::GET)
    ([&webuiDir](const std::string& filename) {
        if (filename.find("..") != std::string::npos || filename.find('/') != std::string::npos) {
            return http::jsonError(400, "invalid path");
        }
        namespace fs = std::filesystem;
        try {
            fs::path base = fs::canonical(webuiDir);
            std::string fullPath = webuiDir + "/js/" + filename;
            fs::path requested = fs::canonical(fullPath);
            if (requested.string().find(base.string()) != 0) {
                return http::jsonError(403, "forbidden");
            }
            std::string content = readStaticFile(requested.string());
            if (content.empty()) return http::jsonError(404, "not found");
            auto res = crow::response(content);
            res.add_header("Content-Type", "application/javascript");
            res.add_header("Cache-Control", "public, max-age=3600");
            return res;
        } catch (...) { return http::jsonError(404, "not found"); }
    });

    // API proxy not needed — JS calls /api/* directly
    // Session cookie auth is handled in route handlers via auth.isAuthorized()
}

} // namespace routes
