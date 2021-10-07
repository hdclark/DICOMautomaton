//Alignment_ABC.cc -- A part of DICOMautomaton 2021. Written by hal clark, ...
//
// This file is meant to contain an implementation of the deformable registration algorithm "ABC".

#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <limits>
#include <map>
#include <memory>
#include <string>    
#include <optional> 
#include <vector>

#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.

#include <boost/filesystem.hpp> // Needed for Boost filesystem access.

#include <eigen3/Eigen/Dense> //Needed for Eigen library dense matrices.

#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for samples_1D.
#include "YgorImages.h"       //Needed for planar_image_collection.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Alignment_ABC.h"


// This function is where the deformable registration algorithm should be implemented.
std::optional<AlignViaABCTransform>
AlignViaABC(AlignViaABCParams & params,
            const planar_image_collection<float, double> & moving,
            const planar_image_collection<float, double> & stationary ){

    if(moving.images.empty() || stationary.images.empty()){
        FUNCWARN("Unable to perform ABC alignment: an image array is empty");
        return std::nullopt;
    }

    // The planar_image_collection class is documented at
    // https://github.com/hdclark/Ygor/blob/master/src/YgorImages.h#L286
    // but it is effectively a simple wrapper around a 2D std::vector of floats with one float per pixel (voxel).
    // Images also have a position in 3D space (vec3).
    // vec3's are just three numbers: x, y, and z coordinates.

    // The following is an example of using Eigen for matrices, in case it is needed.
    {
        Eigen::Matrix4d A;
        A <<  1.0,  0.0,  0.0,  0.0,
              0.0,  0.0,  1.0,  0.0,
             -3.0,  3.0, -2.0, -1.0,
              2.0, -2.0,  1.0,  1.0;
        auto AT = A.transpose();

        auto C = A * AT;

        double coeff_sum = 0.0;
        for(int i = 0; i < 4; ++i){
            for(int j = 0; j < 4; ++j){
                coeff_sum += C(i,j) * 1.23;
            }
        }

        FUNCINFO("The Eigen example coefficient sum is " << coeff_sum);
    }


    // -----------------------------------------------
    // Implement algorithm here.
    //
    // Note that the moving image array should not be modified! A temporary should be made if this is needed, but often it
    // won't be because the transformation can create a temporary copy on-the-fly. Here is how to make a mutable copy in
    // case it is needed:

    // ...
    // ...
    // ...
    //    Note: if the algorithm fails at some point, emit a warning using FUNCWARN() (see above) and return std::nullopt .
    // ...
    // ...
    // ...

    // -----------------------------------------------


    // This structure is described in Alignment_ABC.h. Finding this transform is the ultimate goal of this algorithm.
    // For now, we'll leave it undefined. But a valid AlignViaABCTransform should be created and returned if the algorithm
    // successfully completes.
    AlignViaABCTransform transform;
    return transform;
}

