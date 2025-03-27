//ExportOriginalFiles.cc - A part of DICOMautomaton 2024. Written by hal clark.

#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <set> 
#include <stdexcept>
#include <string>    
#include <filesystem>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Metadata.h"
#include "../Regex_Selectors.h"
#include "../Operation_Dispatcher.h"

#include "ExportOriginalFiles.h"


OperationDoc OpArgDocExportOriginalFiles() {
    OperationDoc out;
    out.name = "ExportOriginalFiles";
    out.tags.emplace_back("category: file export");

    out.desc = "This operation attempts to copy the original file associated with the selected object(s)."
               "\n\n"
               "Note that the original file may not be available, or may no longer be accessible."
               " For example, when the working directory has been modified and relative paths are used,"
               " or when network resources are used.";

    out.notes.emplace_back(
        "This operation does not modify the selection."
    );
    out.notes.emplace_back(
        "Selectors for this operation are only considered when you explicitly provide them."
        " The default values are not used by this operation."
    );


    const auto swap_backslashes = [](const std::string &s){
        return Lineate_Vector( SplitStringToVector(s, '\\', 'd'), "/" );
    };
    const auto tempdir = swap_backslashes( std::filesystem::temp_directory_path().string() );
    const auto tcd = swap_backslashes( (std::filesystem::temp_directory_path() / "dcma_exportoriginalfiles").string());
    out.args.emplace_back();
    out.args.back().name = "RootDirectory";
    out.args.back().desc = "The root directory in which to copy files.";
    out.args.back().default_val = tcd;
    out.args.back().expected = true;
    out.args.back().examples = { tempdir, 
                                 ".",
                                 "$HOME/dcma_exportoriginalfiles/" };

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = LSWhitelistOpArgDoc();
    out.args.back().name = "LineSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = T3WhitelistOpArgDoc();
    out.args.back().name = "TransformSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = TPWhitelistOpArgDoc();
    out.args.back().name = "RTPlanSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    return out;
}

bool ExportOriginalFiles(Drover &DICOM_data,
                         const OperationArgPkg& OptArgs,
                         std::map<std::string, std::string>& InvocationMetadata,
                         const std::string& FilenameLex){
    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto RootDirectory = OptArgs.getValueStr("RootDirectory").value();

    const auto NormalizedROILabelRegexOpt = OptArgs.getValueStr("NormalizedROILabelRegex");
    const auto ROILabelRegexOpt = OptArgs.getValueStr("ROILabelRegex");
    const auto ROISelectionOpt = OptArgs.getValueStr("ROISelection");

    const auto ImageSelectionOpt = OptArgs.getValueStr("ImageSelection");

    const auto LineSelectionOpt = OptArgs.getValueStr("LineSelection");

    const auto MeshSelectionOpt = OptArgs.getValueStr("MeshSelection");

    const auto PointSelectionOpt = OptArgs.getValueStr("PointSelection");

    const auto TransSelectionOpt = OptArgs.getValueStr("TransformSelection");

    const auto TableSelectionOpt = OptArgs.getValueStr("TableSelection");

    const auto RTPlanSelectionOpt = OptArgs.getValueStr("RTPlanSelection");
    //-----------------------------------------------------------------------------------------------------------------

    std::list<std::string> keys;
    keys.emplace_back("Fullpath");
    keys.emplace_back("FullPath");
    keys.emplace_back("Filename");
    keys.emplace_back("FileName");

    std::set<std::string> filenames;
    const auto ingest = [&](const std::set<std::string> &l_values) -> bool {
        for(const auto &v : l_values){
            filenames.insert(v);
        }

        // Indicate when something is provided for ingestion so we can terminate searching for more.
        return !l_values.empty();
    };

    if( ROILabelRegexOpt
    ||  NormalizedROILabelRegexOpt
    ||  ROISelectionOpt ){
        auto CCs_all = All_CCs( DICOM_data );
        auto CCs = Whitelist( CCs_all, ROILabelRegexOpt, NormalizedROILabelRegexOpt, ROISelectionOpt );
        YLOGINFO("Selected " << CCs.size() << " contour ROIs using selector");

        for(const auto& cc_refw : CCs){
            for(const auto &key : keys){
                const bool found = ingest( Extract_Distinct_Values( &(cc_refw.get()), key ) );
                if(found) break;
            }
        }
    }

    if(ImageSelectionOpt){
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionOpt.value() );
        YLOGINFO("Selected " << IAs.size() << " image arrays using ImageSelection selector");

        for(const auto& x_it_ptr : IAs){
            for(const auto &key : keys){
                const bool found = ingest( Extract_Distinct_Values( *x_it_ptr, key ) );
                if(found) break;
            }
        }
    }

    if(PointSelectionOpt){
        auto PCs_all = All_PCs( DICOM_data );
        auto PCs = Whitelist( PCs_all, PointSelectionOpt.value() );
        YLOGINFO("Selected " << PCs.size() << " point clouds using PointSelection selector");

        for(const auto& x_it_ptr : PCs){
            for(const auto &key : keys){
                const bool found = ingest( Extract_Distinct_Values( *x_it_ptr, key ) );
                if(found) break;
            }
        }
    }

    if(MeshSelectionOpt){
        auto SMs_all = All_SMs( DICOM_data );
        auto SMs = Whitelist( SMs_all, MeshSelectionOpt.value() );
        YLOGINFO("Selected " << SMs.size() << " surface meshes using MeshSelection selector");

        for(const auto& x_it_ptr : SMs){
            for(const auto &key : keys){
                const bool found = ingest( Extract_Distinct_Values( *x_it_ptr, key ) );
                if(found) break;
            }
        }
    }

    if(RTPlanSelectionOpt){
        auto TPs_all = All_TPs( DICOM_data );
        auto TPs = Whitelist( TPs_all, RTPlanSelectionOpt.value() );
        YLOGINFO("Selected " << TPs.size() << " tables using RTPlanSelection selector");

        for(const auto& x_it_ptr : TPs){
            for(const auto &key : keys){
                const bool found = ingest( Extract_Distinct_Values( *x_it_ptr, key ) );
                if(found) break;
            }
        }
    }

    if(LineSelectionOpt){
        auto LSs_all = All_LSs( DICOM_data );
        auto LSs = Whitelist( LSs_all, LineSelectionOpt.value() );
        YLOGINFO("Selected " << LSs.size() << " line samples using LineSelection selector");

        for(const auto& x_it_ptr : LSs){
            for(const auto &key : keys){
                const bool found = ingest( Extract_Distinct_Values( *x_it_ptr, key ) );
                if(found) break;
            }
        }
    }

    if(TransSelectionOpt){
        auto T3s_all = All_T3s( DICOM_data );
        auto T3s = Whitelist( T3s_all, TransSelectionOpt.value() );
        YLOGINFO("Selected " << T3s.size() << " tables using TransSelection selector");

        for(const auto& x_it_ptr : T3s){
            for(const auto &key : keys){
                const bool found = ingest( Extract_Distinct_Values( *x_it_ptr, key ) );
                if(found) break;
            }
        }
    }

    if(TableSelectionOpt){
        auto STs_all = All_STs( DICOM_data );
        auto STs = Whitelist( STs_all, TableSelectionOpt.value() );
        YLOGINFO("Selected " << STs.size() << " tables using TableSelection selector");

        for(const auto& x_it_ptr : STs){
            for(const auto &key : keys){
                const bool found = ingest( Extract_Distinct_Values( *x_it_ptr, key ) );
                if(found) break;
            }
        }
    }


    // Copy the files.
    bool copied_all = true;
    for(const auto &filename : filenames){
        try{
            const auto f = std::filesystem::path(filename);
            const auto f_name = f.filename();
            const auto f_ext = f_name.extension();

            const auto find_next_available_filename = [&](){
                auto t = std::filesystem::path(RootDirectory) / f_name;
                if(!std::filesystem::exists(t)){
                    return t;
                }

                for(int64_t i = 0; i < 50'000; ++i){
                    auto l_f_name = f_name;
                    l_f_name.replace_extension("_"_s + std::to_string(i) + f_ext.string());;
                    t = std::filesystem::path(RootDirectory) / l_f_name;
                    if(!std::filesystem::exists(t)){
                        return t;
                    }
                }
                throw std::runtime_error("Unable to identify an unused destination filename");
                return t;
            };

            const auto t = find_next_available_filename();

            YLOGDEBUG("Attempting to copy '" << f.string() << "' to '" << t.string() << "'");
            std::filesystem::copy(f, t);

        }catch(const std::exception &e){
            copied_all = false;
            YLOGWARN("Unable to copy '" << filename << "'. Continuing with other exports");
        }
    }

    return copied_all;
}

