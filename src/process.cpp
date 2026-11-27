#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "kcf_project/project.hpp"
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <csignal>
#include <cerrno>
#include <cstring>
extern char** environ;
namespace kpm {
int execute(const std::vector<std::string>& args, const fs::path& cwd, Log log, const std::atomic_bool* cancel) {
    if (args.empty()) throw std::runtime_error("Empty command");
    if (cancel && cancel->load()) return 130;
    std::vector<char*> argv;
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    int pipes[2];
    if (pipe2(pipes,O_CLOEXEC)) throw std::runtime_error("pipe failed");
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions,pipes[1],STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions,pipes[1],STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions,pipes[0]);
    int error = posix_spawn_file_actions_addchdir_np(&actions,cwd.c_str());
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    sigset_t mask, defaults; sigemptyset(&mask); sigemptyset(&defaults);
    sigaddset(&defaults,SIGINT); sigaddset(&defaults,SIGTERM);
    posix_spawnattr_setsigmask(&attributes,&mask);
    posix_spawnattr_setsigdefault(&attributes,&defaults);
    // An owned child has its own process group; Stop signals only this direct child.
    posix_spawnattr_setpgroup(&attributes,0);
    posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP|POSIX_SPAWN_SETSIGMASK|POSIX_SPAWN_SETSIGDEF);
    pid_t pid = -1;
    if (!error) error = posix_spawnp(&pid,argv[0],&actions,&attributes,argv.data(),environ);
    posix_spawn_file_actions_destroy(&actions); posix_spawnattr_destroy(&attributes);
    close(pipes[1]);
    if(error) {close(pipes[0]);throw std::runtime_error("Cannot start " + args[0] + ": " + std::strerror(error));}
    if(log) log("Started PID " + std::to_string(pid) + ": " + args[0] + "\n");
    fcntl(pipes[0],F_SETFL,O_NONBLOCK);
    bool stopped=false, done=false; int status=0;
    while(!done) {
        if(cancel && cancel->load() && !stopped) { kill(pid,SIGTERM);stopped=true; }
        pollfd p{pipes[0],POLLIN,0};poll(&p,1,50);
        char buffer[8192];ssize_t n;
        while((n=read(pipes[0],buffer,sizeof(buffer)))>0) if(log)log(std::string(buffer,static_cast<size_t>(n)));
        auto result=waitpid(pid,&status,WNOHANG);
        if(result==pid)done=true;
        else if(result<0&&errno!=EINTR) {close(pipes[0]);throw std::runtime_error("waitpid failed");}
    }
    char buffer[8192];ssize_t n;while((n=read(pipes[0],buffer,sizeof(buffer)))>0)if(log)log(std::string(buffer,static_cast<size_t>(n)));
    close(pipes[0]);
    int rc=WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);
    if(log)log("Exited PID " + std::to_string(pid) + ": " + std::to_string(rc) + "\n");
    return rc;
}
}
