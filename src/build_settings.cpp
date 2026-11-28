#include "build_settings.hpp"
#include <regex>
#include <set>

namespace kpm {
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("Build settings: " + message);
}

// These are literal CMake arguments, never snippets of CMake language. Reject
// list separators and expansion syntax even inside bracket-quoted arguments:
// target properties may interpret them again at generation time.
void literal(const Json& value, const std::string& field) {
    check(value.is_string(), field + " must contain strings");
    const auto s = value.get<std::string>();
    check(!s.empty() && s.find_first_of(";$\\\n\r") == std::string::npos &&
          s.find('\0') == std::string::npos, field + " requires nonempty literal values (no ;, $, backslash or newline)");
    static const std::set<std::string> keywords{
        "PUBLIC", "PRIVATE", "INTERFACE", "BEFORE", "AFTER", "SYSTEM",
        "FILE_SET", "TYPE", "BASE_DIRS", "FILES", "debug", "optimized", "general"};
    check(!keywords.count(s), field + " contains a reserved CMake keyword: " + s);
}

std::string cmake_argument(const std::string& value) {
    std::string equals;
    while (value.find("]" + equals + "]") != std::string::npos) equals += '=';
    return "[" + equals + "[" + value + "]" + equals + "]";
}

std::string path_argument(const std::string& value) {
    if (fs::path(value).is_absolute()) return cmake_argument(value);
    std::string escaped;
    for (char c : value) { if (c == '"') escaped += '\\'; escaped += c; }
    return "\"${CMAKE_CURRENT_SOURCE_DIR}/" + escaped + "\"";
}
}

Json element_build_settings(const Json& element) {
    Json result = {{"sources", Json::array()}, {"include_directories", Json::array()},
                   {"packages", Json::array()}, {"link_libraries", Json::array()},
                   {"compile_definitions", Json::array()}, {"compile_options", Json::array()},
                   {"link_options", Json::array()}};
    if (!element.contains("build")) return result;
    const auto& settings = element.at("build");
    check(settings.is_object(), "build must be an object");
    for (auto it = settings.begin(); it != settings.end(); ++it) {
        check(result.contains(it.key()), "unknown field: " + it.key());
        check(it.value().is_array(), it.key() + " must be an array");
        result[it.key()] = it.value();
    }
    for (auto it = result.begin(); it != result.end(); ++it) {
        if (it.key() == "packages") continue;
        for (const auto& value : it.value()) {
            literal(value, it.key());
            const auto s = value.get<std::string>();
            if (it.key() == "compile_definitions")
                check(std::regex_match(s, std::regex("[A-Za-z_][A-Za-z0-9_]*(=.*)?")),
                      "compile_definitions requires NAME or NAME=value, without -D");
        }
    }
    std::set<std::string> names;
    for (auto& package : result["packages"]) {
        check(package.is_object(), "packages must contain objects");
        for (auto it = package.begin(); it != package.end(); ++it)
            check(it.key() == "name" || it.key() == "version" || it.key() == "mode" || it.key() == "components",
                  "unknown package field: " + it.key());
        check(package.contains("name") && package.at("name").is_string(), "package name is required");
        auto name = package.at("name").get<std::string>();
        check(std::regex_match(name, std::regex("[A-Za-z_][A-Za-z0-9_.+-]*")), "invalid package name");
        literal(name, "package name");
        check(name != "KCF" && name != "nlohmann_json", "KCF and nlohmann_json are already provided by the project");
        check(names.insert(name).second, "duplicate package: " + name);
        if (!package.contains("version")) package["version"] = "";
        if (!package.contains("mode")) package["mode"] = "auto";
        if (!package.contains("components")) package["components"] = Json::array();
        check(package["version"].is_string(), "package version must be a string");
        auto version = package["version"].get<std::string>();
        check(version.empty() || std::regex_match(version, std::regex("[0-9]+(\\.[0-9]+){0,3}")), "invalid package version");
        check(package["mode"] == "auto" || package["mode"] == "config" || package["mode"] == "module", "package mode must be auto, config or module");
        check(package["components"].is_array(), "package components must be an array");
        for (const auto& component : package["components"]) {
            literal(component, "package component");
            check(std::regex_match(component.get<std::string>(), std::regex("[A-Za-z0-9_.+-]+")), "invalid package component");
            static const std::set<std::string> reserved{
                "REQUIRED", "OPTIONAL", "QUIET", "EXACT", "CONFIG", "MODULE", "NO_MODULE",
                "COMPONENTS", "OPTIONAL_COMPONENTS", "GLOBAL", "BYPASS_PROVIDER", "UNWIND_INCLUDE",
                "NAMES", "CONFIGS", "HINTS", "PATHS", "PATH_SUFFIXES", "REGISTRY_VIEW",
                "NO_DEFAULT_PATH", "NO_POLICY_SCOPE", "NO_PACKAGE_ROOT_PATH", "NO_CMAKE_PATH",
                "NO_CMAKE_ENVIRONMENT_PATH", "NO_SYSTEM_ENVIRONMENT_PATH", "NO_CMAKE_PACKAGE_REGISTRY",
                "NO_CMAKE_BUILDS_PATH", "NO_CMAKE_SYSTEM_PATH", "NO_CMAKE_INSTALL_PREFIX",
                "NO_CMAKE_SYSTEM_PACKAGE_REGISTRY", "CMAKE_FIND_ROOT_PATH_BOTH",
                "ONLY_CMAKE_FIND_ROOT_PATH", "NO_CMAKE_FIND_ROOT_PATH"};
            check(!reserved.count(component.get<std::string>()), "reserved package component");
        }
    }
    return result;
}

namespace detail {
std::string build_packages(const Json& model) {
    std::map<std::string, Json> requests;
    std::string output;
    for (const auto& element : model.at("elements")) {
        auto settings = element_build_settings(element);
        for (const auto& package : settings.at("packages")) {
            const auto name = package.at("name").get<std::string>();
            auto existing = requests.find(name);
            if (existing != requests.end()) {
                check(existing->second == package, "conflicting find_package requests for " + name);
                continue;
            }
            requests.emplace(name, package);
            output += "find_package(" + cmake_argument(name);
            if (package.at("version") != "") output += " " + cmake_argument(package.at("version"));
            if (package.at("mode") == "config") output += " CONFIG";
            if (package.at("mode") == "module") output += " MODULE";
            output += " REQUIRED";
            if (!package.at("components").empty()) {
                output += " COMPONENTS";
                for (const auto& component : package.at("components")) output += " " + cmake_argument(component);
            }
            output += ")\n";
        }
    }
    return output;
}

std::string build_target(const Json& element) {
    const auto settings = element_build_settings(element);
    const auto target = element.at("target").get<std::string>();
    const std::pair<const char*, const char*> commands[] = {
        {"sources", "target_sources"}, {"include_directories", "target_include_directories"},
        {"link_libraries", "target_link_libraries"}, {"compile_definitions", "target_compile_definitions"},
        {"compile_options", "target_compile_options"}, {"link_options", "target_link_options"}};
    std::string output;
    for (const auto& [field, command] : commands) {
        if (settings.at(field).empty()) continue;
        output += std::string(command) + "(" + target + " PRIVATE\n";
        for (const auto& value : settings.at(field)) {
            auto s = value.get<std::string>();
            bool path = std::string(field) == "sources" || std::string(field) == "include_directories" ||
                        (std::string(field) == "link_libraries" && s.find('/') != std::string::npos);
            output += "  " + (path ? path_argument(s) : cmake_argument(s)) + "\n";
        }
        output += ")\n";
    }
    return output;
}
}
}
