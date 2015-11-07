

#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include <experimental/any>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"


struct DBSCANTimeCoursesUserData {
    //Input values.
    size_t MinPts;    //DBSCAN algorithm paramater.
    double Eps;       //DBSCAN algorithm parameter.
   
    //Output values.
    size_t NumberOfClusters;

};

bool DBSCANTimeCourses(planar_image_collection<float,double>::images_list_it_t first_img_it,
                       std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                       std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                       std::experimental::any );

