
#include <experimental/optional>
#include <functional>
#include <list>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"

//--------------------------------------------------------------------------------------------------------------
//------------------------------------------- Image Purging Functors -------------------------------------------
//--------------------------------------------------------------------------------------------------------------
bool PurgeAboveTemporalThreshold(const planar_image<float,double> &animg, double tmax){
    //Remove images which occur after some point in time. Returns true for all images which will be purged.
    auto dt = animg.GetMetadataValueAs<double>("dt");
    return dt && (dt.value() > tmax);
}

bool PurgeNone(const planar_image<float,double> &){
    //A 'null' purge functor. Removes nothing.
    return false;
}



//--------------------------------------------------------------------------------------------------------------
//------------------------------------------- Image Grouping Functors ------------------------------------------
//--------------------------------------------------------------------------------------------------------------
std::list<planar_image_collection<float,double>::images_list_it_t> 
GroupSpatiallyOverlappingImages(planar_image_collection<float,double>::images_list_it_t first_img_it,
                                std::reference_wrapper<planar_image_collection<float,double>> pic){
    //Select all images with substantial spatial overlap.
    const auto img_cntr = first_img_it->center();
    const auto ortho = first_img_it->row_unit.Cross( first_img_it->col_unit ).unit();
    const std::list<vec3<double>> points = { img_cntr, img_cntr + ortho * first_img_it->pxl_dz * 0.25,
                                                       img_cntr - ortho * first_img_it->pxl_dz * 0.25 };
    return pic.get().get_images_which_encompass_all_points(points);
}

std::list<planar_image_collection<float,double>::images_list_it_t>
GroupTemporallyOverlappingImages(planar_image_collection<float,double>::images_list_it_t first_img_it,
                                std::reference_wrapper<planar_image_collection<float,double>> pic){

    //NOTE: The units of time here are unknown and not standard. If possible, it would be best to check other
    //      metadata or have a more definite (standardized) interpretation.
    auto L_time_opt = first_img_it->GetMetadataValueAs<double>("dt");
    if(!L_time_opt) FUNCERR("Missing metadata info needed for temporal grouping (on L). Cannot continue");
    const auto L_time = L_time_opt.value();
  
    auto overlapping_imgs = pic.get().get_images_satisfying([=](const planar_image<float,double> &animg) -> bool {
        auto R_time_opt = animg.GetMetadataValueAs<double>("dt");
        if(!R_time_opt) FUNCERR("Missing metadata info needed for spatial-temporal grouping (on R). Cannot continue");
        const auto R_time = R_time_opt.value();
        return (RELATIVE_DIFF(L_time,R_time) < 1E-3);
    });
    return overlapping_imgs;
}

std::list<planar_image_collection<float,double>::images_list_it_t>
GroupSpatiallyTemporallyOverlappingImages(planar_image_collection<float,double>::images_list_it_t first_img_it,
                                          std::reference_wrapper<planar_image_collection<float,double>> pic){
    //Select all images with substantial spatial overlap.
    const auto img_cntr = first_img_it->center();
    const auto ortho = first_img_it->row_unit.Cross( first_img_it->col_unit ).unit();
    const std::list<vec3<double>> points = { img_cntr, img_cntr + ortho * first_img_it->pxl_dz * 0.25,
                                                       img_cntr - ortho * first_img_it->pxl_dz * 0.25 };
    auto candidate_images = pic.get().get_images_which_encompass_all_points(points);
   
    //Now filter out those that do not have the same time.
    decltype(candidate_images) out;

    //NOTE: The units of time here are unknown and not standard. If possible, it would be best to check other
    //      metadata or have a more definite (standardized) interpretation.
    auto L_time = first_img_it->GetMetadataValueAs<double>("dt");
    if(!L_time) FUNCERR("Missing metadata info needed for spatial-temporal grouping (on L). Cannot continue");

    for(auto &an_img_it : candidate_images){
        auto R_time = an_img_it->GetMetadataValueAs<double>("dt");
        if(!R_time) FUNCERR("Missing metadata info needed for spatial-temporal grouping (on R). Cannot continue");
        if(RELATIVE_DIFF(L_time.value(), R_time.value()) < 1E-3) out.push_back(an_img_it);
    }

    return out;
}

std::list<planar_image_collection<float,double>::images_list_it_t>
GroupIndividualImages(planar_image_collection<float,double>::images_list_it_t first_img_it,
                      std::reference_wrapper<planar_image_collection<float,double>> ){
    //Process each image separately (i.e., each group consists of a single image).
    return std::list<decltype(first_img_it)>({ first_img_it });
}


std::list<planar_image_collection<float,double>::images_list_it_t>
GroupAllImages(planar_image_collection<float,double>::images_list_it_t,
               std::reference_wrapper<planar_image_collection<float,double>> pic){
    return pic.get().get_all_images();
}

