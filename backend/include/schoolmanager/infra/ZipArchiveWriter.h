#pragma once

#include <string>
#include <vector>

namespace schoolmanager::infra {

struct ZipArchiveEntry {
    std::string name;
    std::string content;
};

class ZipArchiveWriter {
  public:
    std::string write(const std::vector<ZipArchiveEntry>& entries) const;
};

}  // namespace schoolmanager::infra
