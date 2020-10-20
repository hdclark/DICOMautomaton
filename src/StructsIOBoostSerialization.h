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


//Class: Contour_Data.
template<typename Archive>
void serialize(Archive &a, Contour_Data &c, const unsigned int /*version*/){
    a & boost::serialization::make_nvp("ccs",c.ccs);
    return;
}


//Class: Image_Array.
template<typename Archive>
void serialize(Archive &a, Image_Array &i, const unsigned int version){
    if(version == 0){
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
    if(version == 0){
        a & boost::serialization::make_nvp("pset",p.pset);
    }else{
        FUNCWARN("Point_Cloud archives with version " << version << " are not recognized");
    }
    return;
}

//Class: Surface_Mesh.
template<typename Archive>
void serialize(Archive &a, Surface_Mesh &p, const unsigned int version){
    if(version == 0){
        // Note: No dynamic surface_mesh attributes are saved in version 0 due to use of std::any.
        //       Until a suitable reflection mechanism is located, we are stuck ignoring members
        //       that make use of std::any.
        a & boost::serialization::make_nvp("meshes",p.meshes);
    }else{
        FUNCWARN("Surface_Mesh archives with version " << version << " are not recognized");
    }
    return;
}

//Class: Static_Machine_State.
template<typename Archive>
void serialize(Archive &a, Static_Machine_State &p, const unsigned int version){
    if(version == 0){
        a & boost::serialization::make_nvp("CumulativeMetersetWeight",p.CumulativeMetersetWeight)
          & boost::serialization::make_nvp("ControlPointIndex",p.ControlPointIndex)

          & boost::serialization::make_nvp("GantryAngle",p.GantryAngle)
          & boost::serialization::make_nvp("GantryRotationDirection",p.GantryRotationDirection)

          & boost::serialization::make_nvp("BeamLimitingDeviceAngle",p.BeamLimitingDeviceAngle)
          & boost::serialization::make_nvp("BeamLimitingDeviceRotationDirection",p.BeamLimitingDeviceRotationDirection)

          & boost::serialization::make_nvp("PatientSupportAngle",p.PatientSupportAngle)
          & boost::serialization::make_nvp("PatientSupportRotationDirection",p.PatientSupportRotationDirection)

          & boost::serialization::make_nvp("TableTopEccentricAngle",p.TableTopEccentricAngle)
          & boost::serialization::make_nvp("TableTopEccentricRotationDirection",p.TableTopEccentricRotationDirection)

          & boost::serialization::make_nvp("TableTopVerticalPosition",p.TableTopVerticalPosition)
          & boost::serialization::make_nvp("TableTopLongitudinalPosition",p.TableTopLongitudinalPosition)
          & boost::serialization::make_nvp("TableTopLateralPosition",p.TableTopLateralPosition)

          & boost::serialization::make_nvp("TableTopPitchAngle",p.TableTopPitchAngle)
          & boost::serialization::make_nvp("TableTopPitchRotationDirection",p.TableTopPitchRotationDirection)

          & boost::serialization::make_nvp("TableTopRollAngle",p.TableTopRollAngle)
          & boost::serialization::make_nvp("TableTopRollRotationDirection",p.TableTopRollRotationDirection)

          & boost::serialization::make_nvp("IsocentrePosition",p.IsocentrePosition)

          & boost::serialization::make_nvp("JawPositionsX",p.JawPositionsX)
          & boost::serialization::make_nvp("JawPositionsY",p.JawPositionsY)
          & boost::serialization::make_nvp("MLCPositionsX",p.MLCPositionsX)

          & boost::serialization::make_nvp("metadata",p.metadata);
    }else{
        FUNCWARN("Static_Machine_State archives with version " << version << " are not recognized");
    }
    return;
}

//Class: Dynamic_Machine_State.
template<typename Archive>
void serialize(Archive &a, Dynamic_Machine_State &p, const unsigned int version){
    if(version == 0){
        a & boost::serialization::make_nvp("BeamNumber",p.BeamNumber)
          & boost::serialization::make_nvp("FinalCumulativeMetersetWeight",p.FinalCumulativeMetersetWeight)

          & boost::serialization::make_nvp("static_states",p.static_states)

          & boost::serialization::make_nvp("metadata",p.metadata);
    }else{
        FUNCWARN("Dynamic_Machine_State archives with version " << version << " are not recognized");
    }
    return;
}

//Class: TPlan_Config.
template<typename Archive>
void serialize(Archive &a, TPlan_Config &p, const unsigned int version){
    if(version == 0){
        a & boost::serialization::make_nvp("dynamic_states",p.dynamic_states)

          & boost::serialization::make_nvp("metadata",p.metadata);
    }else{
        FUNCWARN("TPlan_Config archives with version " << version << " are not recognized");
    }
    return;
}

//Class: Line_Sample.
template<typename Archive>
void serialize(Archive &a, Line_Sample &l, const unsigned int version){
    if(version == 0){
        a & boost::serialization::make_nvp("line",l.line);
    }else{
        FUNCWARN("Line_Sample archives with version " << version << " are not recognized");
    }
    return;
}

//Class: Drover.
template<typename Archive>
void serialize(Archive &a, Drover &d, const unsigned int version){
    if(version == 0){
        FUNCERR("Archives with version 0 are no longer supported. Cannot continue");
    }else if(version == 1){
        a & boost::serialization::make_nvp("contour_data",d.contour_data)
          & boost::serialization::make_nvp("image_data",d.image_data)
          & boost::serialization::make_nvp("point_data",d.point_data);
    }else if(version == 2){
        a & boost::serialization::make_nvp("contour_data",d.contour_data)
          & boost::serialization::make_nvp("image_data",d.image_data)
          & boost::serialization::make_nvp("point_data",d.point_data)
          & boost::serialization::make_nvp("smesh_data",d.smesh_data);
    }else if(version == 3){
        a & boost::serialization::make_nvp("contour_data",d.contour_data)
          & boost::serialization::make_nvp("image_data",d.image_data)
          & boost::serialization::make_nvp("point_data",d.point_data)
          & boost::serialization::make_nvp("smesh_data",d.smesh_data)
          & boost::serialization::make_nvp("tplan_data",d.tplan_data)
          & boost::serialization::make_nvp("lsamp_data",d.lsamp_data);
    }else{
        FUNCWARN("Drover archives with version " << version << " are not recognized");
    }
    return;
}

} // namespace serialization
} // namespace boost


//BOOST_CLASS_VERSION(Image_Array, 0); // Initial version number.
BOOST_CLASS_VERSION(Image_Array, 1) // After removing the disused 'bits' and 'filename' members.

BOOST_CLASS_VERSION(Point_Cloud, 0) // Initial version number.

BOOST_CLASS_VERSION(Surface_Mesh, 0) // Initial version number, effectively just a fv_surface_mesh wrapper class.

BOOST_CLASS_VERSION(Static_Machine_State, 0) // Initial version number.
BOOST_CLASS_VERSION(Dynamic_Machine_State, 0) // Initial version number.
BOOST_CLASS_VERSION(TPlan_Config, 0) // Initial version number.

BOOST_CLASS_VERSION(Line_Sample, 0) // Initial version number.

//BOOST_CLASS_VERSION(Drover, 0); // Initial version number.
//BOOST_CLASS_VERSION(Drover, 1); // After removing Dose_Arrays and Drover::Has_Been_Melded.
//BOOST_CLASS_VERSION(Drover, 2); // After adding v0 of the Surface_Mesh member.
BOOST_CLASS_VERSION(Drover, 3) // After adding v0 of the TPlan_Config member and v0 of the Line_Sample member.

