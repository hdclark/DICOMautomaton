//ImprintImages.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <asio.hpp>
#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../YgorImages_Functors/ConvenienceRoutines.h"

#include "ImprintImages.h"



OperationDoc OpArgDocImprintImages(){
    OperationDoc out;
    out.name = "ImprintImages";

    out.desc = 
        "This operation creates imprints of point clouds on the selected images."
        " Images are modified where the points are coindicident.";
        
    
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "VoxelValue";
    out.args.back().desc = "The value to give voxels which are coincident with a point from the point cloud."
                           " Note that point cloud attributes, if present, may override this value.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1.0", "0.0", "1.23", "nan", "inf" };

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The image channel to use. Zero-based.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "1", "2" };

    return out;
}



bool ImprintImages(Drover &DICOM_data,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto PointSelectionStr = OptArgs.getValueStr("PointSelection").value();

    const auto VoxelValue = std::stod( OptArgs.getValueStr("VoxelValue").value() );
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    //-----------------------------------------------------------------------------------------------------------------

    auto PCs_all = All_PCs( DICOM_data );
    auto PCs = Whitelist( PCs_all, PointSelectionStr );

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );

    for(auto & pcp_it : PCs){
        for(auto & iap_it : IAs){
            for(auto &img : (*iap_it)->imagecoll.images){

                if(false){
/*

// TODO : updated when attributes are supported.

                // If there is a magnitude attribute, use it.
                }else if( (*pcp_it)->attributes.count("magnitude") != 0 ){
                    // Verify it is a simple scalar attribute.
                    auto *magn = std::any_cast<std::vector<double>>( &((*pcp_it)->attributes["magnitude"]) );
                    if( (magn == nullptr) 
                    ||  (magn->size() != (*pcp_it)->points.size()) ){
                        throw std::runtime_error("Point cloud magnitude present, but invalid. Refusing to continue.");
                    }

                    // Imprint the images.
                    auto m_it = std::begin(*magn);
                    for(const auto &pp : (*pcp_it)->points){
                        const auto P = pp.first;
                        const auto index = img.index( P, Channel );
                        if(0 <= index){
                            img.reference(index) = *m_it;
                        }
                        ++m_it;
                    }


*/                
                // If there is no magnitude attribute, simply use the user-provided VoxelValue.
                }else{
                    for(const auto &P : (*pcp_it)->pset.points){
                        const auto index = img.index( P, Channel );
                        if(0 <= index) img.reference(index) = VoxelValue;
                    }
                }
            }
        }
    }

    // Update the image window and level for display.
    for(auto & iap_it : IAs){
        for(auto & img_refw : (*iap_it)->imagecoll.images){
            UpdateImageWindowCentreWidth( img_refw );
            UpdateImageDescription( img_refw, "Imprinted point cloud" );
        }
    }

    return true;
}
