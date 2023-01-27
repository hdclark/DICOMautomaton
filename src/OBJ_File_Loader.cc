//OBJ_File_Loader.cc - A part of DICOMautomaton 2019. Written by hal clark.
//
// This program loads surface meshes from OBJ files.
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

#include <filesystem>

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathIOOBJ.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorString.h"       //Needed for SplitStringToVector, Canonicalize_String2, SplitVector functions.

#include "Structs.h"
#include "Imebra_Shim.h"


bool Load_Points_From_OBJ_Files( Drover &DICOM_data,
                                 std::map<std::string,std::string> & /* InvocationMetadata */,
                                 const std::string &,
                                 std::list<std::filesystem::path> &Filenames ){

    //This routine will attempt to load OBJ-format files as point clouds. Note that not all OBJ files contain point
    // clouds, and support for OBJ files is limited to a simplified subset. Note that a non-OBJ file that is passed
    // to this routine will be fully parsed as an OBJ file in order to assess validity. This can be problematic for
    // multiple reasons.
    //
    // Note: This routine returns false only iff a file is suspected of being suited for this loader, but could not be
    //       loaded (e.g., the file seems appropriate, but a parsing failure was encountered).
    //
    if(Filenames.empty()) return true;

    size_t i = 0;
    const size_t N = Filenames.size();

    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        YLOGINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = *bfit;

        DICOM_data.point_data.emplace_back( std::make_shared<Point_Cloud>() );

        try{
            //////////////////////////////////////////////////////////////
            // Attempt to load the file.
            std::ifstream FI(Filename, std::ios::in);
            if(!ReadPointSetFromOBJ(DICOM_data.point_data.back()->pset, FI)){
                throw std::runtime_error("Unable to read mesh from file.");
            }
            FI.close();
            //////////////////////////////////////////////////////////////

            // Reject the file if the point cloud is not valid.
            const auto N_points = DICOM_data.point_data.back()->pset.points.size();
            if( N_points == 0 ){
                throw std::runtime_error("Unable to read point cloud from file.");
            }

            // Supply generic minimal metadata iff it is needed.
            std::map<std::string, std::string> generic_metadata;

            generic_metadata["Filename"] = Filename.string(); 

            generic_metadata["PatientID"] = "unspecified";
            generic_metadata["StudyInstanceUID"] = Generate_Random_UID(60);
            generic_metadata["SeriesInstanceUID"] = Generate_Random_UID(60);
            generic_metadata["FrameOfReferenceUID"] = Generate_Random_UID(60);
            generic_metadata["SOPInstanceUID"] = Generate_Random_UID(60);
            generic_metadata["Modality"] = "PointCloud";

            generic_metadata["PointName"] = "unspecified"; 
            generic_metadata["NormalizedPointName"] = "unspecified"; 

            generic_metadata["ROIName"] = "unspecified"; 
            generic_metadata["NormalizedROIName"] = "unspecified"; 
            DICOM_data.point_data.back()->pset.metadata.merge(generic_metadata);

            YLOGINFO("Loaded point cloud with " << N_points << " points");
            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            YLOGINFO("Unable to load as OBJ point cloud file");
            DICOM_data.point_data.pop_back();
        };

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }

    return true;
}

bool Load_Mesh_From_OBJ_Files( Drover &DICOM_data,
                               std::map<std::string,std::string> & /* InvocationMetadata */,
                               const std::string &,
                               std::list<std::filesystem::path> &Filenames ){

    //This routine will attempt to load OBJ-format files as surface meshes. Note that not all OBJ files contain meshes,
    // and support for OBJ files is limited to a simplified subset. Note that a non-OBJ file that is passed to this
    // routine will be fully parsed as an OBJ file in order to assess validity. This can be problematic for multiple
    // reasons.
    //
    // Note: This routine returns false only iff a file is suspected of being suited for this loader, but could not be
    //       loaded (e.g., the file seems appropriate, but a parsing failure was encountered).
    //
    if(Filenames.empty()) return true;

    size_t i = 0;
    const size_t N = Filenames.size();

    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        YLOGINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = *bfit;

        DICOM_data.smesh_data.emplace_back( std::make_shared<Surface_Mesh>() );

        try{
            //////////////////////////////////////////////////////////////
            // Attempt to load the file.
            std::ifstream FI(Filename, std::ios::in);
            if(!ReadFVSMeshFromOBJ(DICOM_data.smesh_data.back()->meshes, FI)){
                throw std::runtime_error("Unable to read mesh from file.");
            }
            FI.close();
            //////////////////////////////////////////////////////////////

            // Reject the file if the mesh is not valid.
            const auto N_verts = DICOM_data.smesh_data.back()->meshes.vertices.size();
            const auto N_faces = DICOM_data.smesh_data.back()->meshes.faces.size();
            if( (N_verts == 0)
            ||  (N_faces == 0) ){
                throw std::runtime_error("Unable to read mesh from file.");
            }

            // Supply generic minimal metadata iff it is needed.
            std::map<std::string, std::string> generic_metadata;

            generic_metadata["Filename"] = Filename.string(); 

            generic_metadata["PatientID"] = "unspecified";
            generic_metadata["StudyInstanceUID"] = Generate_Random_UID(60);
            generic_metadata["SeriesInstanceUID"] = Generate_Random_UID(60);
            generic_metadata["FrameOfReferenceUID"] = Generate_Random_UID(60);
            generic_metadata["SOPInstanceUID"] = Generate_Random_UID(60);
            generic_metadata["Modality"] = "SurfaceMesh";

            generic_metadata["MeshName"] = "unspecified"; 
            generic_metadata["NormalizedMeshName"] = "unspecified"; 

            generic_metadata["ROIName"] = "unspecified"; 
            generic_metadata["NormalizedROIName"] = "unspecified"; 
            DICOM_data.smesh_data.back()->meshes.metadata.merge(generic_metadata);

            YLOGINFO("Loaded surface mesh with " 
                     << N_verts << " vertices and "
                     << N_faces << " faces");
            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            YLOGINFO("Unable to load as OBJ mesh file");
            DICOM_data.smesh_data.pop_back();
        };

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }

    return true;
}
