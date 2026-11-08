#pragma once

#include "domain/error.hpp"
#include <string>
#include <optional>
#include <vector>

namespace domain {

class IStorageService {
public:
    virtual ~IStorageService() = default;

    virtual Result<bool> saveString(const std::string& key, const std::string& value) = 0;
    virtual Result<std::string> getString(const std::string& key, const std::string& defaultValue = "") = 0;
    virtual Result<bool> hasKey(const std::string& key) = 0;
    virtual Result<bool> removeKey(const std::string& key) = 0;
};

} // namespace domain
