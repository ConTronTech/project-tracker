#include <crow.h>
#include <crow/middlewares/cors.h>
#include "core/config.h"
#include "db/database.h"
#include "http/middleware.h"
#include "routes/projects.h"
#include "routes/files.h"
#include "routes/webui.h"
#include "security/auth.h"
#include "security/rate_limiter.h"
#include "security/sanitizer.h"
#include "utils/logger.h"
#include "utils/string_utils.h"
#include <iostream>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>

static std::atomic<bool> shutdown_requested{false};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        shutdown_requested.store(true);
    }
}

int main(int argc, char* argv[]) {
    std::string config_file = "config.env";
    if (argc > 1) {
        config_file = argv[1];
    }

    try {
        // ============================================
        // 1. Load configuration
        // ============================================
        core::Config& config = core::Config::get_instance();
        config.load(config_file);

        // ============================================
        // 2. Initialize logger
        // ============================================
        std::string log_level_str = config.get("env", "LOG_LEVEL", "INFO");
        utils::LogLevel log_level = utils::LogLevel::INFO;
        if (log_level_str == "DEBUG") log_level = utils::LogLevel::DEBUG;
        else if (log_level_str == "WARN") log_level = utils::LogLevel::WARN;
        else if (log_level_str == "ERROR") log_level = utils::LogLevel::ERROR;

        utils::Logger::get_instance().init(log_level);
        utils::Logger::get_instance().info("===========================================");
        utils::Logger::get_instance().info("Project Tracker API v2 starting...");
        utils::Logger::get_instance().info("===========================================");

        // ============================================
        // 3. Initialize authentication
        // ============================================
        security::Auth auth;
        std::string apiKey;
        if (auto* k = std::getenv("PROJECT_API_KEY")) {
            apiKey = k;
        }
        auth.init(apiKey);
        auth.configSessions(
            config.get_int("env", "SESSION_MAX", 50),
            config.get_int("env", "SESSION_TIMEOUT", 86400),
            config.get_bool("env", "BIND_SESSION_TO_IP", true)
        );

        // ============================================
        // 4. Initialize rate limiter
        // ============================================
        security::RateLimiter rateLimiter;
        rateLimiter.init(
            config.get_bool("env", "RATE_LIMIT_ENABLED", true),
            config.get_int("env", "READ_RPM", 120),
            config.get_int("env", "WRITE_RPM", 30),
            config.get_int("env", "BURST_SIZE", 15),
            config.get_int("env", "BAN_THRESHOLD", 10),
            config.get_int("env", "BAN_DURATION", 600)
        );

        // Parse trusted proxies
        std::string proxyStr = config.get("env", "TRUSTED_PROXIES", "");
        if (!proxyStr.empty()) {
            auto proxies = utils::StringUtils::split(proxyStr, ',');
            for (auto& p : proxies) p = utils::StringUtils::trim(p);
            rateLimiter.setTrustedProxies(proxies);
        }

        // ============================================
        // 5. Load sanitizer config
        // ============================================
        std::string allowedExt = config.get("env", "ALLOWED_EXTENSIONS", "");
        if (!allowedExt.empty()) {
            security::Sanitizer::loadAllowedExtensions(allowedExt);
        }

        // ============================================
        // 6. Signal handlers
        // ============================================
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        // ============================================
        // 7. Create Crow app with middleware
        // ============================================
        crow::App<http::SecurityMiddleware> app;

        // Configure middleware
        http::SecurityMiddleware::rate_limiter = &rateLimiter;
        http::SecurityMiddleware::max_body_size = config.get_int("env", "MAX_BODY_SIZE", 10485760);

        // ============================================
        // 8. Configure CORS (locked down)
        // ============================================
        std::string corsStr = config.get("env", "CORS_ORIGINS", "");
        if (!corsStr.empty()) {
            auto origins = utils::StringUtils::split(corsStr, ',');
            for (auto& o : origins) o = utils::StringUtils::trim(o);
            http::setCorsOrigins(origins);
            utils::Logger::get_instance().info("CORS: " + std::to_string(origins.size()) + " origins allowed");
        } else {
            utils::Logger::get_instance().info("CORS: no origins configured (same-origin only)");
        }

        // ============================================
        // 9. Initialize database
        // ============================================
        std::string db_path = config.get("env", "DB_PATH", "/data/projects.db");
        ProjectDB db(db_path);

        // ============================================
        // 10. Register routes
        // ============================================
        routes::registerProjectRoutes(app, db, auth);
        routes::registerFileRoutes(app, db, auth);

        // WebUI routes (login, logout, static files, session management)
        std::string webuiDir = "webui";
        routes::registerWebUIRoutes(app, auth, webuiDir);

        // ============================================
        // 11. Print startup info
        // ============================================
        int port = config.get_int("env", "PORT", 8080);

        utils::Logger::get_instance().info("API @ http://0.0.0.0:" + std::to_string(port));
        utils::Logger::get_instance().info("Routes:");
        utils::Logger::get_instance().info("  GET    /api/projects              list/search");
        utils::Logger::get_instance().info("  GET    /api/projects/<slug>       detail");
        utils::Logger::get_instance().info("  POST   /api/projects              create *");
        utils::Logger::get_instance().info("  PUT    /api/projects/<slug>       update *");
        utils::Logger::get_instance().info("  DELETE /api/projects/<slug>       delete *");
        utils::Logger::get_instance().info("  GET    /api/projects/<slug>/files/<name>      read");
        utils::Logger::get_instance().info("  PUT    /api/projects/<slug>/files/<name>      write *");
        utils::Logger::get_instance().info("  DELETE /api/projects/<slug>/files/<name>      delete *");
        utils::Logger::get_instance().info("  PATCH  /api/projects/<slug>/files/<name>      edit *");
        utils::Logger::get_instance().info("  GET    /api/projects/<slug>/files/<name>/download");
        utils::Logger::get_instance().info("  (* = requires X-API-Key)");
        utils::Logger::get_instance().info("Press Ctrl+C to stop");

        // ============================================
        // 12. Start server
        // ============================================
        // Run Crow in a separate thread so we can handle shutdown
        std::thread server_thread([&]() {
            try {
                app.loglevel(crow::LogLevel::Warning);
                app.port(port).multithreaded().run();
            } catch (const std::exception& e) {
                utils::Logger::get_instance().error("Server error: " + std::string(e.what()));
            }
        });

        // Periodic cleanup loop
        while (!shutdown_requested.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Every 5 minutes, clean up rate limiter and sessions
            static int cleanup_counter = 0;
            cleanup_counter++;
            if (cleanup_counter >= 300) {
                cleanup_counter = 0;
                rateLimiter.cleanupOldClients();
                auth.cleanExpiredSessions();
            }
        }

        // ============================================
        // 13. Graceful shutdown
        // ============================================
        utils::Logger::get_instance().info("Shutting down...");
        app.stop();

        if (server_thread.joinable()) {
            server_thread.join();
        }

        utils::Logger::get_instance().info("Server stopped");
        utils::Logger::get_instance().info("===========================================");

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
