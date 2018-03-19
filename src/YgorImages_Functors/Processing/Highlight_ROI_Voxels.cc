
#include <exception>
#include <functional>
#include <list>
#include <stdexcept>

#include "../ConvenienceRoutines.h"
#include "Highlight_ROI_Voxels.h"
#include "YgorImages.h"
#include "YgorMisc.h"

template <class T> class contour_collection;

bool HighlightROIVoxels(planar_image_collection<float,double>::images_list_it_t first_img_it,
                        std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                        std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                        std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                        std::experimental::any user_data){

    //This routine walks over all voxels in the first image, overwriting voxel values. The value can depend on whether
    // the voxel is interior or exterior to the specified ROI(s) boundaries.
    //
    // NOTE: This routine currently ignores all except the first image. You can save computational effort by
    //       only bothering to hand this routine time-independent image arrays (i.e., arrays with spatial but
    //       not temporal indices).

    //This routine requires a valid HighlightROIVoxelsUserData struct packed into the user_data. 
    HighlightROIVoxelsUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<HighlightROIVoxelsUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    if(!user_data_s->overwrite_interior && !user_data_s->overwrite_exterior){
        FUNCWARN("Nothing to do. Select either interior or exterior. Currently a no-op, but proceeding anyway");
    }
    
    if(ccsl.empty()){
        throw std::invalid_argument("Missing contour info needed for voxel colouring. Cannot continue");
    }

    //Modify the first image as per the mask and specified behaviour.
    Mutate_Voxels_Opts ebv_opts;
    ebv_opts.editstyle      = Mutate_Voxels_Opts::EditStyle::InPlace;
    ebv_opts.aggregate      = Mutate_Voxels_Opts::Aggregate::First;
    ebv_opts.adjacency      = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
    ebv_opts.maskmod        = Mutate_Voxels_Opts::MaskMod::Noop;
    if(false){
    }else if(user_data_s->overlap == ContourOverlapMethod::ignore){
        ebv_opts.contouroverlap  = Mutate_Voxels_Opts::ContourOverlap::Ignore;
    }else if(user_data_s->overlap == ContourOverlapMethod::opposite_orientations_cancel){
        ebv_opts.contouroverlap  = Mutate_Voxels_Opts::ContourOverlap::HonourOppositeOrientations;
    }else if(user_data_s->overlap == ContourOverlapMethod::overlapping_contours_cancel){
        ebv_opts.contouroverlap  = Mutate_Voxels_Opts::ContourOverlap::ImplicitOrientations;
    }else{
        throw std::invalid_argument("Invalid ContourOverlap option provided.");
    }
    if(false){
    }else if(user_data_s->inclusivity == HighlightInclusionMethod::centre){
        ebv_opts.inclusivity    = Mutate_Voxels_Opts::Inclusivity::Centre;
    }else if(user_data_s->inclusivity == HighlightInclusionMethod::planar_corners_inclusive){
        ebv_opts.inclusivity    = Mutate_Voxels_Opts::Inclusivity::Inclusive;
    }else if(user_data_s->inclusivity == HighlightInclusionMethod::planar_corners_exclusive){
        ebv_opts.inclusivity    = Mutate_Voxels_Opts::Inclusivity::Exclusive;
    }else{
        throw std::invalid_argument("Invalid Inclusivity option provided.");
    }


    auto f_bounded = std::function<void(long int, long int, long int, float &)>();
    if(user_data_s->overwrite_interior){
        f_bounded = [&](long int /*row*/, long int /*col*/, long int /*channel*/, float &voxel_val) {
                voxel_val = user_data_s->outgoing_interior_val;
        };
    }

    auto f_unbounded = std::function<void(long int, long int, long int, float &)>();
    if(user_data_s->overwrite_exterior){
        f_unbounded = [&](long int /*row*/, long int /*col*/, long int /*channel*/, float &voxel_val) {
            voxel_val = user_data_s->outgoing_exterior_val;
        };
    }

    std::list<std::reference_wrapper<planar_image<float,double>>> selected_imgs;
    for(auto &img_it : selected_img_its) selected_imgs.push_back( std::ref(*img_it) );

    Mutate_Voxels<float,double>( std::ref(*first_img_it),
                                 selected_imgs, 
                                 ccsl, 
                                 ebv_opts, 
                                 f_bounded,
                                 f_unbounded );


    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    UpdateImageDescription( std::ref(*first_img_it), "Highlighted ROIs" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it) );

    return true;
}





