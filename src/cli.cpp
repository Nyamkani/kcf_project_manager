#include "kcf_project/project.hpp"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
namespace {
volatile std::sig_atomic_t interrupted=0;
void signal_handler(int) {interrupted=1;}
const char* help = R"(KCF Project Manager 0.1.0
  kcf-project create DIRECTORY NAME [--dry-run]
  kcf-project add PROJECT ELEMENT [--dry-run]
  kcf-project set PROJECT ELEMENT SETTINGS_JSON_FILE [--dry-run]
  kcf-project regenerate PROJECT [--dry-run]
  kcf-project tree PROJECT
  kcf-project validate PROJECT
  kcf-project build PROJECT [--target TARGET] [--build-dir DIR] [--config Debug|Release] [--prefix SDK_PREFIX]
  kcf-project run PROJECT ELEMENT [--build-dir DIR] [--config Debug|Release]
  kcf-project launch PROJECT [--build-dir DIR] [--config Debug|Release]
Run/launch remain in the foreground. Ctrl+C stops only the owned child.
Settings JSON keys: hz, argv (array), env (object), workdir, supervised,
startup_timeout_ms, health_timeout_ms, shutdown_timeout_ms.
)";
}
int main(int argc,char** argv) {
    try {
        if(argc==1||(argc==2&&std::string(argv[1])=="--help")){std::cout<<help;return 0;}
        std::string command=argv[1];std::vector<std::string> positional;
        std::map<std::string,std::string> options;bool preview=false;
        for(int i=2;i<argc;++i){std::string a=argv[i];if(a=="--dry-run")preview=true;
            else if(a.rfind("--",0)==0){if(a!="--target"&&a!="--build-dir"&&a!="--config"&&a!="--prefix")throw std::runtime_error("Unknown option: "+a);if(++i==argc)throw std::runtime_error("Missing option value");if(!options.emplace(a,argv[i]).second)throw std::runtime_error("Duplicate option: "+a);}
            else positional.push_back(a);}
        auto count=[&](size_t n){if(positional.size()!=n)throw std::runtime_error("Wrong arguments; use --help");};
        bool mutation=command=="create"||command=="add"||command=="set"||command=="regenerate";
        bool operation=command=="build"||command=="run"||command=="launch";
        if(preview&&!mutation)throw std::runtime_error("--dry-run is only valid for generation/settings");
        if(!options.empty()&&!operation)throw std::runtime_error("Build/run options are not valid here");
        if(command!="build"&&(options.count("--target")||options.count("--prefix")))throw std::runtime_error("--target/--prefix require build");
        if(command=="create"){count(2);std::cout<<kpm::diff(kpm::create(positional[0],positional[1],preview));}
        else if(command=="add"){count(2);std::cout<<kpm::diff(kpm::add(positional[0],positional[1],preview));}
        else if(command=="set"){count(3);std::cout<<kpm::diff(kpm::configure_element(positional[0],positional[1],kpm::read_json(positional[2]),preview));}
        else if(command=="regenerate"){count(1);std::cout<<kpm::diff(kpm::regenerate(positional[0],preview));}
        else if(command=="tree"){count(1);std::cout<<kpm::tree(positional[0]);}
        else if(command=="validate"){count(1);kpm::load(positional[0]);std::cout<<"Valid schema 1 project\n";}
        else if(operation){
            count(command=="run"?2:1);
            std::string config=options.count("--config")?options["--config"]:"Debug";
            std::string dir=options.count("--build-dir")?options["--build-dir"]:"build";
            std::atomic_bool cancel{false},done{false};std::signal(SIGINT,signal_handler);std::signal(SIGTERM,signal_handler);
            std::thread watcher([&]{while(!done){if(interrupted)cancel=true;std::this_thread::sleep_for(std::chrono::milliseconds(20));}});
            int result=2;
            try {auto log=[](const std::string& s){std::cout<<s<<std::flush;};
                if(command=="build")result=kpm::build(positional[0],dir,config,options["--target"],options["--prefix"],log,&cancel);
                else result=kpm::run(positional[0],dir,config,command=="run"?positional[1]:"",log,&cancel);
            }catch(...){done=true;watcher.join();throw;}
            done=true;watcher.join();return result;
        }else throw std::runtime_error("Unknown command: "+command);
        return 0;
    }catch(const std::exception& e){std::cerr<<"kcf-project: "<<e.what()<<'\n';return 2;}
}
