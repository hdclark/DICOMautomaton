
#FILE(GLOB thrift_autogen_srcs "/*/*cc")
#add_library( Thrift_objs OBJECT ${thrift_autogen_srcs} )

add_library( Thrift_objs OBJECT
    gen-cpp/DCMA_constants.cc
    gen-cpp/DCMA_types.cc
    gen-cpp/Receiver.cc
    Serialization.cc
#    Receiver_server.impl.cc
#    Receiver_client.impl.cc
)
set_target_properties( Thrift_objs PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

