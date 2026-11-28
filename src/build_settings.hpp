#pragma once
#include "kcf_project/project.hpp"
namespace kpm::detail {
std::string build_packages(const Json& model);
std::string build_target(const Json& element);
}
