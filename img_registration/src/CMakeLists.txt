

add_library(            Alignment_ABC_obj OBJECT Alignment_ABC.cc)
add_library(            Alignment_TPS_obj OBJECT ../../src/Alignment_TPSRPM.cc)
#add_library(tpsrpm
#        ../../src/Alignment_TPSRPM.cc)
add_library(centroid
        ../../src/Alignment_Rigid.cc)
set_target_properties(  Alignment_ABC_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_executable (run_def_reg
    Run_Def_Reg.cc
    $<TARGET_OBJECTS:Alignment_ABC_obj>
    $<TARGET_OBJECTS:Alignment_TPSRPM_obj>
)

target_link_libraries (run_def_reg
    "$<$<BOOL:${WITH_NLOPT}>:${NLOPT_LIBRARIES}>"
    ygor
#    tpsrpm
    centroid
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

