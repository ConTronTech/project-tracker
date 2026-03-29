#pragma once

#include <crow.h>
#include "http/middleware.h"
#include "security/auth.h"

namespace routes {

// Register WebUI routes (login, logout, static files, API proxy)
void registerWebUIRoutes(
    crow::App<http::SecurityMiddleware>& app,
    security::Auth& auth,
    const std::string& webuiDir
);

} // namespace routes
