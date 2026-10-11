#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_RENAME_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_RENAME_HPP

#include <filesystem>

int RenamePath_nid_no_patch(const std::filesystem::path& source, const std::filesystem::path& destination);

#endif
