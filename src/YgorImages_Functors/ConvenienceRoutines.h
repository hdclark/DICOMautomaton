#pragma once

#include <string>

#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorImages.h"


void UpdateImageDescription(
        std::reference_wrapper<planar_image<float,double>> img_refw,
        const std::string &Description);

void UpdateImageDescription(
        planar_image_collection<float,double>::images_list_it_t img_it,
        const std::string &Description);



void UpdateImageWindowCentreWidth(
        std::reference_wrapper<planar_image<float,double>> img_refw,
        const Stats::Running_MinMax<float> &RMM );

void UpdateImageWindowCentreWidth(
        planar_image_collection<float,double>::images_list_it_t img_it,
        const Stats::Running_MinMax<float> &RMM );

