
add_library(            Perfusion_SCDI_obj OBJECT Perfusion_SCDI.cc)
set_target_properties(  Perfusion_SCDI_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Perfusion_SCSI_obj OBJECT Perfusion_SCSI.cc)
set_target_properties(  Perfusion_SCSI_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Common_obj OBJECT Common.cc)
set_target_properties(  Common_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_executable (run_model
    Run_Model.cc
    $<TARGET_OBJECTS:Perfusion_SCDI_obj>
    $<TARGET_OBJECTS:Perfusion_SCSI_obj>
    $<TARGET_OBJECTS:Common_obj>
)
target_link_libraries (run_model
    "$<$<BOOL:${WITH_NLOPT}>:${NLOPT_LIBRARIES}>"
    ygor 
    Boost::filesystem
    Boost::serialization
    Boost::iostreams
    Boost::thread
    Boost::system
    m
    Threads::Threads
)

add_executable (gen_test_inputs
    Generate_Test_Inputs.cc
)
target_link_libraries (gen_test_inputs
    ygor 
    m
    Threads::Threads
)

install(TARGETS run_model
                gen_test_inputs
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

