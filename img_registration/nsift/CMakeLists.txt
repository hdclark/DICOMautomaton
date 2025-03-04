INCLUDE(${CMAKE_SOURCE_DIR}/img_registration/nsift/IJMacros.txt)

#Change PROJECT_NAME to the name of your project
#PROJECT( itkScaleInvariantFeatureImageFilter )

#The following lines are required to use Dart
ENABLE_TESTING()
INCLUDE(Dart)

#Declare any external dependencies that your project may have here.
#examples include: ITK, VTK, JPEG, PNG, OpenGL, ZLIB, Perl, Java
#If you're not sure what name to use, look in the Modules directory of your
#cmake install and check that a file named Find(Package).cmake exists
SET(Required_Packages
ITK
)

#this foreach loads all of the packages that you specified as required.
#It shouldn't need to be modified.
FOREACH(Package ${Required_Packages})
  LOADPACKAGE(${Package})
ENDFOREACH(Package)

#Set any libraries that your project depends on.
#examples: ITKCommon, VTKRendering, etc
SET(Libraries ${ITK_LIBRARIES})

#the following block of code builds executable for 3D and 4D (3D+time)
#images, and for each of the 3 feature types: n-SIFT (sift),
#global histogram (hist) and reoriented global histogram (rhist)
#the executable name is of the form (image type)(feature type)keys
#e.g. 3D n-SIFT feature = 3Dsiftkeys
ADD_EXECUTABLE( 3Dsiftkeys testnD.cxx )
TARGET_LINK_LIBRARIES ( 3Dsiftkeys ${ITK_LIBRARIES})
SET_TARGET_PROPERTIES( 3Dsiftkeys PROPERTIES
		       COMPILE_FLAGS "-DDIMENSION=3 -DSIFT_FEATURE" )

ADD_EXECUTABLE( 3Dhistkeys testnD.cxx )
TARGET_LINK_LIBRARIES ( 3Dhistkeys ${ITK_LIBRARIES})
SET_TARGET_PROPERTIES( 3Dhistkeys PROPERTIES
		       COMPILE_FLAGS -DDIMENSION=3 )

ADD_EXECUTABLE( 3Drhistkeys testnD.cxx )
TARGET_LINK_LIBRARIES ( 3Drhistkeys ${ITK_LIBRARIES})
SET_TARGET_PROPERTIES( 3Drhistkeys PROPERTIES
		       COMPILE_FLAGS "-DDIMENSION=3 -DREORIENT" )


ADD_EXECUTABLE( 4Dsiftkeys testnD.cxx )
TARGET_LINK_LIBRARIES ( 4Dsiftkeys ${ITK_LIBRARIES})
SET_TARGET_PROPERTIES( 4Dsiftkeys PROPERTIES
		       COMPILE_FLAGS "-DDIMENSION=4 -DSIFT_FEATURE" )

ADD_EXECUTABLE( 4Dhistkeys testnD.cxx )
TARGET_LINK_LIBRARIES ( 4Dhistkeys ${ITK_LIBRARIES})
SET_TARGET_PROPERTIES( 4Dhistkeys PROPERTIES
		       COMPILE_FLAGS -DDIMENSION=4 )

ADD_EXECUTABLE( 4Drhistkeys testnD.cxx )
TARGET_LINK_LIBRARIES ( 4Drhistkeys ${ITK_LIBRARIES})
SET_TARGET_PROPERTIES( 4Drhistkeys PROPERTIES
		       COMPILE_FLAGS "-DDIMENSION=4 -DREORIENT" )


#The following tests duplicate the examples given in the Insight Paper
ADD_TEST(3DSiftImageMatch 3Dsiftkeys --match ${CMAKE_SOURCE_DIR}/brainweb165a10f17.mha  ${CMAKE_SOURCE_DIR}/brainweb1e1a10f20.mha )
ADD_TEST(3DSiftImageSyntheticRotate10 3Dsiftkeys --synthetic --rotate-mid --rotate 10 ${CMAKE_SOURCE_DIR}/brainweb165a10f17.mha )

