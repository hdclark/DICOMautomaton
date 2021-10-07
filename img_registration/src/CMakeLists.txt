
#add_library(            Img_Reg_Shared_obj OBJECT Img_Reg_Shared.cc)
#set_target_properties(  Img_Reg_Shared_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

#add_library(            Img_Reg_Affine_obj OBJECT Img_Reg_Affine.cc)
#set_target_properties(  Img_Reg_Affine_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_executable (run_def_reg
    Run_Def_Reg.cc
#    $<TARGET_OBJECTS:Img_Reg_Shared_obj>
#    $<TARGET_OBJECTS:Img_Reg_Affine_obj>
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

