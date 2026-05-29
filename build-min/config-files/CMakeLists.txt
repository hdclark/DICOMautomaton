
# Installation info.
if(WITH_WT)
    install(FILES webserver.conf 
            DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/DICOMautomaton"
    )
    install(DIRECTORY webserver_styles
            DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/DICOMautomaton"
    )
endif()

