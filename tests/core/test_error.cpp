// void_core Error and Result tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/core/error.hpp>
#include <string>

using namespace void_core;

// =============================================================================
// Error Tests
// =============================================================================

TEST_CASE("Error construction", "[core][error]") {
    SECTION("from string") {
        Error err("Test error");
        REQUIRE(err.message() == "Test error");
        REQUIRE(err.code() == ErrorCode::Unknown);
    }

    SECTION("from code and message") {
        Error err(ErrorCode::InvalidArgument, "Bad argument");
        REQUIRE(err.code() == ErrorCode::InvalidArgument);
        REQUIRE(err.message() == "Bad argument");
    }

    SECTION("with context") {
        Error err = Error("Base error").with_context("key", "value");
        auto* ctx = err.get_context("key");
        REQUIRE(ctx != nullptr);
        REQUIRE(*ctx == "value");
    }
}

TEST_CASE("Error factory methods", "[core][error]") {
    SECTION("PluginError::not_found") {
        Error err = PluginError::not_found("test_plugin");
        REQUIRE(err.code() == ErrorCode::NotFound);
        REQUIRE(err.message().find("test_plugin") != std::string::npos);
    }

    SECTION("PluginError::already_registered") {
        Error err = PluginError::already_registered("test_plugin");
        REQUIRE(err.code() == ErrorCode::AlreadyExists);
    }

    SECTION("HandleError::null") {
        Error err = HandleError::null();
        REQUIRE(err.code() == ErrorCode::InvalidArgument);
    }

    SECTION("HandleError::stale") {
        Error err = HandleError::stale();
        REQUIRE(err.code() == ErrorCode::InvalidState);
    }

    SECTION("HotReloadError::incompatible_version") {
        Error err = HotReloadError::incompatible_version("1.0.0", "2.0.0");
        REQUIRE(err.code() == ErrorCode::IncompatibleVersion);
    }
}

// =============================================================================
// Result<T> Tests
// =============================================================================

TEST_CASE("Result construction", "[core][result]") {
    SECTION("Ok with value") {
        Result<int> r = Ok(42);
        REQUIRE(r.is_ok());
        REQUIRE_FALSE(r.is_err());
        REQUIRE(r.value() == 42);
    }

    SECTION("Ok void") {
        Result<void> r = Ok();
        REQUIRE(r.is_ok());
        REQUIRE_FALSE(r.is_err());
    }

    SECTION("Err with message") {
        Result<int> r = Err<int>(Error("Something failed"));
        REQUIRE(r.is_err());
        REQUIRE_FALSE(r.is_ok());
        REQUIRE(r.error().message() == "Something failed");
    }

    SECTION("Err with Error object") {
        Error err(ErrorCode::NotFound, "Not found");
        Result<int> r = Err<int>(err);
        REQUIRE(r.is_err());
        REQUIRE(r.error().code() == ErrorCode::NotFound);
    }
}

TEST_CASE("Result value access", "[core][result]") {
    SECTION("value on Ok") {
        Result<std::string> r = Ok(std::string("hello"));
        REQUIRE(r.value() == "hello");
    }

    SECTION("value_or on Ok") {
        Result<int> r = Ok(42);
        REQUIRE(r.value_or(0) == 42);
    }

    SECTION("value_or on Err") {
        Result<int> r = Err<int>(Error("error"));
        REQUIRE(r.value_or(0) == 0);
    }

    SECTION("unwrap on Ok") {
        Result<int> r = Ok(42);
        REQUIRE(r.unwrap() == 42);
    }
}

TEST_CASE("Result move semantics", "[core][result]") {
    SECTION("move value out") {
        Result<std::string> r = Ok(std::string("hello"));
        std::string s = std::move(r).value();
        REQUIRE(s == "hello");
    }

    SECTION("move result") {
        Result<int> r1 = Ok(42);
        Result<int> r2 = std::move(r1);
        REQUIRE(r2.is_ok());
        REQUIRE(r2.value() == 42);
    }
}

TEST_CASE("Result map operations", "[core][result]") {
    SECTION("map on Ok") {
        Result<int> r = Ok(21);
        auto r2 = r.map([](int x) { return x * 2; });
        REQUIRE(r2.is_ok());
        REQUIRE(r2.value() == 42);
    }

    SECTION("map on Err") {
        Result<int> r = Err<int>(Error("error"));
        auto r2 = r.map([](int x) { return x * 2; });
        REQUIRE(r2.is_err());
    }

    SECTION("and_then on Ok") {
        Result<int> r = Ok(42);
        auto r2 = r.and_then([](int x) -> Result<std::string> {
            return Ok(std::to_string(x));
        });
        REQUIRE(r2.is_ok());
        REQUIRE(r2.value() == "42");
    }

    SECTION("and_then on Err") {
        Result<int> r = Err<int>(Error("error"));
        auto r2 = r.and_then([](int x) -> Result<std::string> {
            return Ok(std::to_string(x));
        });
        REQUIRE(r2.is_err());
    }

    SECTION("or_else on Err") {
        Result<int> r = Err<int>(Error("error"));
        auto r2 = r.or_else([](const Error& /*e*/) -> Result<int> {
            return Ok(0);
        });
        REQUIRE(r2.is_ok());
        REQUIRE(r2.value() == 0);
    }

    SECTION("or_else on Ok") {
        Result<int> r = Ok(42);
        auto r2 = r.or_else([](const Error& /*e*/) -> Result<int> {
            return Ok(0);
        });
        REQUIRE(r2.is_ok());
        REQUIRE(r2.value() == 42);
    }
}

TEST_CASE("Result boolean conversion", "[core][result]") {
    Result<int> ok = Ok(42);
    Result<int> err = Err<int>(Error("error"));

    REQUIRE(static_cast<bool>(ok));
    REQUIRE_FALSE(static_cast<bool>(err));
}

TEST_CASE("Result with complex types", "[core][result]") {
    struct Data {
        int x;
        std::string s;
    };

    SECTION("Ok with struct") {
        Result<Data> r = Ok(Data{42, "hello"});
        REQUIRE(r.is_ok());
        REQUIRE(r.value().x == 42);
        REQUIRE(r.value().s == "hello");
    }

    SECTION("vector in result") {
        Result<std::vector<int>> r = Ok(std::vector<int>{1, 2, 3});
        REQUIRE(r.is_ok());
        REQUIRE(r.value().size() == 3);
    }
}
