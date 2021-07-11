//Script_Loader.h.

#pragma once

#include <istream>
#include <list>

#include "Structs.h"

bool Load_DCMA_Script(std::istream &is,
                      std::list<OperationArgPkg> &op_list);

