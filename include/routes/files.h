#pragma once

#include <crow.h>
#include "http/middleware.h"
#include "db/database.h"
#include "security/auth.h"

namespace routes {

// Register all file routes on the Crow app
void registerFileRoutes(
    crow::App<http::SecurityMiddleware>& app,
    ProjectDB& db,
    security::Auth& auth
);

} // namespace routes
