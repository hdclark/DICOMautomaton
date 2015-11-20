

#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <utility>

#include <experimental/any>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"


bool ReasonableHUWindow(planar_image_collection<float,double>::images_list_it_t first_img_it,
                        std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                        std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                        std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                        float FullWidth, float Centre,
                        std::experimental::any userdata );


//Specilizations with some reasonable default values.

bool StandardGenericHUWindow(planar_image_collection<float,double>::images_list_it_t first_img_it,
                             std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                             std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                             std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                             std::experimental::any userdata );

bool StandardHeadAndNeckHUWindow(planar_image_collection<float,double>::images_list_it_t first_img_it,
                                 std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                                 std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                                 std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                                 std::experimental::any userdata );

bool StandardAbdominalHUWindow(planar_image_collection<float,double>::images_list_it_t first_img_it,
                               std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                               std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                               std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                               std::experimental::any userdata );

bool StandardThoraxHUWindow(planar_image_collection<float,double>::images_list_it_t first_img_it,
                            std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                            std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                            std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                            std::experimental::any userdata );

bool StandardBoneHUWindow(planar_image_collection<float,double>::images_list_it_t first_img_it,
                          std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                          std::experimental::any userdata );
