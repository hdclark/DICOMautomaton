//Per_ROI_Time_Courses.h.
#pragma once


#include <cmath>
#include <cstdint>
#include <experimental/any>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <string>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"

template <class T, class R> class planar_image_collection;


//User data struct for harvesting data afterward. Note that, because the driver routine calls the supplied functional
// several times (depending on the user's grouping), ensure that the data in this struct can be incrementally computed.
//
// For example, a sum of all pixel values + count of all pixels will be easier to accomplish than directly computing
// an average. The average requires a distinct final step which will be hard to do with the incremental approach.
//

struct ComputePerROITimeCoursesUserData {

    //const auto boxr = 2; //The inclusive 'radius' of the square box to use to average nearby pixels. Controls amount of spatial averaging.
    //const auto min_datum = 3; //The minimum number of nearby pixels needed to proceed with each average/variance estimate/etc.. 
                          // Note this is very sensistive to boxr. If boxr = 1 the max min_datum is 5. If boxr = 2 the max min_datum 
                          // is 13. In general, it is best to keep it at 3 or maybe 5 if you want to be extra precise about interpreting
                          // variance estimates.

    std::map<std::string, samples_1D<double>> time_courses;
    std::map<std::string, uint64_t>           total_voxel_count; //Number of voxels in ROI, over (x,y,z,t).
    std::map<std::string, uint64_t>           voxel_count; //Number of voxels in ROI, over (x,y,z).

};

bool ComputePerROICourses(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::experimental::any ud );

