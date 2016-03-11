//StructsIOBoostSerialization.h - Written by hal clark in 2016.
//
// This file defines routines for using Struct.h classes with Boost.Serialization.
//
// This file is optional. If you choose to use it, ensure you link with libboost_serialization.
//
// The objects in this file have been 'wrapped' into a boost.serialization name-value pair so that they can be
// serialized to archives requiring entities to be named (i.e., XML) in addition to the 'easier' formats (i.e.,
// plain text, binary).
//

#pragma once

#include <boost/serialization/nvp.hpp> 

#include <boost/serialization/string.hpp>
#include <boost/serialization/list.hpp>
//#include <boost/serialization/vector.hpp>
//#include <boost/serialization/map.hpp>
#include <boost/serialization/shared_ptr.hpp>

#include "YgorMath.h"
#include "YgorImages.h"

#include "YgorMathIOBoostSerialization.h"
#include "YgorImagesIOBoostSerialization.h"

namespace boost {
namespace serialization {


//Class: contours_with_meta.
template<typename Archive>
void serialize(Archive &a, contours_with_meta &c, const unsigned int /*version*/){
    a & boost::serialization::make_nvp("base_cc", boost::serialization::base_object<contour_collection<double>>(c))
      & boost::serialization::make_nvp("roi_number",c.ROI_number)
      & boost::serialization::make_nvp("minimum_separation",c.Minimum_Separation)
      & boost::serialization::make_nvp("raw_roi_name",c.Raw_ROI_name)
      & boost::serialization::make_nvp("segmentation_history",c.Segmentation_History);
    return;
}

//Class: Contour_Data.
template<typename Archive>
void serialize(Archive &a, Contour_Data &c, const unsigned int /*version*/){
    a & boost::serialization::make_nvp("ccs",c.ccs);
    return;
}

//Class: Dose_Array.
template<typename Archive>
void serialize(Archive &a, Dose_Array &d, const unsigned int /*version*/){
    a & boost::serialization::make_nvp("imagecoll",d.imagecoll)
      & boost::serialization::make_nvp("filename",d.filename)
      & boost::serialization::make_nvp("bits",d.bits)
      & boost::serialization::make_nvp("grid_scale",d.grid_scale);
    return;
}

//Class: Image_Array.
template<typename Archive>
void serialize(Archive &a, Image_Array &i, const unsigned int /*version*/){
    a & boost::serialization::make_nvp("imagecoll",i.imagecoll)
      & boost::serialization::make_nvp("filename",i.filename)
      & boost::serialization::make_nvp("bits",i.bits);
    return;
}

//Class: Drover.
template<typename Archive>
void serialize(Archive &a, Drover &d, const unsigned int /*version*/){
    a & boost::serialization::make_nvp("has_been_melded",d.Has_Been_Melded)
      & boost::serialization::make_nvp("contour_data",d.contour_data)
      & boost::serialization::make_nvp("dose_data",d.dose_data)
      & boost::serialization::make_nvp("image_data",d.image_data);
    return;
}



} // namespace serialization
} // namespace boost
