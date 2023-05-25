// Fork.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocFork();

bool Fork(Drover &,
          const OperationArgPkg &,
          std::map<std::string, std::string> &,
          const std::string & );
