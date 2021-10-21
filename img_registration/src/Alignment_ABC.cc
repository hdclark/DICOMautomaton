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
    // vec3's are just three numbers: x, y, and z coordinates, which are all 'double'-sized floats.

    // The following is an example of iterating over images in the volume, iterating over the voxels (pixels) in an
    // image, computing each voxel's 3D position, reading the voxel intensity, and writing a new voxel intensity.
    {
        // Make a copy of the moving image set that is NOT const.
        // const == read-only, so this will allow us to modify the images and voxels.
        planar_image_collection<float, double> copy_of_moving(moving);
        for(auto &img : copy_of_moving.images){
            const long int N_rows  = img.rows;
            const long int N_cols  = img.columns;
            const long int N_chnls = img.channels; // Each voxel can have multiple channels (e.g., r, g, b), but most medical images have a single channel.
            if(N_chnls) FUNCWARN("Multiple channels detected. Ignoring all but the first channel");
            const long int channel = 0;

            for(long int row = 0; row < N_rows; ++row){
                for(long int col = 0; col < N_cols; ++col){

                    // Position of the voxel.
                    //
                    // Access x, y, and z components like 'pos.x' if needed.
                    // Can write to a stream directly like 'std::cout << pos << std::endl;' .
                    const vec3<double> pos = img.position(row, col);

                    // Read the voxel intensity / strength / value.
                    const float val = img.value(row, col, channel);

                    // Write a new voxel intensity / strength / value, in this case adding 1.23 to the previous value.
                    img.reference(row, col, channel) = val + 1.23;

                    // Write a new voxel intensity / strength / value if we're close to some arbitrary point in space.
                    // This will draw a sphere in the image.
                    if(pos.distance( img.position(0,0) ) < 10.0){
                        img.reference(row, col, channel) = 456.789;
                    }
                }
            }
        }
    }

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

