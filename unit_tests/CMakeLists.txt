#include_directories(../src)
#
#add_library(tps
#        ../src/Alignment_TPSRPM.cc)
#
#add_library(centroid
#        ../src/Alignment_Rigid.cc)
#
#add_executable (alignment_tps_unit_test
#        Alignment_TPSRPM.cc
#        compile_and_run.sh
#        Main.cc
#        )
#
#target_link_libraries (alignment_tps_unit_test
#        ygor
#        tps
#        centroid
#        )
#
#add_subdirectory(doctest)