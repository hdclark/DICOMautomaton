//Contour_Similarity.h.
#pragma once


#include <cmath>
#include <cstdint>
#include <any>
#include <functional>
#include <limits>
#include <limits>
#include <list>
#include <map>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"

template <class T, class R> class planar_image_collection;
template <class T> class contour_collection;


//User data struct for harvesting data afterward. Note that, because the driver routine calls the supplied functional
// several times (depending on the user's grouping), ensure that the data in this struct can be incrementally computed.
//
// For example, a sum of all pixel values + count of all pixels will be easier to accomplish than directly computing
// an average. The average requires a distinct final step which will be hard to do with the incremental approach.
//

struct ComputeContourSimilarityUserData {

    //const auto boxr = 2; //The inclusive 'radius' of the square box to use to average nearby pixels. Controls amount of spatial averaging.
    //const auto min_datum = 3; //The minimum number of nearby pixels needed to proceed with each average/variance estimate/etc.. 
                          // Note this is very sensistive to boxr. If boxr = 1 the max min_datum is 5. If boxr = 2 the max min_datum 
                          // is 13. In general, it is best to keep it at 3 or maybe 5 if you want to be extra precise about interpreting
                          // variance estimates.

    uint64_t contour_L_voxels = 0; // Number of voxels present in contour L. (Surrogate for volume.)
    uint64_t contour_R_voxels = 0; // Number of voxels present in contour R. (Surrogate for volume.)
    uint64_t overlap_voxels   = 0; // Number of voxels present in both contours. (Surrogate for overlap.)

    // Compute the Dice similarity coefficient with current voxel counts.
    double Dice_Coefficient() const {
        if( (contour_L_voxels == 0) && (contour_R_voxels == 0) ) return std::numeric_limits<double>::quiet_NaN();
        return (2.0*overlap_voxels) / ( (1.0*contour_L_voxels) + (1.0*contour_R_voxels) );
    };
  
    // Compute the Jaccard similarity coefficient with current voxel counts.
    double Jaccard_Coefficient() const {
        if( (contour_L_voxels == 0) && (contour_R_voxels == 0) ) return std::numeric_limits<double>::quiet_NaN();
        return (1.0*overlap_voxels) / ( (1.0*contour_L_voxels) + (1.0*contour_R_voxels) - (1.0*overlap_voxels) );
    };
  

    void Clear() {
        contour_L_voxels = 0;
        contour_R_voxels = 0;
        overlap_voxels   = 0;
        return;
    };

};

bool ComputeContourSimilarity(planar_image_collection<float,double> &,
                              std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                              std::list<std::reference_wrapper<contour_collection<double>>>,
                              std::any ud );

