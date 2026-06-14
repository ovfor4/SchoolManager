#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace schoolmanager::application {

enum class ApplicationErrorCode {
    BadRequest,
    NotFound,
};

class ApplicationError : public std::runtime_error {
  public:
    ApplicationError(ApplicationErrorCode code, std::string message)
        : std::runtime_error(std::move(message)), code_(code)
    {
    }

    [[nodiscard]] ApplicationErrorCode code() const noexcept
    {
        return code_;
    }

  private:
    ApplicationErrorCode code_;
};

}  // namespace schoolmanager::application
