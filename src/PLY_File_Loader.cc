//PLY_File_Loader.cc - A part of DICOMautomaton 2020. Written by hal clark.
//
// This program loads surface meshes or point clouds from PLY files.
//

#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    

#include <cstdlib>            //Needed for exit() calls.

#include <boost/filesystem.hpp>

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathIOPLY.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for SplitStringToVector, Canonicalize_String2, SplitVector functions.

#include "Structs.h"
#include "Imebra_Shim.h"


bool Load_From_PLY_Files( Drover &DICOM_data,
                          const std::map<std::string,std::string> & /* InvocationMetadata */,
                          const std::string &,
                          std::list<boost::filesystem::path> &Filenames ){

    //This routine will attempt to load PLY-format files as surface meshes or point clouds. The difference between a
    // mesh and a point cloud, for the purposes of this routine, is the presence of one or more faces; if there are
    // faces, then the file contains a mesh. Note that only a minimal, basic subset of PLY is supported.
    // This routine will most likely reject non-PLY files since the header has a rigid structured.
    //
    // Note: This routine supports both ASCII and binary PLY files. In fact the header for both files is the same.
    //       However, line endings *might* be problematic on some systems. If problems are encountered, consider making
    //       all line endings in the (text) header equal to '\n' -- namely, replace '\r\n' or '\r' with '\n'.
    //
    // Note: This routine returns false only iff a file is suspected of being suited for this loader, but could not be
    //       loaded (e.g., the file seems appropriate, but a parsing failure was encountered).
    //
    if(Filenames.empty()) return true;

    size_t i = 0;
    const size_t N = Filenames.size();

    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        FUNCINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = bfit->string();

        DICOM_data.smesh_data.emplace_back( std::make_shared<Surface_Mesh>() );

        try{
            //////////////////////////////////////////////////////////////
            // Attempt to load the file.
            std::ifstream FI(Filename.c_str(), std::ios::in | std::ios::binary);
            if(!ReadFVSMeshFromPLY(DICOM_data.smesh_data.back()->meshes, FI)){
                throw std::runtime_error("Unable to read mesh or point cloud from file.");
            }
            FI.close();
            //////////////////////////////////////////////////////////////

            // Reject the file if the mesh is not valid.
            const auto N_verts = DICOM_data.smesh_data.back()->meshes.vertices.size();
            const auto N_faces = DICOM_data.smesh_data.back()->meshes.faces.size();
            if( N_verts == 0 ){
                throw std::runtime_error("Unable to read mesh or point cloud from file.");
            }

            // Supply generic minimal metadata iff it is needed.
            std::map<std::string, std::string> generic_metadata;
            generic_metadata["Filename"] = Filename; 

            generic_metadata["PatientID"] = "unspecified";
            generic_metadata["StudyInstanceUID"] = Generate_Random_UID(60);
            generic_metadata["SeriesInstanceUID"] = Generate_Random_UID(60);
            generic_metadata["FrameOfReferenceUID"] = Generate_Random_UID(60);
            generic_metadata["SOPInstanceUID"] = Generate_Random_UID(60);

            generic_metadata["ROIName"] = "unspecified"; 
            generic_metadata["NormalizedROIName"] = "unspecified"; 

            const bool is_surface_mesh = (N_faces != 0);
            if(is_surface_mesh){
                generic_metadata["Modality"] = "SurfaceMesh";
                generic_metadata["MeshName"] = "unspecified"; 
                generic_metadata["NormalizedMeshName"] = "unspecified"; 

                DICOM_data.smesh_data.back()->meshes.metadata.merge(generic_metadata);

                FUNCINFO("Loaded surface mesh with " 
                         << N_verts << " vertices and "
                         << N_faces << " faces");

            }else{ // then it must be a point cloud.
                generic_metadata["Modality"] = "PointCloud";
                generic_metadata["PointName"] = "unspecified"; 
                generic_metadata["NormalizedPointName"] = "unspecified"; 

                // Transfer the relevant data to a point cloud.
                // Note: done in this order to (1) avoid copies, and (2) to minimize risk of throw leaving an empty mesh.
                auto pset = DICOM_data.smesh_data.back()->meshes.convert_to_point_set();

                DICOM_data.point_data.emplace_back( std::make_shared<Point_Cloud>() );
                DICOM_data.point_data.back()->pset.swap( pset );
                DICOM_data.smesh_data.pop_back();

                DICOM_data.point_data.back()->pset.metadata.merge(generic_metadata);

                FUNCINFO("Loaded point cloud with " << N_verts << " poinnts");
            }

            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            FUNCINFO("Unable to load as PLY mesh or point cloud file");
            DICOM_data.smesh_data.pop_back();
        };

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }

    return true;
}
