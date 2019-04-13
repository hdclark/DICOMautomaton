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
#include <boost/serialization/version.hpp>

#include "Structs.h"

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


//Class: Image_Array.
template<typename Archive>
void serialize(Archive &a, Image_Array &i, const unsigned int version){
    if(false){
    }else if(version == 0){
        std::string dummy_filename;
        unsigned int dummy_bits;
        a & boost::serialization::make_nvp("imagecoll",i.imagecoll)
          & boost::serialization::make_nvp("filename",dummy_filename)
          & boost::serialization::make_nvp("bits",dummy_bits);
    }else if(version == 1){
        a & boost::serialization::make_nvp("imagecoll",i.imagecoll);
    }else{
        FUNCWARN("Image_Array archives with version " << version << " are not recognized");
    }
    return;
}

//Class: Point_Cloud.
template<typename Archive>
void serialize(Archive &a, Point_Cloud &p, const unsigned int version){
    if(false){
    }else if(version == 0){
        // Note: No dynamic point_cloud attributes are saved in version 0 due to use of std::any.
        //       Until a suitable reflection mechanism is located, we are stuck ignoring members
        //       that make use of std::any.
        a & boost::serialization::make_nvp("points",p.points)
          & boost::serialization::make_nvp("metadata",p.metadata);
    }else{
        FUNCWARN("Point_Cloud archives with version " << version << " are not recognized");
    }
    return;
}

//Class: Drover.
template<typename Archive>
void serialize(Archive &a, Drover &d, const unsigned int version){
    if(false){
    }else if(version == 0){
        FUNCERR("Archives with version 0 are no longer supported. Cannot continue");
    }else if(version == 1){
        a & boost::serialization::make_nvp("contour_data",d.contour_data)
          & boost::serialization::make_nvp("image_data",d.image_data)
          & boost::serialization::make_nvp("point_data",d.point_data);
    }else{
        FUNCWARN("Drover archives with version " << version << " are not recognized");
    }
    return;
}

} // namespace serialization
} // namespace boost


//BOOST_CLASS_VERSION(Image_Array, 0); // Initial version number.
BOOST_CLASS_VERSION(Image_Array, 1); // After removing the disused 'bits' and 'filename' members.

BOOST_CLASS_VERSION(Point_Cloud, 0); // Initial version number.

//BOOST_CLASS_VERSION(Drover, 0); // Initial version number.
BOOST_CLASS_VERSION(Drover, 1); // After removing Dose_Arrays and Drover::Has_Been_Melded.

