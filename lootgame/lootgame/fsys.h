#pragma once

#include <filesystem>

namespace fsys = std::experimental::filesystem;

template<typename Fn>
void forEachFileInFolder(fsys::path const& folder, Fn && fn) {
   for (auto&& p : fsys::directory_iterator(folder)) {
      if (fsys::is_directory(p.path())) {
         forEachFileInFolder(p.path(), fn);
      }
      else {
         fn(p.path());
      }
   }
}
