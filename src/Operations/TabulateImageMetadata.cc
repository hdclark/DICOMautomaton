//TabulateImageMetadata.cc - A part of DICOMautomaton 2022. Written by hal clark.

#include <cstdlib>            //Needed for exit() calls.
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <filesystem>

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Metadata.h"
#include "../Regex_Selectors.h"

#include "TabulateImageMetadata.h"


OperationDoc OpArgDocTabulateImageMetadata(){
    OperationDoc out;
    out.name = "TabulateImageMetadata";

    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: table processing");
    out.tags.emplace_back("category: metadata");

    out.desc = 
        "Extract metadata from images and write them in a tabular format.";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "Keys";
    out.args.back().desc = "A string of semicolon-separated metadata keys."
                           " The value corresponding to each key will be output."
                           " Images that do not have the metadata key will either return the user-provided"
                           " DefaultValue, or simply not output anything if ProvideDefault is false.";
    out.args.back().default_val = "PatientID;Filename";
    out.args.back().expected = true;
    out.args.back().examples = { "PatientID", "Filename", "Treatment plan C" };


    out.args.emplace_back();
    out.args.back().name = "ProvideDefault";
    out.args.back().desc = "If an image does not have the given metadata key, this option controls whether"
                           " the user-provided DefaultValue is output.";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "DefaultValue";
    out.args.back().desc = "If an image does not have the given metadata key, this default will be output"
                           " when ProvideDefault is true.";
    out.args.back().default_val = "NA";
    out.args.back().expected = true;
    out.args.back().examples = { "", "NA", "NULL", "1.23" };


    out.args.emplace_back();
    out.args.back().name = "UserComment";
    out.args.back().desc = "A string that will be inserted into the output."
                           " If left empty, the column will be empty in the output.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "Using XYZ", "Treatment plan C" };


    out.args.emplace_back();
    out.args.back().name = "TableShape";
    out.args.back().desc = "Controls the 'shape' of the output, i.e., whether all records appear on the same line"
                           " ('wide') or are split along several lines ('tall', i.e., key-value shape).";
    out.args.back().default_val = "wide";
    out.args.back().expected = true;
    out.args.back().examples = { "wide", "tall" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "IncludeHeader";
    out.args.back().desc = "Controls whether a 'header' is output. Note that the header refers to the metadata"
                           "keys, which may appear in different places depending on the TableShape";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "TableLabel";
    out.args.back().desc = "A label to attach to the new table.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "xyz", "sheet A" };

    return out;
}

bool TabulateImageMetadata(Drover &DICOM_data,
                                          const OperationArgPkg& OptArgs,
                                          std::map<std::string, std::string>& /*InvocationMetadata*/,
                                          const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto KeysStr = OptArgs.getValueStr("Keys").value();

    const auto ProvideDefaultStr = OptArgs.getValueStr("ProvideDefault").value();
    const auto DefaultValueStr = OptArgs.getValueStr("DefaultValue").value();
    const auto UserCommentStr = OptArgs.getValueStr("UserComment").value();

    const auto TableShapeStr = OptArgs.getValueStr("TableShape").value();
    const auto IncludeHeaderStr = OptArgs.getValueStr("IncludeHeader").value();
    const auto TableLabelStr = OptArgs.getValueStr("TableLabel").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto NormalizedTableLabelStr = X(TableLabelStr);

    const auto regex_true = Compile_Regex("^tr?u?e?$");

    const auto regex_wide = Compile_Regex("^wi?d?e?$");
    const auto regex_tall = Compile_Regex("^ta?l?l?$");

    const bool ShouldProvideDefault = std::regex_match(ProvideDefaultStr, regex_true);
    const bool ShouldIncludeHeader = std::regex_match(IncludeHeaderStr, regex_true);

    const bool TableIsWide = std::regex_match(TableShapeStr, regex_wide);
    const bool TableIsTall = std::regex_match(TableShapeStr, regex_tall);

    if( !TableIsWide
    &&  !TableIsTall ){
        throw std::runtime_error("Unrecognized table shape");
    }

    if( !ShouldProvideDefault
    &&  TableIsWide ){
        throw std::runtime_error("Unwilling to combine wide table shape and ProvideDefault = false, which can result in jumbled output");
    }

    // Tokenize metadata keys.
    auto split_keys = SplitStringToVector(KeysStr, ';', 'd');
    split_keys = SplitVector(split_keys, ',', 'd');
    split_keys = SplitVector(split_keys, '\n', 'd');
    split_keys = SplitVector(split_keys, '\r', 'd');
    split_keys = SplitVector(split_keys, '\t', 'd');
    for(auto &ac : split_keys){
        const auto ctrim = CANONICALIZE::TRIM_ENDS;
        ac = Canonicalize_String2(ac, ctrim);
    }

    // Create new table for output.
    {
        auto meta = coalesce_metadata_for_basic_table({}, meta_evolve::iterate);
        DICOM_data.table_data.emplace_back( std::make_unique<Sparse_Table>() );
        DICOM_data.table_data.back()->table.metadata = meta;
        DICOM_data.table_data.back()->table.metadata["TableLabel"] = TableLabelStr;
        DICOM_data.table_data.back()->table.metadata["NormalizedTableLabel"] = NormalizedTableLabelStr;
        DICOM_data.table_data.back()->table.metadata["Description"] = "Generated via TabulateImageMetadata";
    }
    auto* t = &(DICOM_data.table_data.back()->table); 

    // Header.
    if(ShouldIncludeHeader){
        if(TableIsWide){
            const auto r = t->next_empty_row();
            int64_t c = 0;

            for(const auto& key : split_keys){
                t->inject(r, c++, key);
            }

            if(!UserCommentStr.empty()){
                t->inject(r, c++, "UserComment");
            }
        }
    }

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){

        for(const auto &animg : (*iap_it)->imagecoll.images){
            auto r = t->next_empty_row();
            int64_t c = 0;

            // Metadata.
            for(const auto& key : split_keys){
                auto val_opt = get_as<std::string>(animg.metadata, key);

                if(TableIsWide){
                    if(val_opt){
                        t->inject(r, c, val_opt.value());
                    }else if(ShouldProvideDefault){
                        t->inject(r, c, DefaultValueStr);
                    }
                    c++;

                }else if(TableIsTall){
                    if(val_opt){
                        if(ShouldIncludeHeader){
                            t->inject(r, c, key);
                            t->inject(r, c+1, val_opt.value());
                        }else{
                            t->inject(r, c, val_opt.value());
                        }
                        ++r;

                    }else if(ShouldProvideDefault){
                        if(ShouldIncludeHeader){
                            t->inject(r, c, key);
                            t->inject(r, c+1, DefaultValueStr);
                        }else{
                            t->inject(r, c, DefaultValueStr);
                        }
                        ++r;
                    }

                }else{
                    throw std::logic_error("Unexpected table shape");
                }
            }

            // UserComment.
            if(!UserCommentStr.empty()){
                if(TableIsWide){
                    t->inject(r, c, UserCommentStr);
                    c++;

                }else if(TableIsTall){
                    if(ShouldIncludeHeader){
                        t->inject(r, c, "UserComment");
                        t->inject(r, c+1, UserCommentStr);
                    }else{
                        t->inject(r, c, UserCommentStr);
                    }
                    ++r;

                }else{
                    throw std::logic_error("Unexpected table shape");
                }

            }
        }
    }

    return true;
}
