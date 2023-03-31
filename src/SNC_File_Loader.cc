// SNC_File_Loader.cc - A part of DICOMautomaton 2022. Written by hal clark.
//
// This program loads ASCII SNC-formatted images files from a popular radiotherapy equipment vendor.
//

#include <string>
#include <map>
#include <vector>
#include <list>
#include <fstream>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <exception>
#include <numeric>
#include <memory>
#include <stdexcept>
#include <filesystem>
#include <cstdlib>            //Needed for exit() calls.

#include "YgorIO.h"
#include "YgorTime.h"
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorString.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"

#include "Metadata.h"
#include "Structs.h"


// Attempt to read an unwrapped cylindrical radiotherapy phantom dose image in ASCII format with 'SNC' extension.
//
// Throws if file is not a SNC file.
// Returns false if it is a SNC file, but it could not be parsed.
// Returns true if it is a SNC and could be parsed.
bool
read_snc_file( std::istream &is, planar_image_collection<float,double> &imgs ){
    constexpr bool debug = false;

    // Extract the magic header number.
    const std::string expected_magic("*Version:\t1.0.2");
    std::string magic( expected_magic.size(), '\0' );
    is.read( magic.data(), expected_magic.size() );
    if(magic != expected_magic){
        throw std::invalid_argument("Unrecognized file magic header number, not recognized as an ASCII SNC file");
    }

    // At this point, it looks like this is an SNC file. Parsing issues will now be errors.

    std::optional<std::string> missing_data_val;
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> col_positions;
    std::vector<double> row_positions;
    std::vector<std::vector<double>> pixels;
    std::optional<double> pixel_dose_scale;

    std::map<std::string, std::string> metadata;
    metadata["Modality"] = "OTHER";
    metadata["PatientID"] = "Unknown";


    std::string line;
    while(std::getline(is, line)){
        const auto ctrim = CANONICALIZE::TRIM_ENDS;
        line = Canonicalize_String2(line, ctrim);
        line = PurgeCharsFromString(line, "\r\n");


        if(line.empty()){
            continue;
        }

        auto tokens = SplitStringToVector(line, '\t', 'd');

        // Extract metadata.
        if(line.at(0UL) == '*'){
            if(tokens.size() < 2UL){
                // Metadata should be like '*key:\tvalue' or '*key\tvalue1\tvalue2\tvalue3\t...'
                YLOGWARN("Encountered unrecognized metadata format");
                return false;
            }
            
            // Trim the preceeding '*' and trailing ':' from the key, if needed.
            if(!tokens.front().empty()){
                const auto pos_end  = std::string::npos;
                const auto pos_astr = tokens.front().find_first_not_of('*');
                const auto pos_semi = tokens.front().find(':', pos_astr);
                if(pos_astr != pos_end){
                    const auto l = (pos_semi != pos_end) && (pos_astr < pos_semi) ? (pos_semi - pos_astr) : pos_end;
                    tokens.front() = tokens.front().substr(pos_astr, l);
                }
            }
            const auto key = tokens.at(0);

            // Handle the metadata.
            if(tokens.size() == 2UL){
                const auto val = tokens.at(1);
                if(debug) YLOGINFO("Storing metadata: '" << key << "' = '" << val << "'");

                if(key == "Hole Value"){
                    missing_data_val = val;

                }else if(key == "Coordinate Units"){
                    if(val != "mm"){
                        YLOGWARN("Unrecognized spatial units");
                        return false;
                    }

                }else if(key == "Dose Units"){
                    if(val == "cGy"){
                        pixel_dose_scale = 1.0/100.0;
                        metadata["DoseUnits"] = "GY";
                    }else if(val == "Gy"){
                        pixel_dose_scale = 1.0;
                        metadata["DoseUnits"] = "GY";
                    }else{
                        YLOGWARN("Unrecognized pixel dose units");
                        return false;
                    }
                    
                }else{
                    metadata[key] = val;

                }

            }else{
                if(key == "Y\\X"){
                    const auto end = std::end(tokens);
                    for(auto it = std::next(std::begin(tokens)); it != end; ++it){
                        col_positions.push_back( std::stod(*it) );
                    }
                    if(debug) YLOGINFO("Loaded Y\\X array with " << col_positions.size() << " entries");

                }else{
                    if(debug) YLOGINFO("key = '" << key << "'");
                    YLOGWARN("Encountered unknown multi-val metadata");
                    return false;
                }
            }

        // Treat the line as an image.
        }else{
            if(tokens.size() < 2UL){
                YLOGWARN("Encountered line with insufficient pixel data");
                return false;
            }
            row_positions.push_back( std::stod(tokens.at(0)) );
            pixels.emplace_back();
            const auto end = std::end(tokens);
            for(auto it = std::next(std::begin(tokens)); it != end; ++it){
                const double raw_pix_val = (missing_data_val && (*it == missing_data_val.value())) ? nan : std::stod(*it);
                pixels.back().emplace_back( raw_pix_val * pixel_dose_scale.value_or(1.0) );
            }
        }
    }

    // Validation.
    if(!pixel_dose_scale){
        YLOGWARN("Missing pixel dose units");
        return false;
    }

    // Ensure we have pixel data.
    if( pixels.empty() ){
        YLOGWARN("Missing pixel information");
        return false;
    }

    // Ensure we have pixel location information.
    if( col_positions.empty()
    ||  row_positions.empty() ){
        YLOGWARN("Missing pixel position information");
        return false;
    }

    // Note the images are defined with the top-left pixel having the maximal (positive) row number and the minimal
    // (negative) column number. We flip the row number to be consistent with other images in DICOMautomaton.

    const vec3<double> row_unit(0.0, 1.0, 0.0);
    const vec3<double> col_unit(1.0, 0.0, 0.0);
    const double pxl_dx = (row_positions.front() - row_positions.back()) / static_cast<double>(row_positions.size() - 1UL);
    const double pxl_dy = (col_positions.back() - col_positions.front()) / static_cast<double>(col_positions.size() - 1UL);
    const double pxl_dz = 1.0;
    if( (pxl_dx <= 0.0)
    ||  (pxl_dy <= 0.0)
    ||  (pxl_dz <= 0.0) ){
        YLOGWARN("Image orientation not as expected");
        return false;
    }
    if(debug) YLOGINFO("pxl_dx, dy, dz = " << pxl_dx << ", " << pxl_dy << ", " << pxl_dz);
    metadata["PixelSpacing"] = std::to_string(pxl_dx) + R"***(\)***" + std::to_string(pxl_dy);

    const vec3<double> zero3(0.0, 0.0, 0.0);
    const vec3<double> anchor = zero3;
    const vec3<double> offset( row_positions.back(), col_positions.front(), 0.0 );
    if(debug) YLOGINFO("offset = " << offset);

    const auto image_height = static_cast<long int>(row_positions.size());
    const auto image_width  = static_cast<long int>(col_positions.size());
    const auto image_chnls  = static_cast<long int>(1);
    if(!isininc(1,image_width,10'000)){
        YLOGWARN("Unexpected image width");
        return false;
    }
    if(!isininc(1,image_height,10'000)){
        YLOGWARN("Unexpected image height");
        return false;
    }

    imgs.images.emplace_back();
    imgs.images.back().init_orientation( col_unit, row_unit );
    imgs.images.back().init_spatial(pxl_dx, pxl_dy, pxl_dz, anchor, offset);
    imgs.images.back().init_buffer(image_height, image_width, image_chnls);
    imgs.images.back().metadata = metadata;

    for(long int row = 0L; row < image_height; ++row){
        for(long int col = 0L; col < image_width; ++col){
            imgs.images.back().reference(row, col, 0L) = static_cast<float>( pixels.at(image_height-row-1UL).at(col) );
        }
    }

    return true;
}

// Attempt to write an unwrapped cylindrical radiotherapy phantom dose image in ASCII format with 'SNC' extension.
//
// Returns false if an error is encountered.
bool
write_snc_file( std::ostream &os, const planar_image<float,double> &img ){
    if( (img.columns < 3L)
    ||  (img.rows < 3L)
    ||  (img.channels != 1L) ){
        YLOGWARN("Unable to write image to file: insufficient pixel data");
        return false;
    }

    const double pixel_dose_scale = 100.0; // scale from Gy to cGy.
    if("GY" != get_as<std::string>(img.metadata, "DoseUnits").value_or("")){
        YLOGWARN("Image contains unknown DoseUnits. Refusing to continue");
        return false;
    }

    // TODO: scan for NaN's and pixel min/max to create a valid hole value.

    // Magic header bytes.
    os << "*Version:\t1.0.2" << std::endl;

    // Misc metadata. 
    os << "*Dose Units:\tcGy" << std::endl;
    os << "*Dose Scalar Quantity:\t0" << std::endl;
    os << "*Coordinate Units:\tmm" << std::endl;
    os << "*Hole Value:\tNone" << std::endl;

    os << "*Y\\X";
    for(long int col = 0L; col < img.columns; ++col){
        const auto pos = img.position(0L, col);
        os << "\t" << pos.y;
    }
    os << std::endl;

    // Pixel data.
    for(long int row = 0L; row < img.rows; ++row){
        const auto flipped_pos = img.position(img.rows - row - 1L, 0L);
        os << flipped_pos.x;
        for(long int col = 0L; col < img.columns; ++col){
            const auto pxl_dose = img.value(img.rows - row - 1L, col, 0L);
            os << "\t" << (pxl_dose * pixel_dose_scale);
        }
        os << std::endl;
    }

    return !!os;
}

bool Load_From_SNC_Files( Drover &DICOM_data,
                          std::map<std::string,std::string> & /* InvocationMetadata */,
                          const std::string &,
                          std::list<std::filesystem::path> &Filenames ){

    // This routine will attempt to load SNC images on an individual file basis. Files that are not successfully loaded
    // are not consumed so that they can be passed on to the next loading stage as needed. 
    //
    // Note: This routine returns false only iff a file is suspected of being suited for this loader, but could not be
    //       loaded (e.g., the file seems appropriate, but a parsing failure was encountered).
    //
    if(Filenames.empty()) return true;

    size_t i = 0;
    const size_t N = Filenames.size();

    //DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>() );
    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        YLOGINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = *bfit;

        try{
            std::ifstream is(Filename.string(), std::ios::in | std::ios::binary);

            planar_image_collection<float,double> imgs;
            if(!read_snc_file(is, imgs)){
                return false;
            }

            // Ensure reasonable metadata is present.
            // TODO.

            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>() );
            DICOM_data.image_data.back()->imagecoll = imgs;

            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            YLOGINFO("Unable to load as SNC file: '" << e.what() << "'");
        };

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }
            
    //If nothing was loaded, do not post-process.
    const size_t N2 = Filenames.size();
    if(N == N2){
        //DICOM_data.image_data.pop_back();
        return true;
    }

    return true;
}

