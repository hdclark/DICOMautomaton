//DroverDebug.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <cstdlib>            //Needed for exit() calls.
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <variant>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DroverDebug.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


static void dump_metadata(std::ostream &os,
                          std::string indent,
                          const std::map<std::string, std::string> m){
    for(const auto &p : m){
        os << indent << "'" << p.first << "' : '" << p.second << "'" << std::endl;
    }
    return;
}

OperationDoc OpArgDocDroverDebug(void){
    OperationDoc out;
    out.name = "DroverDebug";

    out.desc = 
        "This operation reports basic information on the state of the main Drover class."
        " It can be used to report on the state of the data, which can be useful for debugging.";

    out.args.emplace_back();
    out.args.back().name = "IncludeMetadata";
    out.args.back().desc = "Whether to include metadata in the output."
                           " This data can significantly increase the size of the output.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };

    return out;
}



Drover DroverDebug(Drover DICOM_data, 
                   OperationArgPkg OptArgs, 
                   std::map<std::string,std::string> /*InvocationMetadata*/, 
                   std::string /*FilenameLex*/ ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto IncludeMetadataStr = OptArgs.getValueStr("IncludeMetadata").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");

    const auto IncludeMetadata = std::regex_match(IncludeMetadataStr, regex_true);

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
                    if(IncludeMetadata){
                        FUNCINFO("    Image " << i_num << " metadata:");
                        dump_metadata(std::cout, "        ", img.metadata);
                    }
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
                     c_dat <<
                     " has " <<
                     cc.contours.size() <<
                     " contours");
            size_t c_num = 0;
            for(auto & c : cc.contours){
                FUNCINFO("    contour " <<
                         c_num <<
                         " has " <<
                         c.points.size() <<
                         " vertices");
                if(!c.points.empty()){
                    FUNCINFO("      contour " <<
                         c_num <<
                         " has average point " <<
                         c.Average_Point());
                }
                if(IncludeMetadata){
                    FUNCINFO("      contour " << c_num << " metadata:");
                    dump_metadata(std::cout, "          ", c.metadata);
                }
                ++c_num;
            }
            ++c_dat;
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
                         pc->pset.points.size() << " points");
                if(IncludeMetadata){
                    FUNCINFO("    Point_Cloud " << p_cnt << " metadata:");
                    dump_metadata(std::cout, "        ", pc->pset.metadata);
                }
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
                if(IncludeMetadata){
                    FUNCINFO("    Surface_Mesh " << m_cnt << " metadata:");
                    dump_metadata(std::cout, "        ", sm->meshes.metadata);
                }
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
                if(IncludeMetadata){
                    FUNCINFO("  TPlan_Config " << tp_cnt << " metadata:");
                    dump_metadata(std::cout, "      ", tp->metadata);
                }

                size_t b_cnt = 0;
                for(const auto &ds : tp->dynamic_states){
                    FUNCINFO("    Beam " << b_cnt << " has " <<
                             ds.static_states.size() << " control points");
                    if(IncludeMetadata){
                        FUNCINFO("      Beam " << b_cnt << " metadata:");
                        dump_metadata(std::cout, "          ", ds.metadata);
                    }
                    ++b_cnt;
                }
            }
            ++tp_cnt;
        }
    }

    //Line sample data.
    {
        FUNCINFO("There are " << DICOM_data.lsamp_data.size() << " Line_Samples loaded");

        size_t l_cnt = 0;
        for(auto &lsp : DICOM_data.lsamp_data){
            if(lsp == nullptr){
                FUNCINFO("  Line_Sample " << l_cnt << " is not valid");

            }else{
                FUNCINFO("  Line_Sample " << l_cnt << " has " << 
                         lsp->line.samples.size() << " datum and " <<
                         lsp->line.metadata.size() << " metadata keys");
                if(IncludeMetadata){
                    FUNCINFO("    Line_Sample " << l_cnt << " metadata:");
                    dump_metadata(std::cout, "        ", lsp->line.metadata);
                }
            }
            ++l_cnt;
        }
    }

    //Transformation data.
    {
        FUNCINFO("There are " << DICOM_data.trans_data.size() << " Transform3s loaded");

        size_t t_cnt = 0;
        for(auto &t3p : DICOM_data.trans_data){

            if( (t3p == nullptr)
            ||  std::holds_alternative<std::monostate>(t3p->transform) ){
                FUNCINFO("  Transform3 " << t_cnt << " is not valid");

            }else{
                const std::string desc = std::visit([&](auto &&t) -> std::string {
                    using V = std::decay_t<decltype(t)>;
                    if constexpr (std::is_same_v<V, std::monostate>){
                        throw std::logic_error("Transformation is invalid. Unable to continue.");
                    }else if constexpr (std::is_same_v<V, affine_transform<double>>){
                        return "an Affine transformation";
                    }else{
                        static_assert(std::is_same_v<V,void>, "Transformation not understood.");
                    }
                    return "";
                }, t3p->transform);

                FUNCINFO("  Transform3 " << t_cnt << " holds " << desc);
                FUNCINFO("  Transform3 " << t_cnt << " has " << 
                         t3p->metadata.size() << " metadata keys");

                if(IncludeMetadata){
                    FUNCINFO("    Transform3 " << t_cnt << " metadata:");
                    dump_metadata(std::cout, "        ", t3p->metadata);
                }
            }
            ++t_cnt;
        }
    }

    return DICOM_data;
}
