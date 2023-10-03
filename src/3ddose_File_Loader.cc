//3ddose_File_Loader.cc - A part of DICOMautomaton 2019. Written by hal clark.
//
// This program loads ASCII DOSXYZnrc 3ddose files.
//

#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <numeric>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    

#include <filesystem>
#include <cstdlib>            //Needed for exit() calls.

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorStats.h"        //Needed for Median().
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorString.h"       //Needed for SplitStringToVector, Canonicalize_String2, SplitVector functions.

#include "Structs.h"
#include "Imebra_Shim.h"      //Needed for Collate_Image_Arrays().


bool Load_From_3ddose_Files( Drover &DICOM_data,
                          std::map<std::string,std::string> & /* InvocationMetadata */,
                          const std::string &,
                          std::list<std::filesystem::path> &Filenames ){

    //This routine will attempt to load 3ddose-format files. Files that are not successfully loaded
    // are not consumed so that they can be passed on to the next loading stage as needed. 
    //
    // The format is described in NRCC Report PIRS-794revB, section 12. Only ASCII format is accepted. Multiple
    // separators are accepted, and whitespace is generally not significant (except if used as a separator between
    // numbers).
    //
    // It is hard to decide whether a given file is definitively in 3ddose format. The
    // threshold to decide is whether any single line contains a point that can be successfully read. If this happens,
    // the file is considered to be in 3ddose format. Therefore, it is best to attempt loading other, more strucured
    // formats if uncertain about the file type ahead of time.
    //
    // Note: This routine returns false only iff a file is suspected of being suited for this loader, but could not be
    //       loaded (e.g., the file seems appropriate, but a parsing failure was encountered).
    //
    // Note: Assumes the dose grid is both regular and axis-aligned.
    //
    if(Filenames.empty()) return true;

    const auto extract_separated_numbers = [](const std::string &in) -> std::vector<double> {
            // This rountine extracts numerical function parameters.
            auto split = SplitStringToVector(in, ' ', 'd');
            split = SplitVector(split, '\t', 'd');

            std::vector<double> numbers;
            for(const auto &w : split){
               try{
                   const auto x = std::stod(w);
                   numbers.emplace_back(x);
               }catch(const std::exception &){ }
            }
            return numbers;
    };

    size_t i = 0;
    const size_t N = Filenames.size();

    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        YLOGINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = *bfit;

        try{
            //////////////////////////////////////////////////////////////
            // Attempt to load the file.
            std::fstream FI(Filename, std::ios::in);
            if(!FI.good()){
                throw std::runtime_error("Unable to read file.");
            }

            bool dims_known = false;
            int64_t N_x = -1;
            int64_t N_y = -1;
            int64_t N_z = -1;

            std::vector<double> spatial_x;
            std::vector<double> spatial_y;
            std::vector<double> spatial_z;

            std::vector<double> doses;

/*
            try{
                double shtl;
                FI >> N_x >> N_y >> N_z;
                for(int64_t i = 0; i <= N_x; ++i){ // Intentionally reads N_x+1 values.
                    FI >> shtl;
                    spatial_x.emplace_back(shtl);
                }
                for(int64_t i = 0; i <= N_x; ++i){ // Intentionally reads N_y+1 values.
                    FI >> shtl;
                    spatial_x.emplace_back(shtl);
                }
                for(int64_t i = 0; i <= N_x; ++i){ // Intentionally reads N_z+1 values.
                    FI >> shtl;
                    spatial_x.emplace_back(shtl);
                }
                while(!FI.eof()){
                    FI >> shtl;
                    doses.emplace_back(shtl);
                }
            }catch(const std::exception &){ }
            FI.close();
*/            
            std::string aline;
            while(!FI.eof()){
                std::getline(FI, aline);

                // Ignore comments.
                const auto hash_pos = aline.find_first_of('#');
                if(hash_pos != std::string::npos){
                    aline = aline.substr(0, hash_pos);
                }

                // Extract all numbers separated by whitespace.
                const auto numbers = extract_separated_numbers(aline);
                if(numbers.empty()) continue;

                // If the matrix dimensions are not yet known, seek this info before reading any other information.
                //
                // Dimensional consistency is the only way to validate 3ddose files, so we use the dimensions to ensure
                // the correct amount of data has been received at the end.
                //
                // Since there is no 3ddose file header or magic numbers we have to ruthlessly reject files that do
                // not immediately present sane dimensions.
                if(!dims_known){
                    if(numbers.size() != 3) throw std::runtime_error("Dimensions not understood.");

                    N_x = static_cast<int64_t>(numbers.at(0));
                    N_y = static_cast<int64_t>(numbers.at(1));
                    N_z = static_cast<int64_t>(numbers.at(2));
                    if((N_x <= 0) || (N_y <= 0) || (N_z <= 0)){
                        throw std::runtime_error("Dimensions invalid.");
                    }
                    dims_known = true;
                    continue;

                // Attempt to parse spatial information.
                }else if( dims_known 
                      && (static_cast<int64_t>(spatial_x.size()) != (N_x + 1)) ){

                    for(const auto &n : numbers){
                        if(!std::isfinite(n)) continue;
                    }
                    spatial_x.insert( std::end(spatial_x), std::begin(numbers), std::end(numbers) );

                }else if( dims_known 
                      && (static_cast<int64_t>(spatial_x.size()) == (N_x + 1))
                      && (static_cast<int64_t>(spatial_y.size()) != (N_y + 1)) ){

                    for(const auto &n : numbers){
                        if(!std::isfinite(n)) continue;
                    }
                    spatial_y.insert( std::end(spatial_y), std::begin(numbers), std::end(numbers) );

                }else if( dims_known 
                      && (static_cast<int64_t>(spatial_x.size()) == (N_x + 1))
                      && (static_cast<int64_t>(spatial_y.size()) == (N_y + 1))
                      && (static_cast<int64_t>(spatial_z.size()) != (N_z + 1)) ){

                    for(const auto &n : numbers){
                        if(!std::isfinite(n)) continue;
                    }
                    spatial_z.insert( std::end(spatial_z), std::begin(numbers), std::end(numbers) );

                // Continue reading in all dose and trailing dose uncertainties.
                //
                // If the final number of voxels differs from the stated dimensions, then this file is not valid.
                }else if( dims_known 
                      && (static_cast<int64_t>(spatial_x.size()) == (N_x + 1))
                      && (static_cast<int64_t>(spatial_y.size()) == (N_y + 1))
                      && (static_cast<int64_t>(spatial_z.size()) == (N_z + 1)) ){
                    for(const auto &n : numbers){
                        if(!std::isfinite(n)) continue;
                    }

                    // Append the doses.
                    doses.insert( std::end(doses), std::begin(numbers), std::end(numbers) );

                // Unexpected scenario.
                }else{
                    YLOGWARN("Unable to read 3ddose file.");
                    return false;
                }

            }
            FI.close();

            // Validate that the file has been fully read.
            if( (static_cast<int64_t>(doses.size()) != (N_x * N_y * N_z))   // Dose data only.
            &&  (static_cast<int64_t>(doses.size()) != (N_x * N_y * N_z * 2L)) ){  // Dose data and uncertainties.
                throw std::runtime_error("Unable to read file.");
            }
            if( static_cast<int64_t>(doses.size()) == (N_x * N_y * N_z * 2L) ){
                doses.resize( static_cast<size_t>(N_x * N_y * N_z) );
            }

            //--------------------------------------------------------
            // Construct an Image_Array to hold the dose data.

            // Scale spatial factors from cm to mm. (The native 3ddose unit is cm but DICOM is mm.)
            const auto scale_cm_to_mm = [](std::vector<double> &boundaries){
                std::transform( std::begin(boundaries), std::end(boundaries),
                                std::begin(boundaries), [](double x) -> double { return x * 10.0; } );
                return;
            };
            scale_cm_to_mm(spatial_x);
            scale_cm_to_mm(spatial_y);
            scale_cm_to_mm(spatial_z);

            // Determine the grid spacing. Note: assumes a regular grid.
            const auto find_voxel_spacing = [](std::vector<double> boundaries) -> double {
                std::adjacent_difference(std::begin(boundaries), std::end(boundaries),
                                         std::begin(boundaries), 
                                         [](double x, double y){ return std::abs(y-x); });
                std::swap(boundaries.front(), boundaries.back()); // The first element is not a difference.
                boundaries.pop_back();
                return Stats::Median(boundaries);
            };
            const auto pxl_dx = find_voxel_spacing(spatial_x);
            const auto pxl_dy = find_voxel_spacing(spatial_y);
            const auto pxl_dz = find_voxel_spacing(spatial_z);

            // Find the centre of the first voxel.
            const auto offset_x = (spatial_x.at(0) + spatial_x.at(1)) * 0.5;
            const auto offset_y = (spatial_y.at(0) + spatial_y.at(1)) * 0.5;
            const auto offset_z = (spatial_z.at(0) + spatial_z.at(1)) * 0.5;

            const auto NumberOfImages = N_z;
            const auto NumberOfRows = N_y;
            const auto NumberOfColumns = N_x;
            const auto NumberOfChannels = 1L;

            const auto SliceThickness = pxl_dz;
            const auto SpacingBetweenSlices = pxl_dz;
            const auto VoxelWidth = pxl_dx;
            const auto VoxelHeight = pxl_dy;

            const auto ImageAnchor = vec3<double>(0.0, 0.0, 0.0);
            auto ImagePosition = vec3<double>( offset_x, offset_y, offset_z );

/*
            const auto ImageOrientationColumn = (  vec3<double>(1.0, 0.0, 0.0) * ( spatial_x.at(1) - spatial_x.at(0) )
                                                 + vec3<double>(0.0, 1.0, 0.0) * ( spatial_y.at(1) - spatial_y.at(0) )
                                                 + vec3<double>(0.0, 0.0, 1.0) * ( spatial_z.at(1) - spatial_z.at(0) )
                                                ).unit();

            const auto ImageOrientationRow = (  vec3<double>(1.0, 0.0, 0.0) * ( spatial_x.at(1) - spatial_x.at(0) )
                                              + vec3<double>(0.0, 1.0, 0.0) * ( spatial_y.at(1) - spatial_y.at(0) )
                                              + vec3<double>(0.0, 0.0, 1.0) * ( spatial_z.at(1) - spatial_z.at(0) )
                                             ).unit();

                                              + vec3<double>(0.0, 1.0, 0.0) * ( spatial_x.at(N_x+1) - spatial_x.at(0) )
                                              + vec3<double>(0.0, 1.0, 0.0) * ( spatial_x.at(N_y*(N_x+1)+1) - spatial_x.at(0) );
*/

            const auto ImageOrientationColumn = vec3<double>(1.0, 0.0, 0.0);
            const auto ImageOrientationRow = vec3<double>(0.0, 1.0, 0.0);
            const auto ImageOrientationOrtho = vec3<double>(0.0, 0.0, 1.0);

            auto InstanceNumber = 0L;
            const auto AcquisitionNumber = 0L;

            using loaded_imgs_storage_t = decltype(DICOM_data.image_data);
            std::list<loaded_imgs_storage_t> loaded_imgs_storage;

        /*
            // Temporal metadata.
            const std::string ContentDate = "20190427";
            const std::string ContentTime = "111558";

            // Other metadata.
            //const double RescaleSlope = 1.0;
            //const double RescaleIntercept = 0.0;
            const std::string OriginFilename = "/dev/null";
            const std::string PatientID = "SyntheticImage";

            const std::string StudyInstanceUID = PatientID + "_Study1";
            const std::string SeriesInstanceUID = StudyInstanceUID + "_Series1";
            const std::string FrameOfReferenceUID = PatientID;
        */    
            const std::string Modality = "RTDOSE";

            loaded_imgs_storage.emplace_back();
            for(int64_t img_index = 0; img_index < NumberOfImages; ++img_index){
                const std::string SOPInstanceUID = Generate_Random_String_of_Length(6);

                auto out = std::make_unique<Image_Array>();
                out->imagecoll.images.emplace_back();

                out->imagecoll.images.back().metadata["Filename"] = Filename.string();
                //out->imagecoll.images.back().metadata["PatientID"] = PatientID;
                //out->imagecoll.images.back().metadata["StudyInstanceUID"] = StudyInstanceUID;
                //out->imagecoll.images.back().metadata["SeriesInstanceUID"] = SeriesInstanceUID;
                out->imagecoll.images.back().metadata["SOPInstanceUID"] = SOPInstanceUID;

                out->imagecoll.images.back().metadata["Rows"] = std::to_string(NumberOfRows);
                out->imagecoll.images.back().metadata["Columns"] = std::to_string(NumberOfColumns);
                out->imagecoll.images.back().metadata["SliceThickness"] = std::to_string(SliceThickness);
                out->imagecoll.images.back().metadata["SpacingBetweenSlices"] = std::to_string(SpacingBetweenSlices);
                out->imagecoll.images.back().metadata["ImagePositionPatient"] = std::to_string(ImagePosition.x) + "\\"
                                                                              + std::to_string(ImagePosition.y) + "\\"
                                                                              + std::to_string(ImagePosition.z);
                out->imagecoll.images.back().metadata["ImageOrientationPatient"] = std::to_string(ImageOrientationRow.x) + "\\"
                                                                                 + std::to_string(ImageOrientationRow.y) + "\\"
                                                                                 + std::to_string(ImageOrientationRow.z) + "\\"
                                                                                 + std::to_string(ImageOrientationColumn.x) + "\\"
                                                                                 + std::to_string(ImageOrientationColumn.y) + "\\"
                                                                                 + std::to_string(ImageOrientationColumn.z);
                out->imagecoll.images.back().metadata["PixelSpacing"] = std::to_string(VoxelHeight) + "\\" + std::to_string(VoxelWidth);
                //out->imagecoll.images.back().metadata["FrameOfReferenceUID"] = FrameOfReferenceUID;

                //out->imagecoll.images.back().metadata["StudyTime"] = ContentTime;
                //out->imagecoll.images.back().metadata["SeriesTime"] = ContentTime;
                //out->imagecoll.images.back().metadata["AcquisitionTime"] = ContentTime;
                //out->imagecoll.images.back().metadata["ContentTime"] = ContentTime;

                //out->imagecoll.images.back().metadata["StudyDate"] = ContentDate;
                //out->imagecoll.images.back().metadata["SeriesDate"] = ContentDate;
                //out->imagecoll.images.back().metadata["AcquisitionDate"] = ContentDate;
                //out->imagecoll.images.back().metadata["ContentDate"] = ContentDate;

                //out->imagecoll.images.back().metadata["InstanceNumber"] = std::to_string(InstanceNumber);
                out->imagecoll.images.back().metadata["AcquisitionNumber"] = std::to_string(AcquisitionNumber);

                out->imagecoll.images.back().metadata["Modality"] = Modality;

                out->imagecoll.images.back().init_orientation(ImageOrientationRow, ImageOrientationColumn);
                out->imagecoll.images.back().init_buffer(NumberOfRows, NumberOfColumns, NumberOfChannels);
                out->imagecoll.images.back().init_spatial(VoxelWidth, VoxelHeight, SliceThickness, ImageAnchor, ImagePosition);

                for(int64_t y = 0; y < N_y; ++y){
                    for(int64_t x = 0; x < N_x; ++x){
                        const auto index = (N_x * N_y * img_index) + (N_x * y) + x;

                        out->imagecoll.images.back().reference(y, x, 0L) = doses.at(index);
                    }
                }

                ImagePosition += ImageOrientationOrtho * SpacingBetweenSlices;
                ++InstanceNumber;

                loaded_imgs_storage.back().push_back( std::move( out ) );
            }

            //Collate each group of images into a single set, if possible. Also stuff the correct contour data in the same set.
            for(auto &loaded_img_set : loaded_imgs_storage){
                if(loaded_img_set.empty()) continue;

                auto collated_imgs = Collate_Image_Arrays(loaded_img_set);
                if(!collated_imgs){
                    throw std::runtime_error("Unable to collate images.");
                }
                DICOM_data.image_data.emplace_back(std::move(collated_imgs));
            }

            //Create an empty contour set iff one does not exist.
            DICOM_data.Ensure_Contour_Data_Allocated();
            //--------------------------------------------------------

            YLOGINFO("Loaded 3ddose file with dimensions " 
                     << N_x << " x " 
                     << N_y << " x " 
                     << N_z );

            //////////////////////////////////////////////////////////////

            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            YLOGINFO("Unable to load as 3ddose file");
        };

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }

    return true;
}
