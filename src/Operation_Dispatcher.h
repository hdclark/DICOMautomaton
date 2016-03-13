//Operation_Dispatcher.h.

#pragma once

#include <string>    
#include <map>
#include <list>
#include <functional>

#include "Structs.h"

typedef std::function<Drover(Drover, OperationArgPkg, std::map<std::string,std::string>, std::string)> op_func_t;

std::map<std::string, op_func_t> Known_Operations(void);


bool Operation_Dispatcher( Drover &DICOM_data,
                           std::map<std::string,std::string> &InvocationMetadata,
                           std::string &FilenameLex, 
                           std::list<OperationArgPkg> &Operations );

