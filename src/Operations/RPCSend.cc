//RPCSend.cc - A part of DICOMautomaton 2022. Written by hal clark.

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
#include "../Regex_Selectors.h"

#ifndef DCMA_USE_THRIFT
    #error "Attempted to compile RPC client without Apache Thrift, which is required"
#endif //DCMA_USE_THRIFT

#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>

#include "../rpc/gen-cpp/Receiver.h"
#include "../rpc/Serialization.h"

#include "RPCSend.h"

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;


OperationDoc OpArgDocRPCSend(){
    OperationDoc out;
    out.name = "RPCSend";

    out.desc = 
        "This operation sends a remote procedure call (RPC) to a corresponding client for distributed computing.";

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


bool RPCSend(Drover &DICOM_data,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto Port = std::stol( OptArgs.getValueStr("Port").value() );
    //-----------------------------------------------------------------------------------------------------------------

    std::shared_ptr<TTransport> transport;
    //auto transport = std::make_shared<TTransport>();
    transport = std::make_shared<TSocket>("localhost", Port);
    transport = std::make_shared<TBufferedTransport>(transport);

    auto protocol = std::make_shared<TBinaryProtocol>(transport);
    ::dcma::rpc::ReceiverClient client(protocol);

    //using namespace ::dcma::rpc;

    try{
        transport->open();

        std::vector<::dcma::rpc::KnownOperation> known_ops;
        ::dcma::rpc::OperationsQuery q;
        client.GetSupportedOperations(known_ops, q);

        YLOGINFO("Implementation of client goes here");
    }catch( const std::exception &e){
        YLOGWARN("Client failed: '" << e.what() << "'");
    }
    transport->close();

    return true;
}

