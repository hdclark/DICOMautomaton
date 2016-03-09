//Analysis_Dispatcher.h.

#pragma once

#include <string>    
#include <map>
#include <list>

#include "Structs.h"

bool Analysis_Dispatcher( Drover &DICOM_data,
                          std::map<std::string,std::string> &InvocationMetadata,
                          std::string &FilenameLex, 
                          std::list<std::string> &Operations );

