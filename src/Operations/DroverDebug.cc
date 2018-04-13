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
        " This operation reports basic information on the state of the main Drover class."
        " It can be used to report on the state of the data, which can be useful for debugging.";

    return out;
}



Drover DroverDebug(Drover DICOM_data, 
                   OperationArgPkg /*OptArgs*/, 
                   std::map<std::string,std::string> /*InvocationMetadata*/, 
                   std::string /*FilenameLex*/ ){

    //Dose data.
    {
        FUNCINFO("There are " <<
                 DICOM_data.dose_data.size() <<
                 " Dose_Arrays loaded");

        size_t d_arr = 0;
        for(auto &dap : DICOM_data.dose_data){
            FUNCINFO("  Dose_Array " <<
                     d_arr++ <<
                     " has " <<
                     dap->imagecoll.images.size() <<
                     " image slices");
        }
    }

    //Image data.
    {
        FUNCINFO("There are " <<
                 DICOM_data.image_data.size() <<
                 " Image_Arrays loaded");

        size_t i_arr = 0;
        for(auto &iap : DICOM_data.image_data){
            FUNCINFO("  Image_Array " <<
                     i_arr++ <<
                     " has " <<
                     iap->imagecoll.images.size() <<
                     " image slices");
            size_t i_num = 0;
            for(auto &img : iap->imagecoll.images){
                FUNCINFO("    Image " <<
                         i_num++ <<
                         " has pxl_dx, pxl_dy, pxl_dz = " <<
                         img.pxl_dx << ", " <<
                         img.pxl_dy << ", " <<
                         img.pxl_dz);
                FUNCINFO("    Image " <<
                         (i_num-1) <<
                         " has anchor, offset = " <<
                         img.anchor << ", " <<
                         img.offset);
                FUNCINFO("    Image " <<
                         (i_num-1) <<
                         " has row_unit, col_unit = " <<
                         img.row_unit << ", " <<
                         img.col_unit);
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

    return DICOM_data;
}
