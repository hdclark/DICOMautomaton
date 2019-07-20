//ConvertMeshesToContours.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <asio.hpp>
#include <algorithm>
#include <experimental/optional>
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

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "ConvertMeshesToContours.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"

#include "../Surface_Meshes.h"


OperationDoc OpArgDocConvertMeshesToContours(void){
    OperationDoc out;
    out.name = "ConvertMeshesToContours";

    out.desc = 
        "This operation constructs ROI contours by slicing the given meshes on a set of image planes.";
        
    out.notes.emplace_back(
        "Surface meshes should represent polyhedra."
    );
    out.notes.emplace_back(
        "This routine does **not** require images to be regular, rectilinear, or even contiguous."
    );
    out.notes.emplace_back(
        "Images and meshes are unaltered. Existing contours are ignored and unaltered."
    );
    out.notes.emplace_back(
        "Contour orientation is (likely) not guaranteed to be consistent in this routine."
    );
        

    out.args.emplace_back();
    out.args.back().name = "ROILabel";
    out.args.back().desc = "A label to attach to the ROI contours.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "body", "air", "bone", "invalid", "above_zero", "below_5.3" };


    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    return out;
}



Drover ConvertMeshesToContours(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabel = OptArgs.getValueStr("ROILabel").value();
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto NormalizedROILabel = X(ROILabel);

    //Construct a destination for the ROI contours.
    if(DICOM_data.contour_data == nullptr){
        std::unique_ptr<Contour_Data> output (new Contour_Data());
        DICOM_data.contour_data = std::move(output);
    }
    DICOM_data.contour_data->ccs.emplace_back();

    const double MinimumSeparation = 1.0; // TODO: is there a routine to do this? (YES: Unique_Contour_Planes()...)
    DICOM_data.contour_data->ccs.back().Raw_ROI_name = ROILabel;
    DICOM_data.contour_data->ccs.back().ROI_number = 10000; // TODO: find highest existing and ++ it.
    DICOM_data.contour_data->ccs.back().Minimum_Separation = MinimumSeparation;

    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );

    long int completed = 0;
    const auto sm_count = SMs.size();
    for(auto & smp_it : SMs){
        // Convert to a CGAL mesh.
        std::stringstream ss;
        if(!WriteFVSMeshToOFF( (*smp_it)->meshes, ss )){
            throw std::runtime_error("Unable to write mesh in OFF format. Cannot continue.");
        }

        dcma_surface_meshes::Polyhedron surface_mesh;
        if(!( ss >> surface_mesh )){
            throw std::runtime_error("Mesh could not be treated as a polyhedron. (Is it manifold?)");
        }

        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(auto & iap_it : IAs){
            for(const auto &animg : (*iap_it)->imagecoll.images){
                // Slice the mesh along the image plane.
                auto lcc = polyhedron_processing::Slice_Polyhedron( surface_mesh, 
                                                                    {{ animg.image_plane() }} );

                // Tag the contours with metadata.
                for(auto &cop : lcc.contours){
                    cop.closed = true;
                    cop.metadata["ROIName"] = ROILabel;
                    cop.metadata["NormalizedROIName"] = NormalizedROILabel;
                    cop.metadata["Description"] = "Sliced surface mesh";
                    cop.metadata["MinimumSeparation"] = std::to_string(MinimumSeparation);
                    for(const auto &key : { "StudyInstanceUID", "FrameofReferenceUID" }){
                        if(animg.metadata.count(key) != 0) cop.metadata[key] = animg.metadata.at(key);
                    }
                }

                DICOM_data.contour_data->ccs.back().contours.splice(DICOM_data.contour_data->ccs.back().contours.end(),
                                                                    lcc.contours);
            }
        }

        ++completed;
        FUNCINFO("Completed " << completed << " of " << sm_count
              << " --> " << static_cast<int>(1000.0*(completed)/sm_count)/10.0 << "\% done");
    }

    return DICOM_data;
}
