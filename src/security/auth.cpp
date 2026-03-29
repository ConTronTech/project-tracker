#include "security/auth.h"
#include "utils/logger.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace security {

Auth::Auth()
    : auth_enabled_(false)
    , max_sessions_(50)
    , session_timeout_(86400)
    , bind_session_to_ip_(true) {
}

void Auth::init(const std::string& apiKey) {
    api_key_ = apiKey;
    auth_enabled_ = !apiKey.empty();
    if (auth_enabled_) {
        utils::Logger::get_instance().info("API key authentication enabled");
    } else {
        utils::Logger::get_instance().warn("API key authentication DISABLED — no key provided");
    }
}

void Auth::configSessions(size_t maxSessions, int timeoutSec, bool bindToIp) {
    max_sessions_ = maxSessions;
    session_timeout_ = std::chrono::seconds(timeoutSec);
    bind_session_to_ip_ = bindToIp;
    utils::Logger::get_instance().info("Sessions: max=" + std::to_string(maxSessions) +
        ", timeout=" + std::to_string(timeoutSec) + "s, ip_bind=" +
        (bindToIp ? "true" : "false"));
}

bool Auth::constantTimeCompare(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;

    volatile unsigned char result = 0;
    for (size_t i = 0; i < a.size(); i++) {
        result |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return result == 0;
}

std::string Auth::generateToken() {
    // Try /dev/urandom first (crypto-quality)
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    unsigned char bytes[32];

    if (urandom.is_open()) {
        urandom.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
        urandom.close();
    } else {
        // Fallback to mt19937 (not crypto-quality but better than nothing)
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<unsigned char> dist(0, 255);
        for (auto& b : bytes) {
            b = dist(gen);
        }
        utils::Logger::get_instance().warn("Using non-crypto PRNG for session token — /dev/urandom unavailable");
    }

    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (auto b : bytes) {
        hex << std::setw(2) << static_cast<int>(b);
    }
    return hex.str();
}

bool Auth::validateApiKey(const std::string& provided) const {
    if (!auth_enabled_) return true;
    if (provided.empty()) return false;
    return constantTimeCompare(provided, api_key_);
}

std::string Auth::createSession(const std::string& clientIp) {
    std::lock_guard<std::mutex> lock(session_mutex_);

    // Enforce max sessions
    if (sessions_.size() >= max_sessions_) {
        cleanExpiredSessions();
        // If still at max, reject
        if (sessions_.size() >= max_sessions_) {
            utils::Logger::get_instance().warn("Max sessions reached, rejecting new session from " + clientIp);
            return "";
        }
    }

    Session session;
    session.token = generateToken();
    session.client_ip = clientIp;
    session.created = std::chrono::steady_clock::now();
    session.last_activity = session.created;
    session.authenticated = true;

    sessions_[session.token] = session;

    utils::Logger::get_instance().info("Session created for " + clientIp +
        " (active: " + std::to_string(sessions_.size()) + ")");

    return session.token;
}

bool Auth::validateSession(const std::string& token, const std::string& clientIp) const {
    if (token.empty()) return false;

    std::lock_guard<std::mutex> lock(session_mutex_);

    auto it = sessions_.find(token);
    if (it == sessions_.end()) return false;

    const Session& session = it->second;

    // Check expiry
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - session.last_activity);
    if (age > session_timeout_) {
        return false;
    }

    // Check IP binding
    if (bind_session_to_ip_ && session.client_ip != clientIp) {
        utils::Logger::get_instance().warn("Session IP mismatch: expected " +
            session.client_ip + ", got " + clientIp);
        return false;
    }

    // Update last activity (const_cast because this is a read-path side effect)
    const_cast<Session&>(session).last_activity = now;

    return session.authenticated;
}

void Auth::destroySession(const std::string& token) {
    std::lock_guard<std::mutex> lock(session_mutex_);

    auto it = sessions_.find(token);
    if (it != sessions_.end()) {
        utils::Logger::get_instance().info("Session destroyed for " + it->second.client_ip);
        sessions_.erase(it);
    }
}

void Auth::cleanExpiredSessions() {
    // Note: caller must hold mutex if called internally
    auto now = std::chrono::steady_clock::now();
    size_t removed = 0;

    for (auto it = sessions_.begin(); it != sessions_.end();) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.last_activity);
        if (age > session_timeout_) {
            it = sessions_.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        utils::Logger::get_instance().info("Cleaned " + std::to_string(removed) +
            " expired sessions (active: " + std::to_string(sessions_.size()) + ")");
    }
}

size_t Auth::getSessionCount() const {
    std::lock_guard<std::mutex> lock(session_mutex_);
    return sessions_.size();
}

std::string Auth::extractSessionToken(const std::string& cookieHeader) {
    if (cookieHeader.empty()) return "";
    std::string prefix = "session=";
    auto pos = cookieHeader.find(prefix);
    if (pos == std::string::npos) return "";
    pos += prefix.length();
    auto end = cookieHeader.find(';', pos);
    if (end == std::string::npos) end = cookieHeader.length();
    return cookieHeader.substr(pos, end - pos);
}

bool Auth::isAuthorized(const std::string& apiKeyHeader,
                        const std::string& cookieHeader,
                        const std::string& clientIp) const {
    // Method 1: API key header (for agents/scripts)
    if (!apiKeyHeader.empty() && validateApiKey(apiKeyHeader)) {
        return true;
    }
    // Method 2: Session cookie (for WebUI)
    std::string token = extractSessionToken(cookieHeader);
    if (!token.empty() && validateSession(token, clientIp)) {
        return true;
    }
    return false;
}

} // namespace security
