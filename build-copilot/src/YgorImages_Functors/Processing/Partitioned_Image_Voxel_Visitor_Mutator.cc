
#include <exception>
#include <functional>
#include <list>
#include <stdexcept>

#include "../ConvenienceRoutines.h"
#include "Partitioned_Image_Voxel_Visitor_Mutator.h"
#include "YgorImages.h"
#include "YgorMisc.h"
#include "YgorLog.h"

template <class T> class contour_collection;

bool PartitionedImageVoxelVisitorMutator(planar_image_collection<float,double>::images_list_it_t first_img_it,
                        std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                        std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                        std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                        std::any user_data){

    //This routine walks over all voxels in the first image, overwriting voxel values (or just visiting them) according
    // to the routines provided by the user. The function called depends on whether the voxel is interior or exterior to
    // the specified ROI(s) boundaries.
    //
    // NOTE: This routine currently ignores all except the first image. You can save computational effort by
    //       only bothering to hand this routine time-independent image arrays (i.e., arrays with spatial but
    //       not temporal indices).

    //This routine requires a valid PartitionedImageVoxelVisitorMutatorUserData struct packed into the user_data. 
    PartitionedImageVoxelVisitorMutatorUserData *user_data_s;
    try{
        user_data_s = std::any_cast<PartitionedImageVoxelVisitorMutatorUserData *>(user_data);
    }catch(const std::exception &e){
        YLOGWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }
    
    if( !user_data_s->f_bounded
    &&  !user_data_s->f_unbounded
    &&  !user_data_s->f_visitor ){
        throw std::invalid_argument("Nothing to do; no valid operation provided. Refusing to continue.");
    }
    
    if(ccsl.empty()){
        throw std::invalid_argument("No contours provided. Cannot continue");
    }

    std::list<std::reference_wrapper<planar_image<float,double>>> selected_imgs;
    for(auto &img_it : selected_img_its) selected_imgs.push_back( std::ref(*img_it) );

    Mutate_Voxels<float,double>( std::ref(*first_img_it),
                                 selected_imgs, 
                                 ccsl, 
                                 user_data_s->mutation_opts, 
                                 user_data_s->f_bounded,
                                 user_data_s->f_unbounded,
                                 user_data_s->f_visitor );


    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    if( !(user_data_s->description.empty()) ){
        UpdateImageDescription( std::ref(*first_img_it), user_data_s->description );
    }
    UpdateImageWindowCentreWidth( std::ref(*first_img_it) );

    return true;
}





