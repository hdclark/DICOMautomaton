//Partitioned_Image_Voxel_Visitor_Mutator.h.
#pragma once


#include <cmath>
#include <experimental/any>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <set>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"

template <class T> class contour_collection;


struct PartitionedImageVoxelVisitorMutatorUserData {

    // Algorithmic changes passed through to the driver function.
    Mutate_Voxels_Opts  mutation_opts;

    std::function<void(long int, long int, long int, float &)> f_bounded;   // Applied to voxels bounded by contours.
    std::function<void(long int, long int, long int, float &)> f_unbounded; // Applied to voxels NOT bounded by contours.
    std::function<void(long int, long int, long int, float &)> f_visitor;   // Applied to all voxels.
    
    std::string description; // If non-empty, used to update image metadata.
};


bool PartitionedImageVoxelVisitorMutator(planar_image_collection<float,double>::images_list_it_t first_img_it,
                        std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                        std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                        std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                        std::experimental::any );





