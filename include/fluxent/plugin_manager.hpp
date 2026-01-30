#pragma once

#include "fluxent/plugin.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace fluxent {

class PluginManager {
public:
  void Register(const std::string &type, std::unique_ptr<Plugin> plugin) {
    plugins_[type] = std::move(plugin);
  }

  Plugin *Get(const std::string &type) const {
    auto it = plugins_.find(type);
    if (it != plugins_.end()) {
      return it->second.get();
    }
    return nullptr;
  }

private:
  std::unordered_map<std::string, std::unique_ptr<Plugin>> plugins_;
};

} // namespace fluxent
