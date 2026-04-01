#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <atomic>

namespace security {

struct Session {
    std::string token;
    std::string client_ip;
    std::chrono::steady_clock::time_point created;
    std::atomic<std::chrono::steady_clock::time_point::rep> last_activity_raw;
    bool authenticated;

    Session() : authenticated(false) {
        last_activity_raw.store(
            std::chrono::steady_clock::now().time_since_epoch().count(),
            std::memory_order_relaxed
        );
    }

    // Convenience: get last_activity as time_point
    std::chrono::steady_clock::time_point lastActivity() const {
        return std::chrono::steady_clock::time_point(
            std::chrono::steady_clock::duration(
                last_activity_raw.load(std::memory_order_relaxed)
            )
        );
    }

    // Convenience: touch last activity — thread-safe without mutex
    void touch() {
        last_activity_raw.store(
            std::chrono::steady_clock::now().time_since_epoch().count(),
            std::memory_order_relaxed
        );
    }
};

class Auth {
private:
    std::string api_key_;
    bool auth_enabled_;
    std::unordered_map<std::string, Session> sessions_;  // O(1) lookup
    mutable std::mutex session_mutex_;
    size_t max_sessions_;
    std::chrono::seconds session_timeout_;
    bool bind_session_to_ip_;

    // Constant-time string comparison
    static bool constantTimeCompare(const std::string& a, const std::string& b);

    // Generate cryptographically random token
    static std::string generateToken();

    // Clean expired sessions (no locking — caller must hold mutex)
    void cleanExpiredSessionsUnlocked();

public:
    Auth();

    // Initialize with API key
    void init(const std::string& apiKey);

    // Configure session settings
    void configSessions(size_t maxSessions, int timeoutSec, bool bindToIp);

    // Check if auth is enabled
    bool isEnabled() const { return auth_enabled_; }

    // Validate API key (constant-time)
    bool validateApiKey(const std::string& provided) const;

    // Session management — no more const_cast UB
    std::string createSession(const std::string& clientIp);
    bool validateSession(const std::string& token, const std::string& clientIp);  // NOT const
    void destroySession(const std::string& token);
    void cleanExpiredSessions();

    // Get active session count
    size_t getSessionCount() const;

    // Check if request is authorized (API key OR valid session cookie)
    bool isAuthorized(const std::string& apiKeyHeader,
                      const std::string& cookieHeader,
                      const std::string& clientIp);  // NOT const (validates session)

    // Extract session token from Cookie header
    static std::string extractSessionToken(const std::string& cookieHeader);
};

} // namespace security
