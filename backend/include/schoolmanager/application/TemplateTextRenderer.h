#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace schoolmanager::application {

std::string renderTextTemplate(
    std::string_view templateText,
    const std::unordered_map<std::string, std::string>& variables);

}  // namespace schoolmanager::application
