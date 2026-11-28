#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <functional>
#include <atomic>
#include <string>
#include <vector>

namespace kpm {
using Json = nlohmann::json;
namespace fs = std::filesystem;
using Log = std::function<void(const std::string&)>;
struct Change { std::string path, before, after; };
using Changes = std::vector<Change>;
class Lock {
    int fd_ = -1;
public:
    explicit Lock(const fs::path& path);
    ~Lock();
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
};
Json read_json(const fs::path& path);
void validate(const Json& model);
// Schema v1: an absent build object means empty per-Element build settings.
Json element_build_settings(const Json& element);
// Validate explicitly selected module paths and merge into schema-v1 build lists.
Json custom_module_settings(const fs::path& root, const Json& build, const Json& request, const std::string& element_name = {});
Changes configure_custom_module(const fs::path& root, const std::string& element, const Json& request, bool preview = false);
Json load(const fs::path& root);
Changes create(const fs::path& root, const std::string& name, bool preview = false);
Changes add(const fs::path& root, const std::string& name, bool preview = false);
Changes configure_element(const fs::path& root, const std::string& name, const Json& settings, bool preview = false);
Changes regenerate(const fs::path& root, bool preview = false);
std::string tree(const fs::path& root);
std::string diff(const Changes& changes);
int execute(const std::vector<std::string>& args, const fs::path& cwd, Log log = {}, const std::atomic_bool* cancel = nullptr);
int build(const fs::path& root, const fs::path& directory, const std::string& config,
          const std::string& target = {}, const std::string& prefix = {}, Log log = {}, const std::atomic_bool* cancel = nullptr);
fs::path executable(const fs::path& root, const fs::path& directory, const std::string& config, const std::string& target);
int run(const fs::path& root, const fs::path& directory, const std::string& config,
        const std::string& element, Log log = {}, const std::atomic_bool* cancel = nullptr);
}
