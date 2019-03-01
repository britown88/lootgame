#pragma once

template<typename Fn>
void forEachFileInFolder(std::filesystem::path const& folder, Fn && fn) {
   for (auto&& p : std::filesystem::directory_iterator(folder)) {
      if (std::filesystem::is_directory(p.path())) {
         forEachFileInFolder(p.path(), fn);
      }
      else {
         fn(p.path());
      }
   }
}
