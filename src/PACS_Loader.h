//PACS_Loader.h.

#pragma once

#include <string>    
#include <map>
#include <list>

#include "Structs.h"

bool Load_From_PACS_DB( Drover &DICOM_data,
                        std::map<std::string,std::string> &InvocationMetadata,
                        const std::string &FilenameLex,
                        std::string &db_connection_params,
                        std::list<std::list<std::string>> &GroupedFilterQueryFiles );


