//ImportDrover.cc - A part of DICOMautomaton 2023. Written by hal clark.

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
#include <filesystem>
#include <vector>

#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorStats.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"

#ifndef DCMA_USE_THRIFT
    #error "Attempted to compile serialization operation without Apache Thrift, which is required"
#endif //DCMA_USE_THRIFT

#include <thrift/transport/TSimpleFileTransport.h>
//#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TZlibTransport.h>
#include <thrift/protocol/TBinaryProtocol.h>
//#include <thrift/protocol/TCompactProtocol.h>
//#include <thrift/protocol/TJSONProtocol.h>
//#include <thrift/protocol/TDebugProtocol.h>

#include "../rpc/gen-cpp/Receiver.h"
#include "../rpc/Serialization.h"

#include "ImportDrover.h"

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;


OperationDoc OpArgDocImportDrover(){
    OperationDoc out;
    out.name = "ImportDrover";

    out.desc = 
        "This operation deserializes a Drover object from a file."
        " It uses Apache Thrift for serialization.";

    out.notes.emplace_back(
        "RPC functionality is currently alpha-quality code, and much is expected to change."
    );

    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The filename to read from.";
    out.args.back().default_val = "in.ts_dcma";
    out.args.back().expected = true;
    out.args.back().examples = { "in.ts_dcma", "/tmp/in.ts_dcma" };

    return out;
}


bool ImportDrover(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& InvocationMetadata,
                  const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto Filename = OptArgs.getValueStr("Filename").value();
    //-----------------------------------------------------------------------------------------------------------------

    const bool permit_read  = true;
    const bool permit_write = false;
    //std::shared_ptr<TSimpleFileTransport> transport;
    auto f_transport = std::make_shared<TSimpleFileTransport>(Filename.c_str(), permit_read, permit_write);
    auto z_transport = std::make_shared<TZlibTransport>(f_transport);

    auto protocol = std::make_shared<TBinaryProtocol>(z_transport);
    //auto protocol = std::make_shared<TCompactProtocol>(z_transport);
    //auto protocol = std::make_shared<TJSONProtocol>(z_transport);
    //auto protocol = std::make_shared<TDebugProtocol>(z_transport);
    ::dcma::rpc::ReceiverClient client(protocol);

    try{
        ::dcma::rpc::Drover d;
        d.read(protocol.get());

        Drover l_DICOM_data;
        Deserialize(d, l_DICOM_data);
        DICOM_data.Consume(l_DICOM_data);
        YLOGINFO("Deserialized Drover object from '" << Filename << "'");

    }catch( const std::exception &e){
        YLOGWARN("Deserialization failed: '" << e.what() << "'");
    }

    return true;
}
