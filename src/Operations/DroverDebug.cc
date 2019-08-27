//DroverDebug.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <cstdlib>            //Needed for exit() calls.
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DroverDebug.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.



OperationDoc OpArgDocDroverDebug(void){
    OperationDoc out;
    out.name = "DroverDebug";

    out.desc = 
        "This operation reports basic information on the state of the main Drover class."
        " It can be used to report on the state of the data, which can be useful for debugging.";

    return out;
}



Drover DroverDebug(Drover DICOM_data, 
                   OperationArgPkg /*OptArgs*/, 
                   std::map<std::string,std::string> /*InvocationMetadata*/, 
                   std::string /*FilenameLex*/ ){

    //Image data.
    {
        FUNCINFO("There are " << DICOM_data.image_data.size() << " Image_Arrays loaded");

        size_t i_arr = 0;
        for(auto &iap : DICOM_data.image_data){
            if(iap == nullptr){
                FUNCINFO("  Image_Array " << i_arr << " is not valid");

            }else{
                FUNCINFO("  Image_Array " << i_arr << " has " <<
                         iap->imagecoll.images.size() << " image slices");

                size_t i_num = 0;
                for(auto &img : iap->imagecoll.images){
                    const auto ModalityOpt = img.GetMetadataValueAs<std::string>("Modality");
                    const auto mm = img.minmax();

                    FUNCINFO("    Image " << i_num << " has" <<
                             " Modality = " << ModalityOpt.value_or("(unspecified)") );
                    FUNCINFO("    Image " << i_num << " has" <<
                             " pixel value range = [" << mm.first << "," << mm.second << "]");
                    FUNCINFO("    Image " << i_num << " has" <<
                             " has pxl_dx, pxl_dy, pxl_dz = " <<
                                 img.pxl_dx << ", " <<
                                 img.pxl_dy << ", " <<
                                 img.pxl_dz);
                    FUNCINFO("    Image " << i_num << " has" <<
                             " anchor, offset = " <<
                                 img.anchor << ", " <<
                                 img.offset);
                    FUNCINFO("    Image " << i_num << " has" <<
                             " row_unit, col_unit = " <<
                                 img.row_unit << ", " <<
                                 img.col_unit);
                    ++i_num;
                }
                ++i_arr;
            }
        }
    }

    //Contour data.
    do{
        if(DICOM_data.contour_data == nullptr){
            FUNCINFO("There are 0 contour_collections loaded");
            break;
        }

        FUNCINFO("There are " <<
                 DICOM_data.contour_data->ccs.size() <<
                 " contour_collections loaded");

        size_t c_dat = 0;
        for(auto & cc : DICOM_data.contour_data->ccs){
            FUNCINFO("  contour_collection " <<
                     c_dat++ <<
                     " has " <<
                     cc.contours.size() <<
                     " contours");
            size_t c_num = 0;
            for(auto & c : cc.contours){
                FUNCINFO("    contour " <<
                         c_num++ <<
                         " has " <<
                         c.points.size() <<
                         " vertices");
                if(!c.points.empty()){
                    FUNCINFO("      contour " <<
                         (c_num-1) <<
                         " has average point " <<
                         c.Average_Point());
                }
            }
        }
    }while(false);

    //Point data.
    {
        FUNCINFO("There are " << DICOM_data.point_data.size() << " Point_Clouds loaded");

        size_t p_cnt = 0;
        for(auto &pc : DICOM_data.point_data){
            if(pc == nullptr){
                FUNCINFO("  Point_Cloud " << p_cnt << " is not valid");

            }else{
                FUNCINFO("  Point_Cloud " << p_cnt << " has " <<
                         pc->points.size() << " points and " <<
                         pc->attributes.size() << " attributes");
            }
            ++p_cnt;
        }
    }

    //Surface mesh data.
    {
        FUNCINFO("There are " << DICOM_data.smesh_data.size() << " Surface_Meshes loaded");

        size_t m_cnt = 0;
        for(auto &sm : DICOM_data.smesh_data){
            if(sm == nullptr){
                FUNCINFO("  Surface_Mesh " << m_cnt << " is not valid");

            }else{
                FUNCINFO("  Surface_Mesh " << m_cnt << " has " << 
                         sm->meshes.vertices.size() << " vertices and " <<
                         sm->meshes.faces.size() << " faces");
            }
            ++m_cnt;
        }
    }

    //Treatment plan data.
    {
        FUNCINFO("There are " << DICOM_data.tplan_data.size() << " TPlan_Configs loaded");

        size_t tp_cnt = 0;
        for(auto &tp : DICOM_data.tplan_data){
            if(tp == nullptr){
                FUNCINFO("  TPlan_Config " << tp_cnt << " is not valid");

            }else{
                FUNCINFO("  TPlan_Config " << tp_cnt << " has " <<
                         tp->dynamic_states.size() << " beams");

                size_t b_cnt = 0;
                for(const auto &ds : tp->dynamic_states){
                    FUNCINFO("    Beam " << b_cnt << " has " <<
                             ds.static_states.size() << " control points");
                    ++b_cnt;
                }
            }
            ++tp_cnt;
        }
    }

    return DICOM_data;
}
