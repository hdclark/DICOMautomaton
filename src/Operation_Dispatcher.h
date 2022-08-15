//Operation_Dispatcher.h.

#pragma once

#include <string>    
#include <map>
#include <list>
#include <functional>
#include <utility>

#include "Structs.h"

using op_func_t = std::function<bool (Drover &, 
                                      const OperationArgPkg &,
                                      std::map<std::string, std::string> &,
                                      const std::string & )>;
using op_doc_func_t = std::function<OperationDoc ()>;
typedef std::pair<op_doc_func_t,op_func_t> op_packet_t;

std::map<std::string, op_packet_t> Known_Operations();

std::map<std::string, op_packet_t> Known_Operations_and_Aliases();

std::map<std::string, std::string> Operation_Lexicon();

bool Operation_Dispatcher( Drover &DICOM_data,
                           std::map<std::string,std::string> &InvocationMetadata,
                           const std::string &FilenameLex,
                           const std::list<OperationArgPkg> &Operations);

