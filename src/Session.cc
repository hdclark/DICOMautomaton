//Session.cc.

#include "Session.h"

#include <iterator>
#include <sstream>
#include <utility>

#include "File_Loader.h"
#include "Lexicon_Loader.h"
#include "Operation_Dispatcher.h"

#include "YgorLog.h"

namespace dcma {

Drover & Session::data(){
    return data_;
}

const Drover & Session::data() const{
    return data_;
}

Session::metadata_t & Session::metadata(){
    return metadata_;
}

const Session::metadata_t & Session::metadata() const{
    return metadata_;
}

std::string & Session::lexicon_filename(){
    return lexicon_filename_;
}

const std::string & Session::lexicon_filename() const{
    return lexicon_filename_;
}

Session::operation_list_t & Session::pending_operations(){
    return pending_operations_;
}

const Session::operation_list_t & Session::pending_operations() const{
    return pending_operations_;
}

bool Session::has_pending_operations() const{
    return !loaded_operations_.empty() || !pending_operations_.empty();
}

void Session::prepare_lexicon(){
    if(!lexicon_filename_.empty()){
        return;
    }

    lexicon_filename_ = Locate_Lexicon_File();
    if(!lexicon_filename_.empty()){
        YLOGINFO("No lexicon was explicitly provided. Using located file '" << lexicon_filename_ << "' as lexicon");
        return;
    }

    YLOGINFO("No lexicon provided or located. Attempting to write a default lexicon");
    lexicon_filename_ = Create_Default_Lexicon_File();
    YLOGINFO("Using file '" << lexicon_filename_ << "' as lexicon");
}

bool Session::load(path_list_t &paths){
    last_error_.clear();
    prepare_lexicon();
    operation_list_t loaded_operations;
    const auto loaded = Load_Files(data_, metadata_, lexicon_filename_, loaded_operations, paths);

    loaded_operations_.splice(std::end(loaded_operations_), loaded_operations);
    if(!loaded){
        std::ostringstream os;
        os << "file loading failed";
        if(!paths.empty()){
            os << "; " << paths.size() << " path(s) were not consumed, including '"
               << paths.front().string() << "'";
        }
        last_error_ = os.str();
    }
    return loaded;
}

bool Session::run(){
    last_error_.clear();
    try{
        prepare_lexicon();
    }catch(const std::exception &e){
        last_error_ = std::string("operation setup failed: ") + e.what();
        return false;
    }
    operation_list_t operations;
    operations.splice(std::end(operations), loaded_operations_);
    operations.splice(std::end(operations), pending_operations_);
    return Operation_Dispatcher(data_, metadata_, lexicon_filename_, operations, &last_error_);
}

bool Session::run(OperationArgPkg operation){
    pending_operations_.push_back(std::move(operation));
    return run();
}

bool Session::run(operation_list_t operations){
    pending_operations_.splice(std::end(pending_operations_), operations);
    return run();
}

bool Session::run_script(std::istream &is, feedback_list_t &feedback){
    last_error_.clear();
    operation_list_t operations;
    if(!Load_DCMA_Script(is, feedback, operations)){
        std::ostringstream os;
        Print_Feedback(os, feedback);
        last_error_ = os.str();
        if(last_error_.empty()){
            last_error_ = "script parsing failed";
        }
        return false;
    }
    return run(std::move(operations));
}

bool Session::run_script(const std::string &script, feedback_list_t &feedback){
    std::istringstream is(script);
    return run_script(is, feedback);
}

std::vector<OperationDoc> Session::operation_docs() const{
    std::vector<OperationDoc> out;
    const auto known_operations = Known_Operations();
    out.reserve(known_operations.size());
    for(const auto &known_op : known_operations){
        out.emplace_back(known_op.second.first());
    }
    return out;
}

const std::string & Session::last_error() const{
    return last_error_;
}

} // namespace dcma
