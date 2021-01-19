add_library(            CPD_Shared_obj OBJECT CPD_Shared.cc)
set_target_properties(  CPD_Shared_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            CPD_Affine_obj OBJECT CPD_Affine.cc)
set_target_properties(  CPD_Affine_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            CPD_Rigid_obj OBJECT CPD_Rigid.cc)
set_target_properties(  CPD_Rigid_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            CPD_Nonrigid_obj OBJECT CPD_Nonrigid.cc)
set_target_properties(  CPD_Nonrigid_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_executable (run_def_reg
    Run_Def_Reg.cc
    $<TARGET_OBJECTS:CPD_Shared_obj>
    $<TARGET_OBJECTS:CPD_Rigid_obj>
    $<TARGET_OBJECTS:CPD_Affine_obj>
    $<TARGET_OBJECTS:CPD_Nonrigid_obj>
)
target_link_libraries (run_def_reg
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

install(TARGETS run_def_reg
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

