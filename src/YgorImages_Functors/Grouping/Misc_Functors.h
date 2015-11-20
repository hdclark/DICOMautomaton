

#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorString.h"

//--------------------------------------------------------------------------------------------------------------
//------------------------------------------- Image Purging Functors -------------------------------------------
//--------------------------------------------------------------------------------------------------------------
bool PurgeAboveTemporalThreshold(const planar_image<float,double> &animg, double tmax);

bool PurgeNone(const planar_image<float,double> &animg);


//--------------------------------------------------------------------------------------------------------------
//------------------------------------------- Image Grouping Functors ------------------------------------------
//--------------------------------------------------------------------------------------------------------------
std::list<planar_image_collection<float,double>::images_list_it_t>
GroupSpatiallyOverlappingImages(planar_image_collection<float,double>::images_list_it_t first_img_it,
                                std::reference_wrapper<planar_image_collection<float,double>> pic);

std::list<planar_image_collection<float,double>::images_list_it_t>
GroupTemporallyOverlappingImages(planar_image_collection<float,double>::images_list_it_t first_img_it,
                                std::reference_wrapper<planar_image_collection<float,double>> pic);

std::list<planar_image_collection<float,double>::images_list_it_t>
GroupSpatiallyTemporallyOverlappingImages(planar_image_collection<float,double>::images_list_it_t first_img_it,
                                          std::reference_wrapper<planar_image_collection<float,double>> pic);

std::list<planar_image_collection<float,double>::images_list_it_t>
GroupIndividualImages(planar_image_collection<float,double>::images_list_it_t first_img_it,
                      std::reference_wrapper<planar_image_collection<float,double>>);

std::list<planar_image_collection<float,double>::images_list_it_t>
GroupAllImages(planar_image_collection<float,double>::images_list_it_t first_img_it,
               std::reference_wrapper<planar_image_collection<float,double>>);

