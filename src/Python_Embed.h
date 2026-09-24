//Python_Embed.h.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Structs.h"

namespace dcma {
namespace python {

void ExecuteScriptFile(Drover &data,
                       std::map<std::string, std::string> &metadata,
                       const std::string &lexicon_filename,
                       const std::string &filename,
                       const std::string &function,
                       const std::vector<std::string> &module_search_paths);

} // namespace python
} // namespace dcma
