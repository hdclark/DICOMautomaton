//Operation_Dispatcher.h.

#pragma once

#include <string>    
#include <map>
#include <set>
#include <list>
#include <functional>
#include <utility>

#include "Structs.h"

using op_func_t = std::function<bool (Drover &, 
                                      const OperationArgPkg &,
                                      std::map<std::string, std::string> &,
                                      const std::string & )>;
using op_doc_func_t = std::function<OperationDoc ()>;
using op_packet_t = std::pair<op_doc_func_t,op_func_t>;
using known_ops_t = std::map<std::string, op_packet_t>;

known_ops_t Known_Operations();

known_ops_t Known_Operations_and_Aliases();

std::map<std::string, std::string> Operation_Lexicon();


using known_ops_tags_t = std::set<std::string>;
known_ops_t Only_Operations(const known_ops_t &, known_ops_tags_t tags);

known_ops_tags_t Get_Unique_Tags(const known_ops_t &);


bool Operation_Dispatcher( Drover &DICOM_data,
                           std::map<std::string,std::string> &InvocationMetadata,
                           const std::string &FilenameLex,
                           const std::list<OperationArgPkg> &Operations);

