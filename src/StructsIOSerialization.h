//StructsIOSerialization.h - Written by hal clark in 2026.
//
// This file defines routines for serializing Struct.h classes with Ygor serialization.

#pragma once

#include <memory>
#include <string>

#include "Structs.h"

#include "YgorIOXMLSerialization.h"
#include "YgorMathIOSerialization.h"
#include "YgorImagesIOSerialization.h"

namespace ygor {
namespace serialization {

#ifndef DCMA_YGOR_SHARED_PTR_SERIALIZATION
#define DCMA_YGOR_SHARED_PTR_SERIALIZATION
template<typename T>
void serialize(xml_oarchive &a, std::shared_ptr<T> &p){
    auto has_value = static_cast<bool>(p);
    a & make_nvp("has_value", has_value);
    if(has_value){
        a & make_nvp("value", *p);
    }
    return;
}

template<typename T>
void serialize(xml_iarchive &a, std::shared_ptr<T> &p){
    bool has_value = false;
    a & make_nvp("has_value", has_value);
    if(has_value){
        p = std::make_shared<T>();
        a & make_nvp("value", *p);
    }else{
        p.reset();
    }
    return;
}
#endif // DCMA_YGOR_SHARED_PTR_SERIALIZATION

//Class: Contour_Data.
template<typename Archive>
void serialize(Archive &a, Contour_Data &c){
    a & make_nvp("ccs", c.ccs);
    return;
}

//Class: Image_Array.
template<typename Archive>
void serialize(Archive &a, Image_Array &i){
    int64_t schema_version = 1;
    a & make_nvp("schema_version", schema_version);
    if(schema_version == 0){
        std::string placeholder_filename;
        unsigned int placeholder_bits = 0UL;
        a & make_nvp("imagecoll", i.imagecoll)
          & make_nvp("filename", placeholder_filename)
          & make_nvp("bits", placeholder_bits);
    }else if(schema_version == 1){
        a & make_nvp("imagecoll", i.imagecoll);
    }else{
        YLOGWARN("Image_Array archives with schema version " << schema_version << " are not recognized");
    }
    return;
}

//Class: Point_Cloud.
template<typename Archive>
void serialize(Archive &a, Point_Cloud &p){
    int64_t schema_version = 0;
    a & make_nvp("schema_version", schema_version);
    if(schema_version == 0){
        a & make_nvp("pset", p.pset);
    }else{
        YLOGWARN("Point_Cloud archives with schema version " << schema_version << " are not recognized");
    }
    return;
}

//Class: Surface_Mesh.
template<typename Archive>
void serialize(Archive &a, Surface_Mesh &p){
    int64_t schema_version = 0;
    a & make_nvp("schema_version", schema_version);
    if(schema_version == 0){
        // Note: No dynamic surface_mesh attributes are saved due to use of std::any.
        a & make_nvp("meshes", p.meshes);
    }else{
        YLOGWARN("Surface_Mesh archives with schema version " << schema_version << " are not recognized");
    }
    return;
}

//Class: Static_Machine_State.
template<typename Archive>
void serialize(Archive &a, Static_Machine_State &p){
    int64_t schema_version = 0;
    a & make_nvp("schema_version", schema_version);
    if(schema_version == 0){
        a & make_nvp("CumulativeMetersetWeight", p.CumulativeMetersetWeight)
          & make_nvp("ControlPointIndex", p.ControlPointIndex)

          & make_nvp("GantryAngle", p.GantryAngle)
          & make_nvp("GantryRotationDirection", p.GantryRotationDirection)

          & make_nvp("BeamLimitingDeviceAngle", p.BeamLimitingDeviceAngle)
          & make_nvp("BeamLimitingDeviceRotationDirection", p.BeamLimitingDeviceRotationDirection)

          & make_nvp("PatientSupportAngle", p.PatientSupportAngle)
          & make_nvp("PatientSupportRotationDirection", p.PatientSupportRotationDirection)

          & make_nvp("TableTopEccentricAngle", p.TableTopEccentricAngle)
          & make_nvp("TableTopEccentricRotationDirection", p.TableTopEccentricRotationDirection)

          & make_nvp("TableTopVerticalPosition", p.TableTopVerticalPosition)
          & make_nvp("TableTopLongitudinalPosition", p.TableTopLongitudinalPosition)
          & make_nvp("TableTopLateralPosition", p.TableTopLateralPosition)

          & make_nvp("TableTopPitchAngle", p.TableTopPitchAngle)
          & make_nvp("TableTopPitchRotationDirection", p.TableTopPitchRotationDirection)

          & make_nvp("TableTopRollAngle", p.TableTopRollAngle)
          & make_nvp("TableTopRollRotationDirection", p.TableTopRollRotationDirection)

          & make_nvp("IsocentrePosition", p.IsocentrePosition)

          & make_nvp("JawPositionsX", p.JawPositionsX)
          & make_nvp("JawPositionsY", p.JawPositionsY)
          & make_nvp("MLCPositionsX", p.MLCPositionsX)

          & make_nvp("metadata", p.metadata);
    }else{
        YLOGWARN("Static_Machine_State archives with schema version " << schema_version << " are not recognized");
    }
    return;
}

//Class: Dynamic_Machine_State.
template<typename Archive>
void serialize(Archive &a, Dynamic_Machine_State &p){
    int64_t schema_version = 0;
    a & make_nvp("schema_version", schema_version);
    if(schema_version == 0){
        a & make_nvp("BeamNumber", p.BeamNumber)
          & make_nvp("FinalCumulativeMetersetWeight", p.FinalCumulativeMetersetWeight)

          & make_nvp("static_states", p.static_states)

          & make_nvp("metadata", p.metadata);
    }else{
        YLOGWARN("Dynamic_Machine_State archives with schema version " << schema_version << " are not recognized");
    }
    return;
}

//Class: RTPlan.
template<typename Archive>
void serialize(Archive &a, RTPlan &p){
    int64_t schema_version = 0;
    a & make_nvp("schema_version", schema_version);
    if(schema_version == 0){
        a & make_nvp("dynamic_states", p.dynamic_states)

          & make_nvp("metadata", p.metadata);
    }else{
        YLOGWARN("RTPlan archives with schema version " << schema_version << " are not recognized");
    }
    return;
}

//Class: Line_Sample.
template<typename Archive>
void serialize(Archive &a, Line_Sample &l){
    int64_t schema_version = 0;
    a & make_nvp("schema_version", schema_version);
    if(schema_version == 0){
        a & make_nvp("line", l.line);
    }else{
        YLOGWARN("Line_Sample archives with schema version " << schema_version << " are not recognized");
    }
    return;
}

//Class: Drover.
template<typename Archive>
void serialize(Archive &a, Drover &d){
    int64_t schema_version = 3;
    a & make_nvp("schema_version", schema_version);
    if(schema_version == 0){
        YLOGERR("Archives with schema version 0 are no longer supported. Cannot continue");
    }else if(schema_version == 1){
        a & make_nvp("contour_data", d.contour_data)
          & make_nvp("image_data", d.image_data)
          & make_nvp("point_data", d.point_data);
    }else if(schema_version == 2){
        a & make_nvp("contour_data", d.contour_data)
          & make_nvp("image_data", d.image_data)
          & make_nvp("point_data", d.point_data)
          & make_nvp("smesh_data", d.smesh_data);
    }else if(schema_version == 3){
        a & make_nvp("contour_data", d.contour_data)
          & make_nvp("image_data", d.image_data)
          & make_nvp("point_data", d.point_data)
          & make_nvp("smesh_data", d.smesh_data)
          & make_nvp("rtplan_data", d.rtplan_data)
          & make_nvp("lsamp_data", d.lsamp_data);
    }else{
        YLOGWARN("Drover archives with schema version " << schema_version << " are not recognized");
    }
    return;
}

} // namespace serialization
} // namespace ygor
