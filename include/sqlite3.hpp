#ifndef SSM_SQLITE3_HPP
#define SSM_SQLITE3_HPP

#include <filesystem>
#include <string>

#include "sqlite3.h"

namespace ssm_sqlite3 {

struct stmt_handle {
    stmt_handle() = default;
    explicit stmt_handle(sqlite3_stmt* s) : stmt(s) {}

    ~stmt_handle() {
        if (stmt != nullptr) sqlite3_finalize(stmt);
    }

    stmt_handle(const stmt_handle&) = delete;
    stmt_handle& operator=(const stmt_handle&) = delete;

    stmt_handle(stmt_handle&& other) noexcept : stmt(other.stmt) {
        other.stmt = nullptr;
    }

    stmt_handle& operator=(stmt_handle&& other) noexcept {
        if (this != &other) {
            if (stmt != nullptr) sqlite3_finalize(stmt);
            stmt = other.stmt;
            other.stmt = nullptr;
        }
        return *this;
    }

    [[nodiscard]] sqlite3_stmt* get() const {
        return stmt;
    }

    explicit operator bool() const {
        return stmt != nullptr;
    }

private:
    sqlite3_stmt* stmt = nullptr;
};

struct database {
    explicit database(const std::filesystem::path& db_path) {
        if (sqlite3_open_v2(db_path.string().c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
            db = nullptr;
        }
    }

    ~database() {
        sqlite3_close(db); // safe to call with nullptr
    }

    database(const database&) = delete;
    database& operator=(const database&) = delete;

    database(database&& other) noexcept : db(other.db) {
        other.db = nullptr;
    }

    database& operator=(database&& other) noexcept {
        if (this != &other) {
            sqlite3_close(db);
            db = other.db;
            other.db = nullptr;
        }
        return *this;
    }

    [[nodiscard]] bool ok() const {
        return db != nullptr;
    }

    [[nodiscard]] const char* errmsg() const {
        return db != nullptr ? sqlite3_errmsg(db) : "Database not opened";
    }

    bool exec(const char* sql) const {
        return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
    }

    static bool bind_text(sqlite3_stmt* stmt, const int index, const std::string& value) {
        return sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT) == SQLITE_OK;
    }

    [[nodiscard]] stmt_handle prepare(const char* sql) const {
        sqlite3_stmt* raw = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK) {
            return {};
        }
        return stmt_handle(raw);
    }

private:
    sqlite3* db = nullptr;
};

} // namespace ssm_sqlite3

#endif //SSM_SQLITE3_HPP
