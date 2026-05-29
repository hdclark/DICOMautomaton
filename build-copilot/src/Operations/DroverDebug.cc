//DroverDebug.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <cstdlib>            //Needed for exit() calls.
#include <fstream>
#include <list>
#include <map>
#include <mutex>
#include <memory>
#include <string>
#include <variant>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"

#include "../Structs.h"
#include "../Alignment_Rigid.h"
#include "../Alignment_TPSRPM.h"
#include "../Alignment_Field.h"
#include "../Regex_Selectors.h"

#include "DroverDebug.h"


static void dump_metadata(std::ostream &os,
                          const std::string& indent,
                          const std::map<std::string, std::string>& m){
    std::lock_guard<std::mutex> lock(ygor::g_term_sync);
    for(const auto &p : m){
        os << indent << "'" << p.first << "' : '" << p.second << "'" << std::endl;
    }
    return;
}

OperationDoc OpArgDocDroverDebug(){
    OperationDoc out;
    out.name = "DroverDebug";

    out.tags.emplace_back("category: meta");

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
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Verbosity";
    out.args.back().desc = "Controls the amount of information printed.";
    out.args.back().default_val = "verbose";
    out.args.back().expected = true;
    out.args.back().examples = { "verbose", "medium", "quiet" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}



bool DroverDebug(Drover &DICOM_data,
                 const OperationArgPkg& OptArgs,
                 std::map<std::string, std::string>& InvocationMetadata,
                 const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto IncludeMetadataStr = OptArgs.getValueStr("IncludeMetadata").value();
    const auto VerbosityStr = OptArgs.getValueStr("Verbosity").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");
    const auto regex_verbose = Compile_Regex("^ve?r?b?o?s?e?$");
    const auto regex_medium = Compile_Regex("^me?d?i?u?m?$");
    const auto regex_quiet = Compile_Regex("^qu?i?e?t?$");

    const auto IncludeMetadata = std::regex_match(IncludeMetadataStr, regex_true);

    enum class verbosity_t {
        verbose,
        medium,
        quiet,
    } verbosity = verbosity_t::verbose;
    if(std::regex_match(VerbosityStr, regex_verbose)){
        verbosity = verbosity_t::verbose;
    }else if(std::regex_match(VerbosityStr, regex_medium)){
        verbosity = verbosity_t::medium;
    }else if(std::regex_match(VerbosityStr, regex_quiet)){
        verbosity = verbosity_t::quiet;
    }else{
        throw std::invalid_argument("Verbosity level not understood");
    }

    //Image data.
    do{
        YLOGINFO("There are " << DICOM_data.image_data.size() << " Image_Arrays loaded");
        if(verbosity == verbosity_t::quiet) break;

        size_t i_arr = 0;
        for(auto &iap : DICOM_data.image_data){
            if(iap == nullptr){
                YLOGINFO("  Image_Array " << i_arr << " is not valid");

            }else{
                YLOGINFO("  Image_Array " << i_arr << " has " <<
                         iap->imagecoll.images.size() << " image slices");
                if(verbosity == verbosity_t::medium) continue;

                size_t i_num = 0;
                for(auto &img : iap->imagecoll.images){
                    const auto ModalityOpt = img.GetMetadataValueAs<std::string>("Modality");
                    const auto mm = img.minmax();

                    YLOGINFO("    Image " << i_num << " has" <<
                             " Modality = " << ModalityOpt.value_or("(unspecified)") );
                    YLOGINFO("    Image " << i_num << " has" <<
                             " pixel value range = [" << mm.first << "," << mm.second << "]");
                    YLOGINFO("    Image " << i_num << " has" <<
                             " has pxl_dx, pxl_dy, pxl_dz = " <<
                                 img.pxl_dx << ", " <<
                                 img.pxl_dy << ", " <<
                                 img.pxl_dz);
                    YLOGINFO("    Image " << i_num << " has" <<
                             " anchor, offset = " <<
                                 img.anchor << ", " <<
                                 img.offset);
                    YLOGINFO("    Image " << i_num << " has" <<
                             " row_unit, col_unit = " <<
                                 img.row_unit << ", " <<
                                 img.col_unit);
                    if(IncludeMetadata){
                        YLOGINFO("    Image " << i_num << " metadata:");
                        dump_metadata(std::cout, "        ", img.metadata);
                    }
                    ++i_num;
                }
                ++i_arr;
            }
        }
    }while(false);

    //Contour data.
    do{
        if(DICOM_data.contour_data == nullptr){
            YLOGINFO("There are 0 contour_collections loaded");
            break;
        }
        YLOGINFO("There are " <<
                 DICOM_data.contour_data->ccs.size() <<
                 " contour_collections loaded");
        if(verbosity == verbosity_t::quiet) break;

        size_t c_dat = 0;
        for(auto & cc : DICOM_data.contour_data->ccs){
            YLOGINFO("  contour_collection " <<
                     c_dat <<
                     " has " <<
                     cc.contours.size() <<
                     " contours");
            if(verbosity == verbosity_t::medium) continue;

            size_t c_num = 0;
            for(auto & c : cc.contours){
                YLOGINFO("    contour " <<
                         c_num <<
                         " has " <<
                         c.points.size() <<
                         " vertices");
                if(!c.points.empty()){
                    YLOGINFO("      contour " <<
                         c_num <<
                         " has average point " <<
                         c.Average_Point());
                }
                if(IncludeMetadata){
                    YLOGINFO("      contour " << c_num << " metadata:");
                    dump_metadata(std::cout, "          ", c.metadata);
                }
                ++c_num;
            }
            ++c_dat;
        }
    }while(false);

    //Point data.
    do{
        YLOGINFO("There are " << DICOM_data.point_data.size() << " Point_Clouds loaded");
        if(verbosity == verbosity_t::quiet) break;

        size_t p_cnt = 0;
        for(auto &pc : DICOM_data.point_data){
            if(pc == nullptr){
                YLOGINFO("  Point_Cloud " << p_cnt << " is not valid");

            }else{
                YLOGINFO("  Point_Cloud " << p_cnt << " has " <<
                         pc->pset.points.size() << " points");
                if(verbosity == verbosity_t::medium) continue;
                if(IncludeMetadata){
                    YLOGINFO("    Point_Cloud " << p_cnt << " metadata:");
                    dump_metadata(std::cout, "        ", pc->pset.metadata);
                }
            }
            ++p_cnt;
        }
    }while(false);

    //Surface mesh data.
    do{
        YLOGINFO("There are " << DICOM_data.smesh_data.size() << " Surface_Meshes loaded");
        if(verbosity == verbosity_t::quiet) break;

        size_t m_cnt = 0;
        for(auto &sm : DICOM_data.smesh_data){
            if(sm == nullptr){
                YLOGINFO("  Surface_Mesh " << m_cnt << " is not valid");

            }else{
                YLOGINFO("  Surface_Mesh " << m_cnt << " has " << 
                         sm->meshes.vertices.size() << " vertices and " <<
                         sm->meshes.faces.size() << " faces");
                if(verbosity == verbosity_t::medium) continue;
                if(IncludeMetadata){
                    YLOGINFO("    Surface_Mesh " << m_cnt << " metadata:");
                    dump_metadata(std::cout, "        ", sm->meshes.metadata);
                }
            }
            ++m_cnt;
        }
    }while(false);

    //Treatment plan data.
    do{
        YLOGINFO("There are " << DICOM_data.rtplan_data.size() << " RTPlans loaded");
        if(verbosity == verbosity_t::quiet) break;

        size_t tp_cnt = 0;
        for(auto &tp : DICOM_data.rtplan_data){
            if(tp == nullptr){
                YLOGINFO("  RTPlan " << tp_cnt << " is not valid");

            }else{
                YLOGINFO("  RTPlan " << tp_cnt << " has " <<
                         tp->dynamic_states.size() << " beams");
                if(verbosity == verbosity_t::medium) continue;
                if(IncludeMetadata){
                    YLOGINFO("  RTPlan " << tp_cnt << " metadata:");
                    dump_metadata(std::cout, "      ", tp->metadata);
                }

                size_t b_cnt = 0;
                for(const auto &ds : tp->dynamic_states){
                    YLOGINFO("    Beam " << b_cnt << " has " <<
                             ds.static_states.size() << " control points");
                    if(IncludeMetadata){
                        YLOGINFO("      Beam " << b_cnt << " metadata:");
                        dump_metadata(std::cout, "          ", ds.metadata);
                    }
                    ++b_cnt;
                }
            }
            ++tp_cnt;
        }
    }while(false);

    //Line sample data.
    do{
        YLOGINFO("There are " << DICOM_data.lsamp_data.size() << " Line_Samples loaded");
        if(verbosity == verbosity_t::quiet) break;

        size_t l_cnt = 0;
        for(auto &lsp : DICOM_data.lsamp_data){
            if(lsp == nullptr){
                YLOGINFO("  Line_Sample " << l_cnt << " is not valid");

            }else{
                YLOGINFO("  Line_Sample " << l_cnt << " has " << 
                         lsp->line.samples.size() << " datum and " <<
                         lsp->line.metadata.size() << " metadata keys");
                if(verbosity == verbosity_t::medium) continue;
                if(IncludeMetadata){
                    YLOGINFO("    Line_Sample " << l_cnt << " metadata:");
                    dump_metadata(std::cout, "        ", lsp->line.metadata);
                }
            }
            ++l_cnt;
        }
    }while(false);

    //Transformation data.
    do{
        YLOGINFO("There are " << DICOM_data.trans_data.size() << " Transform3s loaded");
        if(verbosity == verbosity_t::quiet) break;

        YLOGINFO("  The Transform3 class is " << sizeof(Transform3) << " bytes");

        size_t t_cnt = 0;
        for(auto &t3p : DICOM_data.trans_data){

            if( (t3p == nullptr)
            ||  std::holds_alternative<std::monostate>(t3p->transform) ){
                YLOGINFO("  Transform3 " << t_cnt << " is not valid");

            }else{
                const std::string desc = std::visit([&](auto &&t) -> std::string {
                    using V = std::decay_t<decltype(t)>;
                    if constexpr (std::is_same_v<V, std::monostate>){
                        throw std::logic_error("Transformation is invalid. Unable to continue.");
                    }else if constexpr (std::is_same_v<V, affine_transform<double>>){
                        return "an affine transformation";
                    }else if constexpr (std::is_same_v<V, thin_plate_spline>){
                        return "a thin-plate spline transformation";
                    }else if constexpr (std::is_same_v<V, deformation_field>){
                        return "a vector deformation field";
                    }else{
                        static_assert(std::is_same_v<V,void>, "Transformation not understood.");
                    }
                    return "";
                }, t3p->transform);

                YLOGINFO("  Transform3 " << t_cnt << " holds " << desc);
                YLOGINFO("  Transform3 " << t_cnt << " has " << 
                         t3p->metadata.size() << " metadata keys");

                if(verbosity == verbosity_t::medium) continue;
                if(IncludeMetadata){
                    YLOGINFO("    Transform3 " << t_cnt << " metadata:");
                    dump_metadata(std::cout, "        ", t3p->metadata);
                }
            }
            ++t_cnt;
        }
    }while(false);

    //Table data.
    do{
        YLOGINFO("There are " << DICOM_data.table_data.size() << " Sparse_Tables loaded");
        if(verbosity == verbosity_t::quiet) break;

        size_t t_cnt = 0;
        for(auto &tp : DICOM_data.table_data){
            if(tp == nullptr){
                YLOGINFO("  Sparse_Table " << t_cnt << " is not valid");

            }else{
                YLOGINFO("  Sparse_Table " << t_cnt << " has " << 
                         tp->table.data.size() << " rows and " <<
                         tp->table.metadata.size() << " metadata keys");
                if(verbosity == verbosity_t::medium) continue;
                if(IncludeMetadata){
                    YLOGINFO("    Sparse_Table " << t_cnt << " metadata:");
                    dump_metadata(std::cout, "        ", tp->table.metadata);
                }
            }
            ++t_cnt;
        }
    }while(false);

    //InvocationMetadata.
    do{
        YLOGINFO("There are " << InvocationMetadata.size() << " metadata parameters defined");
        if(verbosity == verbosity_t::quiet) break;
        if(verbosity == verbosity_t::medium) break;

        if(IncludeMetadata){
            dump_metadata(std::cout, "  ", InvocationMetadata);
        }
    }while(false);

    return true;
}
