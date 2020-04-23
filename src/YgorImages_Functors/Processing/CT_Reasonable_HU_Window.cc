
#include <algorithm>
#include <any>
#include <functional>
#include <list>
#include <map>
#include <string>

#include "YgorImages.h"

template <class T> class contour_collection;



bool ReasonableHUWindow(planar_image_collection<float,double>::images_list_it_t first_img_it,
                        std::list<planar_image_collection<float,double>::images_list_it_t> ,
                        std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                        std::list<std::reference_wrapper<contour_collection<double>>>, 
                        float FullWidth, float Centre,
                        std::any ){

    //This routine generates a window which covers normal tissue Hounsfield unit range.
    // The image itself is not altered. Nor are the image values taken into account.
    //
    // Hounsfield units are defined such that:
    //   -1000 HU --> air
    //       0 HU --> water
    //   +4000 HU --> metals
    //
    // So, because we have 8bit or 16bit displays, we have to group bunchs of HU together
    // into a single grayscale level. To resolve different tissues, we need to modify the
    // window and centre to suit. Typical settings are:
    //
    //                               Full Width          Centre
    //   For abdominal scans            350                50
    //   For thorax scans              1500              -500
    //   For bone scans                2000               250
    //
    // A reasonable default that encompasses a reasonable range of tissues, is 
    // 1000 FW and 500 C. This default will not to be particularly good for any specific thing.

    //first_img_it->metadata["Description"] = "Window forced to normal tissue HU range"; //Needed?

    first_img_it->metadata["WindowValidFor"] = first_img_it->metadata["Description"];
    first_img_it->metadata["WindowCenter"]   = std::to_string(Centre);
    first_img_it->metadata["WindowWidth"]    = std::to_string(FullWidth);

    return true;
}



bool StandardGenericHUWindow(planar_image_collection<float,double>::images_list_it_t first_img_it,
                             std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                             std::list<std::reference_wrapper<planar_image_collection<float,double>>> ext_imgs,
                             std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                             std::any userdata ){
    return ReasonableHUWindow( std::move(first_img_it),
                               std::move(selected_img_its),
                               std::move(ext_imgs),
                               std::move(ccsl),
                               static_cast<float>(1000), static_cast<float>(500),
                               std::move(userdata) );
}

bool StandardHeadAndNeckHUWindow(planar_image_collection<float,double>::images_list_it_t first_img_it,
                             std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                             std::list<std::reference_wrapper<planar_image_collection<float,double>>> ext_imgs,
                             std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                             std::any userdata ){
    return ReasonableHUWindow( std::move(first_img_it),
                               std::move(selected_img_its),
                               std::move(ext_imgs),
                               std::move(ccsl),
                               static_cast<float>(255), static_cast<float>(25),
                               std::move(userdata) );
}

bool StandardAbdominalHUWindow(planar_image_collection<float,double>::images_list_it_t first_img_it,
                               std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                               std::list<std::reference_wrapper<planar_image_collection<float,double>>> ext_imgs,
                               std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                               std::any userdata ){
    return ReasonableHUWindow( std::move(first_img_it),
                               std::move(selected_img_its),
                               std::move(ext_imgs),
                               std::move(ccsl),
                               static_cast<float>(350), static_cast<float>(50),
                               std::move(userdata) );
}

bool StandardThoraxHUWindow(planar_image_collection<float,double>::images_list_it_t first_img_it,
                            std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                            std::list<std::reference_wrapper<planar_image_collection<float,double>>> ext_imgs,
                            std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                            std::any userdata ){
    return ReasonableHUWindow( std::move(first_img_it),
                               std::move(selected_img_its),
                               std::move(ext_imgs),
                               std::move(ccsl),
                               static_cast<float>(1500), static_cast<float>(-500),
                               std::move(userdata) );
}

bool StandardBoneHUWindow(planar_image_collection<float,double>::images_list_it_t first_img_it,
                          std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>> ext_imgs,
                          std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                          std::any userdata ){
    return ReasonableHUWindow( std::move(first_img_it),
                               std::move(selected_img_its),
                               std::move(ext_imgs),
                               std::move(ccsl),
                               static_cast<float>(2000), static_cast<float>(250),
                               std::move(userdata) );
}

bool StandardAlphaBetaWindow(planar_image_collection<float,double>::images_list_it_t first_img_it,
                          std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>> ext_imgs,
                          std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                          std::any userdata ){
    // This generic estimate covers the majority of alpha/beta ratios reported by van Leeuwen et al., 2018
    // (doi:10.1186/s13014-018-1040-z) for a variety of tissue types. Note, however, that the complete range appears to
    // span approximately -15 to 30. However, this would provide very little contrast for the majority of tissues
    // (0 to 5).
    return ReasonableHUWindow( std::move(first_img_it),
                               std::move(selected_img_its),
                               std::move(ext_imgs),
                               std::move(ccsl),
                               2.5f, 2.5f,  // Units: 1/Gy.
                               std::move(userdata) );
}

