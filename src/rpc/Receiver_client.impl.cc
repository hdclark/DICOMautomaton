
#include <memory>
#include <vector>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"

#include "../Structs.h"

#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>

#include "gen-cpp/Receiver.h"
#include "Serialization.h"

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;

using namespace ::dcma::rpc;

// Example client usage:
int main(int argc, char **argv){
    int port = 9090;

    auto transport = std::make_shared<TTransport>();
    transport = std::make_shared<TSocket>("localhost", port);
    transport = std::make_shared<TBufferedTransport>(transport);

    auto protocol = std::make_shared<TBinaryProtocol>(transport);
    ReceiverClient client(protocol);

    try{
        transport->open();

        std::vector<KnownOperation> known_ops;
        OperationsQuery q;
        client.GetSupportedOperations(known_ops, q);

        FUNCINFO("Implementation of client goes here");
    }catch( const std::exception &e){
        FUNCWARN("Client failed: '" << e.what() << "'");
    }
    transport->close();

    return 0;
}

