
#include <memory>
#include <vector>

#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorMath.h"
#include "YgorImages.h"

#include "../Structs.h"

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

#include "gen-cpp/Receiver.h"
#include "Serialization.h"

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace ::dcma::rpc;

class ReceiverHandler : virtual public ReceiverIf {
  public:
    ReceiverHandler() {
        YLOGINFO("Initialization code goes here");
    }

    void GetSupportedOperations(std::vector<KnownOperation> & _return, const OperationsQuery& query) {
        YLOGINFO("GetSupportedOperations implementation goes here");

        // Simple test.
        {
            ::vec3<double> a;
            dcma::rpc::vec3_double b;
            Serialize(a, b);
            Deserialize(b, a);
        }

        // Slightly more complicated test.
        {
            ::contour_of_points<double> a;
            dcma::rpc::contour_of_points_double b;
            Serialize(a, b);
            Deserialize(b, a);
        }

        // Slightly more complicated test.
        {
            ::contour_collection<double> a;
            dcma::rpc::contour_collection_double b;
            Serialize(a, b);
            Deserialize(b, a);
        }

        // Full-blown test of Drover serialization.
        {
            ::Drover a;
            dcma::rpc::Drover b;
            Serialize(a, b);
            Deserialize(b, a);
            Serialize(a, b);
        }
    }

    void LoadFiles(LoadFilesResponse& _return, const std::vector<LoadFilesQuery> & server_filenames) {
        YLOGINFO("LoadFiles implementation goes here");
    }
};

// Example of how to use this class:
//
//int main(int argc, char **argv){
//    int port = 9090;
//
//    auto handler = std::make_shared<ReceiverHandler>();
//    auto processor = std::make_shared<ReceiverProcessor>(handler);
//
//    auto transport_server = std::make_shared<TServerSocket>( port );
//    auto transport_factory = std::make_shared<TBufferedTransportFactory>();
//    auto protocol_factory = std::make_shared<TBinaryProtocolFactory>();
//
//    TSimpleServer server(processor, transport_server, transport_factory, protocol_factory);
//    server.serve();
//
//    return 0;
//}

