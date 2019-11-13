
# Installation info.
if(USE_WT)
    install(FILES
        webserver.conf DESTINATION /etc/DICOMautomaton
    )
    install(DIRECTORY
        webserver_styles DESTINATION /etc/DICOMautomaton
    )
endif()

