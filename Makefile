# Makefile for grabbing tag values using Imebra library.
# Written by hal clark in October 2011 for Project - DICOMparse

XTRA_FLAGS   = -DUSTREAM_H #-DUSE_ICU_STRINGS 
XTRA_DEFINES =

# For the shared libraries we create locally, we need to inform the linker that the 
# ELF's should look in the directory they were created in for, say, libimebrashim.so.
# This lets us forgo installing libimebrashim everytime we want to recompile it.
#XTRA_LIBS    = -L/usr/local/lib/ -L.   `./Helpers/Compilation_Echo_Rpath.sh` 
XTRA_LIBS    = -L/usr/local/lib/ -L. -isystem imebra20121219/library/imebra/include/  `./Helpers/Compilation_Echo_Rpath.sh`         
XTRA_MISC    = -lm  

# Note: The Imebra library is chock full of warnings. Periodically endure them with -Wall to check code changes!
CC           = `type ccache &>/dev/null && echo -n 'ccache ' ` g++ -std=c++14 -Wno-deprecated -ggdb -O2 -Wall -Wextra -fdiagnostics-color=always -fno-var-tracking-assignments
#CC           = clang++ -std=c++14 -Wno-deprecated -ggdb -O2 -Wall -Wextra
#CC           = `type ccache &>/dev/null && echo -n 'ccache ' ` clang++ -std=c++14 -Wno-deprecated -ggdb -O2 -Wall -Wextra
#CC           = `type ccache &>/dev/null && echo -n 'ccache ' ` g++ -std=c++11 -Wno-deprecated -ggdb -O0 -Wall -Wextra
#CC           = g++ -std=c++11 -Wall -Wno-deprecated -ggdb -O0 
#CC           = g++ -std=c++11 -Wall -Wno-deprecated -ggdb -Ofast -funsafe-loop-optimizations

# Use the environmental compiler. Enable this when using clang's static analyzer (scan-build -V make -j 8).
#CC           = $(CXX) -std=c++11 -ggdb -O2 -Wall -Wextra -Wno-unused-parameter

# This is a list of the commonly-included-everywhere stuff.
COMMON_DEPS = Structs.o Dose_Meld.o Whole_Match.o
COMMON_SRCS = 
COMMON_LIBS = ${XTRA_LIBS} ${XTRA_MISC} ${XTRA_DEFINES} ${XTRA_FLAGS} \
                 -limebrashim -lygor -lexplicator -ldemarcator -pthread


# In order to build a static wxX11 program, you have to compile the source to make
# static versions of the libraries. There is an additional bug in the wxX11 config
# that you cannot have spaces in the directory (LAME!)
#
# The work-around is to compile to some other location. The compilation gives over
# 500 MB though - so place it somewhere suitable.
#
# The compilation can be done with
#
#  cp ./Packages/wxX11...  <path to the directory here>
#  cd  <path to the directory here>
#  ./configure --with-x11 --disable-shared --enable-log --enable-debug -without-odbc --enable-debug_cntxt  
#  make
#
#  and then one needs to use that particular directory's wx-config, as seen below.
# Remember: this is done so that there will be _less_ issues later!
#WXX11_STATIC_HOME_DIR = "/home/hal/wxX11-2.8.12/"


IMEBRA_SRCS = \
 imebra20121219/library/imebra/src/dataHandlerStringUT.cpp \
 imebra20121219/library/imebra/src/data.cpp \
 imebra20121219/library/imebra/src/colorTransformsFactory.cpp \
 imebra20121219/library/imebra/src/dataHandlerTime.cpp \
 imebra20121219/library/imebra/src/transaction.cpp \
 imebra20121219/library/imebra/src/colorTransform.cpp \
 imebra20121219/library/imebra/src/dataHandlerStringDS.cpp \
 imebra20121219/library/imebra/src/dataHandlerString.cpp \
 imebra20121219/library/imebra/src/MONOCHROME2ToYBRFULL.cpp \
 imebra20121219/library/imebra/src/dataHandlerStringPN.cpp \
 imebra20121219/library/imebra/src/image.cpp \
 imebra20121219/library/imebra/src/drawBitmap.cpp \
 imebra20121219/library/imebra/src/dataHandlerDateTimeBase.cpp \
 imebra20121219/library/imebra/src/dataHandlerStringLT.cpp \
 imebra20121219/library/imebra/src/dataHandlerDateTime.cpp \
 imebra20121219/library/imebra/src/dataHandlerStringAE.cpp \
 imebra20121219/library/imebra/src/YBRFULLToRGB.cpp \
 imebra20121219/library/imebra/src/dicomCodec.cpp \
 imebra20121219/library/imebra/src/transform.cpp \
 imebra20121219/library/imebra/src/charsetsList.cpp \
 imebra20121219/library/imebra/src/dataHandlerStringST.cpp \
 imebra20121219/library/imebra/src/viewHelper.cpp \
 imebra20121219/library/imebra/src/transformsChain.cpp \
 imebra20121219/library/imebra/src/YBRPARTIALToRGB.cpp \
 imebra20121219/library/imebra/src/dataGroup.cpp \
 imebra20121219/library/imebra/src/dataHandlerStringCS.cpp \
 imebra20121219/library/imebra/src/dataHandlerDate.cpp \
 imebra20121219/library/imebra/src/transformHighBit.cpp \
 imebra20121219/library/imebra/src/dataHandlerStringSH.cpp \
 imebra20121219/library/imebra/src/buffer.cpp \
 imebra20121219/library/imebra/src/codec.cpp \
 imebra20121219/library/imebra/src/dataHandlerStringAS.cpp \
 imebra20121219/library/imebra/src/dataHandlerStringUnicode.cpp \
 imebra20121219/library/imebra/src/dataHandlerStringLO.cpp \
 imebra20121219/library/imebra/src/VOILUT.cpp \
 imebra20121219/library/imebra/src/MONOCHROME1ToMONOCHROME2.cpp \
 imebra20121219/library/imebra/src/RGBToYBRFULL.cpp \
 imebra20121219/library/imebra/src/dataHandlerStringIS.cpp \
 imebra20121219/library/imebra/src/MONOCHROME2ToRGB.cpp \
 imebra20121219/library/imebra/src/RGBToYBRPARTIAL.cpp \
 imebra20121219/library/imebra/src/YBRFULLToMONOCHROME2.cpp \
 imebra20121219/library/imebra/src/dataSet.cpp \
 imebra20121219/library/imebra/src/dataHandlerStringUI.cpp \
 imebra20121219/library/imebra/src/jpegCodec.cpp \
 imebra20121219/library/imebra/src/waveform.cpp \
 imebra20121219/library/imebra/src/MONOCHROME1ToRGB.cpp \
 imebra20121219/library/imebra/src/dicomDir.cpp \
 imebra20121219/library/imebra/src/modalityVOILUT.cpp \
 imebra20121219/library/imebra/src/PALETTECOLORToRGB.cpp \
 imebra20121219/library/imebra/src/RGBToMONOCHROME2.cpp \
 imebra20121219/library/imebra/src/codecFactory.cpp \
 imebra20121219/library/imebra/src/LUT.cpp \
 imebra20121219/library/imebra/src/dicomDict.cpp \
 imebra20121219/library/imebra/src/dataHandler.cpp \
 imebra20121219/library/base/src/criticalSection.cpp \
 imebra20121219/library/base/src/baseObject.cpp \
 imebra20121219/library/base/src/charsetConversion.cpp \
 imebra20121219/library/base/src/stream.cpp \
 imebra20121219/library/base/src/streamController.cpp \
 imebra20121219/library/base/src/thread.cpp \
 imebra20121219/library/base/src/memory.cpp \
 imebra20121219/library/base/src/exception.cpp \
 imebra20121219/library/base/src/streamReader.cpp \
 imebra20121219/library/base/src/memoryStream.cpp \
 imebra20121219/library/base/src/baseStream.cpp \
 imebra20121219/library/base/src/streamWriter.cpp \
 imebra20121219/library/base/src/huffmanTable.cpp 


.PHONY: all

all: overlaydosedata automaton pacs_ingress petct_perfusion_analysis \
     pacs_refresh pacs_verify_filesystem_store



########################################################################
#####                        Binary Targets                        #####
########################################################################

Structs.o: Structs.cc Structs.h
	${CC} -fPIC -c Structs.cc

Dose_Meld.o: Dose_Meld.cc Dose_Meld.h Structs.o
	${CC} -fPIC -c Dose_Meld.cc

Whole_Match.o: Whole_Match.cc Whole_Match.h Structs.o
	${CC} -fPIC -c Whole_Match.cc 

libimebrashim.so: Imebra_Shim.cc Imebra_Shim.h Structs.o
	${CC} -shared -Wl,-soname,libimebrashim.so -o libimebrashim.so \
	      -fPIC Imebra_Shim.cc Structs.o ${IMEBRA_SRCS}            \
	      ${XTRA_LIBS} ${XTRA_MISC} ${XTRA_DEFINES} ${XTRA_FLAGS}  \
	          -lygor -Wno-unused-parameter -w 

pacs_ingress: PACS_Ingress.cc ${COMMON_DEPS} libimebrashim.so
	${CC} PACS_Ingress.cc ${COMMON_DEPS} \
	      -o pacs_ingress ${IMEBRA_COMPS} -lpqxx -lpq ${COMMON_LIBS}
#${XTRA_LIBS} ${XTRA_MISC} ${XTRA_DEFINES} ${XTRA_FLAGS} \
#	      -o pacs_ingress -lpqxx -lpq ${COMMON_LIBS}

pacs_refresh: PACS_Refresh.cc ${COMMON_DEPS} libimebrashim.so
	${CC} PACS_Refresh.cc ${COMMON_DEPS} \
	      -o pacs_refresh ${IMEBRA_COMPS} -lpqxx -lpq ${COMMON_LIBS}

pacs_verify_filesystem_store: PACS_Verify_Filesystem_Store.cc ${COMMON_DEPS} libimebrashim.so
	${CC} PACS_Verify_Filesystem_Store.cc ${COMMON_DEPS} \
	      -o pacs_verify_filesystem_store ${IMEBRA_COMPS} -lpqxx -lpq ${COMMON_LIBS}

overlaydosedata: Overlay_Dose_Data.cc ${COMMON_DEPS} libimebrashim.so 
	${CC} Overlay_Dose_Data.cc ${COMMON_DEPS}                      \
	      -o overlaydosedata ${IMEBRA_COMPS} ${COMMON_LIBS}        \
	          -L/usr/X11R6/lib  -lX11 -lXi -lXmu -lglut -lGL -lGLU -lm 

automaton: Automaton.cc ${COMMON_DEPS} libimebrashim.so
	${CC} Automaton.cc ${COMMON_DEPS}                              \
	      -o automaton ${IMEBRA_COMPS} -lcsvtools -lpqxx -lpq ${COMMON_LIBS}


petct_perfusion_analysis: PETCT_Perfusion_Analysis.cc ${COMMON_DEPS} libimebrashim.so \
    $(wildcard YgorImages_*h) $(wildcard YgorImages_*cc) ContourFactories.h
	${CC} PETCT_Perfusion_Analysis.cc ${COMMON_DEPS}  \
          $(wildcard YgorImages_*cc) \
	      -o petct_perfusion_analysis ${IMEBRA_COMPS} \
          -lsfml-graphics -lsfml-audio -lsfml-window -lsfml-system \
          -ljansson -lpqxx -lpq ${COMMON_LIBS}


########################################################################
#####                      Stripping Targets                       #####
########################################################################
strip: all
	strip automaton overlaydosedata petct_perfusion_analysis libimebrashim.so 
# .so might need --strip-unneeded (?)

########################################################################
#####                       Install Targets                        #####
########################################################################
install: all strip
	install -Dm644 libimebrashim.so          /usr/lib/
	install -Dm777 automaton                 /usr/bin/
	install -Dm777 overlaydosedata           /usr/bin/
	install -Dm777 petct_perfusion_analysis  /usr/bin/
	install -Dm777 pacs_refresh              /usr/bin/
	install -Dm777 pacs_ingress              /usr/bin/
	ldconfig -n /usr/lib/

########################################################################
#####                      Cleaning Targets                        #####
########################################################################
clean: 
	for i in libimebrashim.so overlaydosedata automaton \
             petct_perfusion_analysis \
	         Dose_Meld.o Whole_Match.o Structs.o ; do   \
            test -f "$${i}"   &&   rm "$${i}" ;             \
        done ;

