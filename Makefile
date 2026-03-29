# Project Tracker API v2 - Makefile

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread \
           -Iinclude \
           -Ilibs \
           -Ilibs/SQLiteCpp/include \
           -Ilibs/SQLiteCpp/sqlite3

LDFLAGS = -lpthread

# SQLiteCpp sources
SQLITE_SOURCES = libs/SQLiteCpp/src/Column.cpp \
                 libs/SQLiteCpp/src/Database.cpp \
                 libs/SQLiteCpp/src/Statement.cpp \
                 libs/SQLiteCpp/src/Transaction.cpp \
                 libs/SQLiteCpp/src/Exception.cpp \
                 libs/SQLiteCpp/src/Backup.cpp \
                 libs/SQLiteCpp/src/Savepoint.cpp \
                 libs/SQLiteCpp/sqlite3/sqlite3.c

# Project sources
PROJECT_SOURCES = src/main.cpp \
                  src/core/config.cpp \
                  src/db/database.cpp \
                  src/db/models.cpp \
                  src/http/middleware.cpp \
                  src/routes/projects.cpp \
                  src/routes/files.cpp \
                  src/routes/webui.cpp \
                  src/security/auth.cpp \
                  src/security/rate_limiter.cpp \
                  src/security/sanitizer.cpp \
                  src/security/validator.cpp \
                  src/utils/logger.cpp \
                  src/utils/string_utils.cpp

# Object files
PROJECT_OBJECTS = $(PROJECT_SOURCES:.cpp=.o)
SQLITE_CPP_OBJECTS = $(filter %.o,$(SQLITE_SOURCES:.cpp=.o))
SQLITE_C_OBJECTS = $(filter %.o,$(SQLITE_SOURCES:.c=.o))

TARGET = project_api

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(PROJECT_OBJECTS) $(SQLITE_CPP_OBJECTS) $(SQLITE_C_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $^ $(LDFLAGS)

# Compile C++ sources
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile C sources (sqlite3.c)
libs/SQLiteCpp/sqlite3/sqlite3.o: libs/SQLiteCpp/sqlite3/sqlite3.c
	gcc -std=c11 -O2 -DSQLITE_ENABLE_FTS5 -DSQLITE_ENABLE_JSON1 -c $< -o $@

clean:
	find src -name "*.o" -delete
	find libs/SQLiteCpp -name "*.o" -delete
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
