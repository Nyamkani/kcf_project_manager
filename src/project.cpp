#include "kcf_project/project.hpp"
#include "templates.hpp"
#include <fstream>
#include <sstream>
#include <set>
#include <regex>
#include <cmath>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace kpm {
namespace {
void require(bool ok, const std::string& why) { if (!ok) throw std::runtime_error(why); }
std::string read(const fs::path& p) {
    std::ifstream f(p, std::ios::binary); require(bool(f), "Cannot read " + p.string());
    return {std::istreambuf_iterator<char>(f), {}};
}
void write(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary); f << text; f.close();
    require(bool(f), "Cannot write " + p.string());
}
bool plain(const std::string& s) { return s.find('\0') == std::string::npos && s.find('\n') == std::string::npos && s.find('\r') == std::string::npos; }
void name(const std::string& s) {
    require(std::regex_match(s, std::regex("[a-z][a-z0-9_]{0,47}")), "Name must match [a-z][a-z0-9_]{0,47}: " + s);
    require(s != "supervisor", "supervisor is reserved");
}
std::string uuid() { auto s = read("/proc/sys/kernel/random/uuid"); s.erase(s.find_last_not_of("\n") + 1); return s; }
void safe(const fs::path& root, const fs::path& relative) {
    require(!relative.empty() && !relative.is_absolute(), "Invalid managed path");
    auto p = root;
    for (const auto& part : relative) {
        require(part != "..", "Path traversal rejected"); p /= part;
        require(!fs::is_symlink(fs::symlink_status(p)), "Symlink rejected: " + p.string());
    }
}
void root_ok(const fs::path& root) {
    auto s = fs::absolute(root).string();
    require(plain(s) && s.find_first_of("\";\\$") == std::string::npos, "Unsupported CMake project path characters");
    require(!fs::is_symlink(fs::symlink_status(root)), "Project root must not be a symlink");
}
std::string replace(std::string s, const std::string& key, const std::string& value) {
    size_t at = 0; while ((at = s.find(key, at)) != std::string::npos) { s.replace(at, key.size(), value); at += value.size(); } return s;
}
struct Files { std::map<std::string,std::string> managed, user; };
Files render(const Json& m) {
    Files f;
    std::string project = m.at("name");
    std::string cm = "# Managed: edits are detected and preserved; extend ../CMakeLists.txt instead.\n";
    std::string metadata;
    for (const auto& e : m.at("elements")) {
        std::string n = e.at("name"), target = e.at("target"), cls = "Element_" + n;
        std::string base = "src/" + n + "/", header = project + "/" + n + ".hpp";
        f.user["include/" + header] = "#pragma once\n#include <kcf/process/process_element.hpp>\nclass " + cls + " : public kcf::ProcessElement {\npublic:\n    " + cls + "(int argc, char** argv);\n    int Setup() override;\n    int Loop() override;\n    void Shutdown() override;\n};\n";
        f.user[base + n + ".cpp"] = "#include \"" + header + "\"\n// User-owned: implement drivers, algorithms and lifecycle here.\n" + cls + "::" + cls + "(int argc, char** argv) { (void)argc; (void)argv; }\nint " + cls + "::Setup() { return 0; }\nint " + cls + "::Loop() { return 0; }\nvoid " + cls + "::Shutdown() {}\n";
        f.managed[base + "main.cpp"] = replace(replace(templates::element_main, "@HEADER@", header), "@CLASS@", cls);
        cm += "add_executable(" + target + " " + base + "main.cpp " + base + n + ".cpp)\ntarget_include_directories(" + target + " PRIVATE include generated)\ntarget_link_libraries(" + target + " PRIVATE KCF::core nlohmann_json::nlohmann_json)\n";
        metadata += target + "\n$<TARGET_FILE:" + target + ">\n";
    }
    std::string supervisor = project + "_supervisor";
    cm += "add_executable(" + supervisor + " generated/supervisor.cpp)\ntarget_include_directories(" + supervisor + " PRIVATE generated)\ntarget_link_libraries(" + supervisor + " PRIVATE KCF::bringup nlohmann_json::nlohmann_json)\n";
    for (const auto& e : m.at("elements")) cm += "add_dependencies(" + supervisor + " " + e.at("target").get<std::string>() + ")\n";
    metadata += supervisor + "\n$<TARGET_FILE:" + supervisor + ">\n";
    cm += "file(GENERATE OUTPUT \"${CMAKE_BINARY_DIR}/kcf-artifacts-$<CONFIG>.txt\" CONTENT \"" + metadata + "\")\nfile(GENERATE OUTPUT \"${CMAKE_BINARY_DIR}/kcf-source.txt\" CONTENT \"${CMAKE_SOURCE_DIR}\")\n";
    f.managed["cmake/elements.cmake"] = cm;
    f.managed["generated/runtime.hpp"] = templates::runtime;
    f.managed["generated/supervisor.cpp"] = templates::supervisor_main;
    f.managed["kcf.project.json"] = m.dump(2) + "\n";
    f.user["CMakeLists.txt"] = "cmake_minimum_required(VERSION 3.20)\nproject(" + project + " LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 17)\nset(CMAKE_CXX_STANDARD_REQUIRED ON)\nfind_package(KCF CONFIG REQUIRED)\nfind_package(nlohmann_json 3.2 REQUIRED)\ninclude(cmake/elements.cmake)\n# User-owned: add libraries, sources, flags or target properties here.\n";
    f.user["CMakePresets.json"] = Json{{"version",3},{"configurePresets",Json::array({Json{{"name","debug"},{"generator","Unix Makefiles"},{"binaryDir","${sourceDir}/build"},{"cacheVariables",{{"CMAKE_BUILD_TYPE","Debug"}}}},Json{{"name","release"},{"generator","Unix Makefiles"},{"binaryDir","${sourceDir}/build-release"},{"cacheVariables",{{"CMAKE_BUILD_TYPE","Release"}}}}})},{"buildPresets",Json::array({Json{{"name","debug"},{"configurePreset","debug"}},Json{{"name","release"},{"configurePreset","release"}}})}}.dump(2) + "\n";
    f.user[".gitignore"] = "/build*/\n/.kcf-*.lock\n/.kcf/transaction/\n";
    return f;
}
Changes apply(const fs::path& root, const Json& m, bool preview, bool fresh = false) {
    validate(m); auto files = render(m);
    safe(root,".kcf/ownership.json"); safe(root,".kcf/transaction");
    require(!fs::exists(root/".kcf/transaction"), "Incomplete transaction: inspect .kcf/transaction before recovery (see manual)");
    Json old = fresh ? Json{{"managed",Json::object()},{"user",Json::array()}} : read_json(root/".kcf/ownership.json");
    std::set<std::string> users;
    for (const auto& p : old.at("user")) users.insert(p.get<std::string>());
    for (auto it = old.at("managed").begin(); it != old.at("managed").end(); ++it) {
        safe(root,it.key()); require(fs::is_regular_file(root/it.key()), "Missing managed file: " + it.key());
        require(read(root/it.key()) == it.value().get<std::string>(), "Modified managed file: " + it.key() + "; restore or reconcile explicitly");
    }
    Changes changes;
    for (const auto& [p,content] : files.user) {
        safe(root,p);
        if (users.count(p)) { require(fs::is_regular_file(root/p), "Missing user file: " + p); continue; }
        require(!fs::exists(root/p), "Unowned file collision: " + p);
        changes.push_back({p,"",content}); users.insert(p);
    }
    for (const auto& [p,content] : files.managed) {
        safe(root,p);
        bool exists = fs::exists(root/p);
        require(!exists || old.at("managed").contains(p), "Unowned file collision: " + p);
        std::string before = exists ? read(root/p) : "";
        if (!exists || before != content) changes.push_back({p,before,content});
    }
    Json ownership{{"managed",files.managed},{"user",users}};
    auto tracking = ownership.dump(2) + "\n";
    std::string previous = fresh ? "" : read(root/".kcf/ownership.json");
    if (previous != tracking) changes.push_back({".kcf/ownership.json",previous,tracking});
    if (preview || changes.empty()) return changes;
    // Stage every replacement and its backup before making any visible change.
    auto transaction = root/".kcf/transaction";
    fs::create_directories(transaction);
    Json journal = Json::array();
    for (const auto& c : changes) {
        bool existed = fs::exists(root/c.path);
        journal.push_back({{"path",c.path},{"existed",existed}});
        write(transaction/"new"/c.path,c.after);
        if (existed) write(transaction/"old"/c.path,c.before);
    }
    write(transaction/"journal.json",journal.dump(2));
    size_t applied = 0;
    try {
        for (const auto& c : changes) {
            fs::create_directories((root/c.path).parent_path());
            fs::rename(transaction/"new"/c.path,root/c.path); ++applied;
        }
    } catch (...) {
        for (size_t i = applied; i > 0; --i) {
            const auto& entry = journal[i-1]; fs::path p = entry.at("path").get<std::string>();
            if (entry.at("existed").get<bool>()) fs::rename(transaction/"old"/p,root/p);
            else fs::remove(root/p);
        }
        fs::remove_all(transaction); throw;
    }
    fs::remove_all(transaction); return changes;
}
Json element(Json& model, const std::string& n) {
    name(n); std::string project = model.at("name");
    return Json{{"id",uuid()},{"name",n},{"target",project+"_"+n},{"supervised",true},{"hz",10.0},
        {"argv",Json::array()},{"env",Json::object()},{"workdir","."},{"startup_timeout_ms",5000},{"health_timeout_ms",2000},{"shutdown_timeout_ms",1500}};
}
fs::path build_path(const fs::path& root, const fs::path& dir) { return fs::absolute(dir.is_absolute() ? dir : root/dir); }
void config_ok(const std::string& config) { require(config == "Debug" || config == "Release" || config == "RelWithDebInfo" || config == "MinSizeRel", "Invalid build configuration"); }
}
Lock::Lock(const fs::path& p) {
    fd_ = ::open(p.c_str(),O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
    if (fd_ < 0) throw std::runtime_error("Cannot open lock: " + p.string());
    if (flock(fd_,LOCK_EX|LOCK_NB)) { close(fd_); fd_=-1; throw std::runtime_error("Project busy: " + p.string()); }
}
Lock::~Lock() { if(fd_>=0) { flock(fd_,LOCK_UN); close(fd_); } }
Json read_json(const fs::path& p) { return Json::parse(read(p)); }
void validate(const Json& m) {
    require(m.is_object() && m.at("schema") == 1,"Unsupported manifest schema (expected 1)");
    name(m.at("name"));
    auto valid_id = [](const Json& id) { return id.is_string() && std::regex_match(id.get<std::string>(),std::regex("[a-f0-9-]{36}")); };
    require(valid_id(m.at("id")),"Invalid project ID");
    require(m.at("elements").is_array(),"elements must be an array");
    std::set<std::string> ids, names, targets{m.at("name").get<std::string>()+"_supervisor"};
    for(const auto& e:m.at("elements")) {
        require(valid_id(e.at("id")),"Invalid Element ID"); name(e.at("name"));
        require(ids.insert(e.at("id")).second,"Duplicate Element ID");
        require(names.insert(e.at("name")).second,"Duplicate Element name");
        auto t=e.at("target").get<std::string>();
        require(std::regex_match(t,std::regex("[a-z][a-z0-9_]{0,100}")) && targets.insert(t).second,"Invalid/duplicate target");
        require(e.at("supervised").is_boolean(),"supervised must be boolean");
        double hz=e.at("hz").get<double>(); require(std::isfinite(hz)&&hz>0&&hz<=100000,"hz must be in (0, 100000]");
        for(auto k:{"startup_timeout_ms","health_timeout_ms","shutdown_timeout_ms"}) {
            require(e.at(k).is_number_integer(),std::string(k)+" must be an integer");
            auto v=e.at(k).get<long long>(); require(v>0&&v<=3600000,std::string(k)+" must be 1..3600000");
        }
        require(e.at("argv").is_array(),"argv must be an array");
        for(const auto& a:e.at("argv")) require(a.is_string()&&a.get<std::string>().find('\0')==std::string::npos,"argv must contain NUL-free strings");
        require(e.at("env").is_object(),"env must be an object");
        for(auto it=e.at("env").begin();it!=e.at("env").end();++it) {
            require(std::regex_match(it.key(),std::regex("[A-Za-z_][A-Za-z0-9_]*"))&&it.key().rfind("KCF_",0)!=0,"Invalid/reserved env key: "+it.key());
            require(it.value().is_string()&&it.value().get<std::string>().find('\0')==std::string::npos,"Invalid env value");
        }
        auto cwd=e.at("workdir").get<std::string>(); require(!cwd.empty()&&plain(cwd),"Invalid working directory");
    }
}
Json load(const fs::path& root) { root_ok(root); safe(root,"kcf.project.json"); auto m=read_json(root/"kcf.project.json");validate(m);return m; }
Changes create(const fs::path& root,const std::string& n,bool preview) {
    root_ok(root); name(n);
    if (fs::exists(root/"kcf.project.json")) {
        Lock lock(root/".kcf-project.lock"); auto m=load(root);require(m.at("name")==n,"Existing project name differs");return apply(root,m,preview);
    }
    if (fs::exists(root)) require(fs::is_empty(root),"New project requires an empty directory");
    Json m{{"schema",1},{"id",uuid()},{"name",n},{"elements",Json::array()}};
    if(preview) return apply(root,m,true,true);
    fs::create_directories(root); Lock lock(root/".kcf-project.lock"); return apply(root,m,false,true);
}
Changes add(const fs::path& root,const std::string& n,bool preview) {
    Lock lock(root/".kcf-project.lock");auto m=load(root);
    for(const auto& e:m.at("elements")) if(e.at("name")==n) return apply(root,m,preview);
    m["elements"].push_back(element(m,n));return apply(root,m,preview);
}
Changes configure_element(const fs::path& root,const std::string& n,const Json& settings,bool preview) {
    Lock lock(root/".kcf-project.lock");auto m=load(root);require(settings.is_object(),"Settings must be a JSON object");
    std::set<std::string> allowed{"hz","argv","env","workdir","supervised","startup_timeout_ms","health_timeout_ms","shutdown_timeout_ms"};
    for(auto it=settings.begin();it!=settings.end();++it) require(allowed.count(it.key()),"Unsupported setting: "+it.key());
    for(auto& e:m["elements"]) if(e.at("name")==n) { e.update(settings);return apply(root,m,preview); }
    throw std::runtime_error("Unknown Element: "+n);
}
Changes regenerate(const fs::path& root,bool preview) { Lock lock(root/".kcf-project.lock");return apply(root,load(root),preview); }
std::string tree(const fs::path& root) {
    Lock lock(root/".kcf-project.lock");auto m=load(root);std::ostringstream out;
    out<<m.at("name").get<std::string>()<<" [design-time; no observed runtime state]\n  Supervisor: "<<m.at("name").get<std::string>()<<"_supervisor\n";
    for(const auto& e:m.at("elements")) {
        std::string n=e.at("name");out<<"    "<<n<<" ["<<e.at("id").get<std::string>()<<"] supervised="<<e.at("supervised")<<" hz="<<e.at("hz")<<"\n      Class: Element_"<<n<<"\n      Source: src/"<<n<<"/"<<n<<".cpp\n      Header: include/"<<m.at("name").get<std::string>()<<"/"<<n<<".hpp\n      CMake target: "<<e.at("target").get<std::string>()<<"\n      Executable: resolved from configured CMake metadata at launch\n";
    }return out.str();
}
std::string diff(const Changes& changes) { std::ostringstream out;for(const auto& c:changes) {if(c.path==".kcf/ownership.json") {out<<"ownership index updated\n";continue;}out<<"--- "<<c.path<<"\n+++ "<<c.path<<"\n";std::istringstream a(c.before),b(c.after);std::string line;while(std::getline(a,line))out<<'-'<<line<<'\n';while(std::getline(b,line))out<<'+'<<line<<'\n';}if(changes.empty())out<<"No changes\n";return out.str(); }
int build(const fs::path& root,const fs::path& dir,const std::string& config,const std::string& target,const std::string& prefix,Log log,const std::atomic_bool* cancel) {
    Lock lock(root/".kcf-project.lock");auto m=load(root);config_ok(config);
    auto b=build_path(root,dir);fs::create_directories(b);Lock build_lock(b/".kcf-build.lock");
    std::vector<std::string> args{"cmake","-S",fs::absolute(root).string(),"-B",b.string(),"-DCMAKE_BUILD_TYPE="+config};
    if(!prefix.empty()) args.push_back("-DCMAKE_PREFIX_PATH="+prefix);
    int rc=execute(args,root,log,cancel);if(rc)return rc;
    if(!target.empty()) { bool found=target==m.at("name").get<std::string>()+"_supervisor";for(const auto& e:m.at("elements"))found=found||e.at("target")==target;require(found,"Unknown project target: "+target); }
    args={"cmake","--build",b.string(),"--config",config,"--parallel","2"};if(!target.empty()){args.push_back("--target");args.push_back(target);}return execute(args,root,log,cancel);
}
fs::path executable(const fs::path& root,const fs::path& dir,const std::string& config,const std::string& target) {
    config_ok(config);auto b=build_path(root,dir);require(fs::equivalent(read(b/"kcf-source.txt"),root),"Build directory belongs to another project");
    std::ifstream f(b/("kcf-artifacts-"+config+".txt"));require(bool(f),"Missing CMake metadata; configure/build first");
    std::string t,p;std::map<std::string,std::string> paths;
    while(std::getline(f,t)) {require(bool(std::getline(f,p)),"Malformed CMake metadata");require(paths.emplace(t,p).second,"Duplicate target metadata");}
    require(paths.count(target),"Target not in configured CMake metadata: "+target);fs::path result=paths.at(target);
    require(result.is_absolute()&&fs::is_regular_file(result)&&access(result.c_str(),X_OK)==0,"Executable missing; build target: "+target);return result;
}
int run(const fs::path& root,const fs::path& dir,const std::string& config,const std::string& element_name,Log log,const std::atomic_bool* cancel) {
    Lock lock(root/".kcf-project.lock");Lock run_lock(root/".kcf-run.lock");auto m=load(root);auto b=build_path(root,dir);Lock build_lock(b/".kcf-build.lock");
    auto manifest=fs::absolute(root/"kcf.project.json").string();std::vector<std::string> args;
    if(element_name.empty()) {
        bool member=false;for(const auto& e:m.at("elements")) if(e.at("supervised").get<bool>()){member=true;executable(root,dir,config,e.at("target"));fs::path cwd=e.at("workdir").get<std::string>();require(fs::is_directory(cwd.is_absolute()?cwd:root/cwd),"Working directory does not exist");}
        require(member,"Supervisor has no members");args={executable(root,dir,config,m.at("name").get<std::string>()+"_supervisor").string(),manifest,(b/("kcf-artifacts-"+config+".txt")).string()};
    } else {
        for(const auto& e:m.at("elements")) if(e.at("name")==element_name) {fs::path cwd=e.at("workdir").get<std::string>();require(fs::is_directory(cwd.is_absolute()?cwd:root/cwd),"Working directory does not exist");args={executable(root,dir,config,e.at("target")).string(),"--kcf-project",manifest,"--kcf-element",e.at("id")};for(const auto& a:e.at("argv"))args.push_back(a.get<std::string>());}
        require(!args.empty(),"Unknown Element: "+element_name);
    }return execute(args,fs::absolute(root),log,cancel);
}
}
