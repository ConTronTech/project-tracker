#pragma once

#include <crow.h>
#include "http/middleware.h"
#include "db/database.h"
#include "security/auth.h"

namespace routes {

// Register all project routes on the Crow app
void registerProjectRoutes(
    crow::App<http::SecurityMiddleware>& app,
    ProjectDB& db,
    security::Auth& auth
);

} // namespace routes
