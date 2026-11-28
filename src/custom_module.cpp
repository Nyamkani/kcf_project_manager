#include "kcf_project/project.hpp"
#include <set>

namespace kpm {
namespace {
void check(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error("Custom module: " + message);
}
bool within(const fs::path& path, const fs::path& directory) {
    auto p = path.begin();
    for (auto d = directory.begin(); d != directory.end(); ++d, ++p)
        if (p == path.end() || *p != *d) return false;
    return true;
}
}

Json custom_module_settings(const fs::path& root, const Json& build, const Json& request, const std::string& element_name) {
    check(request.is_object(), "request must be an object");
    for (auto it = request.begin(); it != request.end(); ++it)
        check(it.key() == "module_directory" || it.key() == "source_directory" ||
              it.key() == "include_directories" || it.key() == "sources", "unknown field: " + it.key());
    const auto project = fs::canonical(root);
    auto resolve = [&](const Json& value, bool directory) {
        check(value.is_string() && !value.get<std::string>().empty(), "path must be a nonempty string");
        auto text = value.get<std::string>();
        check(text.find('\0') == std::string::npos, "NUL in path");
        fs::path input(text);
        auto path = input.is_absolute() ? input : project / input;
        check(directory ? fs::is_directory(path) : fs::is_regular_file(path),
              std::string(directory ? "directory does not exist: " : "source file does not exist: ") + text);
        auto resolved = fs::canonical(path);
        check(input.is_absolute() || within(resolved, project),
              "external paths (including symlinks) must be explicitly absolute: " + text);
        return resolved;
    };
    auto module = resolve(request.at("module_directory"), true);
    auto sourceDirectory = resolve(request.at("source_directory"), true);
    check(within(sourceDirectory, module), "source directory is outside the selected module");
    check(request.at("sources").is_array() && !request.at("sources").empty(), "select at least one actual .cpp file; directories are not scanned");
    check(request.at("include_directories").is_array(), "include_directories must be an array");
    Json result = element_build_settings(Json{{"build", build}});
    for (const auto* field : {"sources", "include_directories"}) {
        std::set<fs::path> selected;
        for (const auto& value : request.at(field)) {
            bool directory = std::string(field) == "include_directories";
            auto path = resolve(value, directory);
            check(within(path, directory ? module : sourceDirectory), "selected path is outside its module/source directory: " + path.string());
            if (!directory) check(fs::path(value.get<std::string>()).extension() == ".cpp", "only explicitly selected .cpp files are supported");
            if (!directory && !element_name.empty()) {
                for (const auto& file : {element_name + ".cpp", std::string("main.cpp")})
                    check(!fs::equivalent(path, project / "src" / element_name / file), "source is already part of the generated target: " + path.string());
            }
            check(selected.insert(path).second, "duplicate selected path: " + path.string());
            for (const auto& existing : result.at(field)) {
                fs::path entry(existing.get<std::string>());
                if (!entry.is_absolute()) entry = project / entry;
                check(fs::weakly_canonical(entry) != path &&
                      !(fs::exists(entry) && fs::equivalent(entry, path)), "path already registered: " + path.string());
            }
            // Keep explicit absolute selection absolute; internal relative input portable.
            fs::path input(value.get<std::string>());
            result[field].push_back(input.is_absolute() ? path.generic_string() : path.lexically_relative(project).generic_string());
        }
    }
    // Apply the same CMake literal validation used by ordinary set/GUI settings.
    return element_build_settings(Json{{"build", result}});
}
}
