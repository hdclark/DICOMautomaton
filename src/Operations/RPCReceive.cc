//RPCReceive.cc - A part of DICOMautomaton 2023. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <vector>

#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorStats.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"

#include "../Structs.h"
#include "../Metadata.h"
#include "../Regex_Selectors.h"
#include "../Operation_Dispatcher.h"
#include "../Script_Loader.h"

#ifndef DCMA_USE_THRIFT
    #error "Attempted to compile RPC server without Apache Thrift, which is required"
#endif //DCMA_USE_THRIFT

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

#include "../rpc/gen-cpp/Receiver.h"
#include "../rpc/Serialization.h"

#include "RPCReceive.h"


using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

class ReceiverHandler : virtual public ::dcma::rpc::ReceiverIf {
  public:
    ReceiverHandler() {
        YLOGINFO("RPC initialization complete, awaiting procedure calls");
    }

    void GetSupportedOperations(std::vector<::dcma::rpc::KnownOperation> & _return,
                                const ::dcma::rpc::OperationsQuery& query) {
        YLOGINFO("GetSupportedOperations procedure invoked");

        // Simple test.
        {
            ::vec3<double> a;
            ::dcma::rpc::vec3_double b;
            Serialize(a, b);
            Deserialize(b, a);
        }

        // Slightly more complicated test.
        {
            ::contour_of_points<double> a;
            ::dcma::rpc::contour_of_points_double b;
            Serialize(a, b);
            Deserialize(b, a);
        }

        // Slightly more complicated test.
        {
            ::contour_collection<double> a;
            ::dcma::rpc::contour_collection_double b;
            Serialize(a, b);
            Deserialize(b, a);
        }

        // Full-blown test of Drover serialization.
        {
            ::Drover a;
            ::dcma::rpc::Drover b;
            Serialize(a, b);
            Deserialize(b, a);
            Serialize(a, b);
        }

        // Enumerate supported operations.
        const auto known_ops = Known_Operations_and_Aliases();
        _return.reserve(known_ops.size());
        for(const auto& ko_t : known_ops){
            const auto op_name = ko_t.first;
            _return.emplace_back();
            _return.back().name = op_name;
        }
        YLOGINFO("GetSupportedOperations procedure completed");
    }

    void LoadFiles(::dcma::rpc::LoadFilesResponse& _return,
                   const std::vector<::dcma::rpc::LoadFilesQuery> & server_filenames) {
        YLOGINFO("LoadFiles procedure invoked");
        YLOGINFO("LoadFiles procedure completed");
    }

    void ExecuteScript(::dcma::rpc::ExecuteScriptResponse& _return,
                       const ::dcma::rpc::ExecuteScriptQuery& query,
                       const std::string& script) {
        YLOGINFO("ExecuteScript procedure invoked");

        // Deserialize the query input.
        ::Drover l_DICOM_data;
        ::metadata_map_t l_InvocationMetadata;
        std::string l_FilenameLex;
        std::string l_script;

        YLOGINFO("Deserializing state");
        Deserialize(query.drover, l_DICOM_data);
        Deserialize(query.invocation_metadata, l_InvocationMetadata);
        Deserialize(query.filename_lex, l_FilenameLex);
        Deserialize(script, l_script);

        // Execute the script.
        YLOGINFO("Executing script");
        std::list<script_feedback_t> feedback;
        std::stringstream ss( l_script );
        std::list<OperationArgPkg> op_list;
        bool l_ret = Load_DCMA_Script( ss, feedback, op_list );
        if(!l_ret){
            YLOGWARN("Parsing script failed");

        }else{
            l_ret = Operation_Dispatcher(l_DICOM_data,
                                         l_InvocationMetadata,
                                         l_FilenameLex,
                                         op_list);
        }
        if(!l_ret){
            YLOGWARN("Script execution failed");
        }

        // Serialize the outputs.
        YLOGINFO("Serializing state");
        Serialize(l_ret, _return.success);
        Serialize(l_DICOM_data, _return.drover);
        Serialize(l_InvocationMetadata, _return.invocation_metadata);
        Serialize(l_FilenameLex, _return.filename_lex);
        YLOGINFO("ExecuteScript procedure completed");
    }
};


OperationDoc OpArgDocRPCReceive(){
    OperationDoc out;
    out.name = "RPCReceive";
    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: RPC");
    out.tags.emplace_back("category: networking");

    out.desc = 
        "This operation launches a server that accepts remote procedure calls (RPC) for distributed computing.";

    out.notes.emplace_back(
        "RPC functionality is currently alpha-quality code, and much is expected to change."
    );

    out.args.emplace_back();
    out.args.back().name = "Port";
    out.args.back().desc = "The port number to listen on.";
    out.args.back().default_val = "9090";
    out.args.back().expected = true;
    out.args.back().examples = { "13", "8080", "9090", "16378" };

    return out;
}



bool RPCReceive(Drover & /*DICOM_data*/,
                const OperationArgPkg& OptArgs,
                std::map<std::string, std::string>& /*InvocationMetadata*/,
                const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto Port = std::stol( OptArgs.getValueStr("Port").value() );
    //-----------------------------------------------------------------------------------------------------------------

    //using namespace ::dcma::rpc;

    auto handler = std::make_shared<ReceiverHandler>();
    auto processor = std::make_shared<::dcma::rpc::ReceiverProcessor>(handler);

    auto transport_server = std::make_shared<TServerSocket>( Port );
    auto input_transport_factory = std::make_shared<TBufferedTransportFactory>();
    auto output_transport_factory = std::make_shared<TBufferedTransportFactory>();
    auto input_protocol_factory = std::make_shared<TBinaryProtocolFactory>();
    auto output_protocol_factory = std::make_shared<TBinaryProtocolFactory>();

    YLOGINFO("Launching RPC server");
    //TSimpleServer server(processor, transport_server, transport_factory, protocol_factory);
    TSimpleServer server(processor,
                         transport_server, input_transport_factory, output_transport_factory,
                         input_protocol_factory, output_protocol_factory);
    server.serve();

    return true;
}

