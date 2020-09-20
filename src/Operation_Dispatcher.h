//Operation_Dispatcher.h.

#pragma once

#include <string>    
#include <map>
#include <list>
#include <functional>
#include <utility>

#include "Structs.h"

using op_func_t = std::function<Drover (Drover, 
                                const OperationArgPkg &,
                                const std::map<std::string, std::string> &,
                                const std::string & )>;
using op_doc_func_t = std::function<OperationDoc ()>;
typedef std::pair<op_doc_func_t,op_func_t> op_packet_t;

std::map<std::string, op_packet_t> Known_Operations();

bool Operation_Dispatcher( Drover &DICOM_data,
                           std::map<std::string,std::string> &InvocationMetadata,
                           std::string &FilenameLex,
                           std::list<OperationArgPkg> &Operations);

