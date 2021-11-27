// XIM_File_Loader.cc - A part of DICOMautomaton 2021. Written by hal clark.
//
// This program loads XIM-formatted images files from a popular linac vendor.
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
//#include "YgorStats.h"        //Needed for Median().
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

#include "Metadata.h"
#include "Structs.h"
//#include "Imebra_Shim.h"      //Needed for Collate_Image_Arrays().


planar_image<float,double> read_xim_file( std::istream &is ){
    planar_image<float,double> img;
    constexpr bool debug = false;

    const auto extract_uint8 = [](std::istream &is){
        uint8_t o;
        if(!ygor::io::read_binary<decltype(o), YgorEndianness::Little>(is, o)){
            throw std::invalid_argument("Unable to read uint8");
        }
        return o;
    };
    const auto extract_int8 = [](std::istream &is){
        int8_t o;
        if(!ygor::io::read_binary<decltype(o), YgorEndianness::Little>(is, o)){
            throw std::invalid_argument("Unable to read int8");
        }
        return o;
    };
    const auto extract_int16 = [](std::istream &is){
        int16_t o;
        if(!ygor::io::read_binary<decltype(o), YgorEndianness::Little>(is, o)){
            throw std::invalid_argument("Unable to read int16");
        }
        return o;
    };
    const auto extract_int32 = [](std::istream &is){
        int32_t o;
        if(!ygor::io::read_binary<decltype(o), YgorEndianness::Little>(is, o)){
            throw std::invalid_argument("Unable to read int32");
        }
        return o;
    };
    const auto extract_double = [](std::istream &is){
        double_t o;
        if(!ygor::io::read_binary<decltype(o), YgorEndianness::Little>(is, o)){
            throw std::invalid_argument("Unable to read double");
        }
        return o;
    };
    const auto extract_uint8_vec = [](std::istream &is, long int n){
        std::vector<uint8_t> out;
        for(long int i = 0; i < n; ++i){
            out.emplace_back();
            if(!ygor::io::read_binary<decltype(out.back()), YgorEndianness::Little>(is, out.back())){
                throw std::invalid_argument("Unable to read uint8");
            }
        }
        return out;
    };
    const auto extract_int32_vec = [](std::istream &is, long int n){
        std::vector<int32_t> out;
        for(long int i = 0; i < n; ++i){
            out.emplace_back();
            if(!ygor::io::read_binary<decltype(out.back()), YgorEndianness::Little>(is, out.back())){
                throw std::invalid_argument("Unable to read int32");
            }
        }
        return out;
    };
    const auto extract_string = [](std::istream &is, long int n){
        std::string out;
        for(long int i = 0; i < n; ++i){
            char c = '\0';
            if(!ygor::io::read_binary<decltype(c), YgorEndianness::Little>(is, c)){
                throw std::invalid_argument("Unable to read char");
            }
            out.push_back(c);
        }
        out.erase(std::remove_if(std::begin(out),
                                 std::end(out),
                                 [](unsigned char c){ return !std::isprint(c); } ),
                  std::end(out) );
        return out;
    };

    const auto magic_number = extract_string(is,8);
    if(magic_number != "VMS.XI"){
        throw std::invalid_argument("Unrecognized file magic number: '"_s + magic_number + "'");
    }
    if(debug) FUNCINFO("Format ID: '" << magic_number << "'");


    const auto format_version = extract_int32(is);
    const auto image_width = extract_int32(is);
    const auto image_height = extract_int32(is);
    const auto bits_per_pixel = extract_int32(is);
    const auto bytes_per_pixel = extract_int32(is);
    const bool decompression_reqd = (extract_int32(is) != 0);

    if(debug) FUNCINFO("format_version = " << format_version);
    if(debug) FUNCINFO("image_width = " << image_width);
    if(debug) FUNCINFO("image_height = " << image_height);
    if(debug) FUNCINFO("bits_per_pixel = " << bits_per_pixel);
    if(debug) FUNCINFO("bytes_per_pixel = " << bytes_per_pixel);
    if(debug) FUNCINFO("decompression_reqd = " << decompression_reqd);

    img.metadata["FormatVersion"]   = std::to_string(format_version);
    img.metadata["Columns"]         = std::to_string(image_width);
    img.metadata["Rows"]            = std::to_string(image_height);
    img.metadata["BitsPerPixel"]    = std::to_string(bits_per_pixel);
    img.metadata["BytesPerPixel"]   = std::to_string(bytes_per_pixel);
    //img.metadata["CompressionUsed"] = std::to_string(decompression_reqd);

    if(!isininc(1,image_width,10'000)){
        throw std::runtime_error("Unexpected image width");
    }
    if(!isininc(1,image_height,10'000)){
        throw std::runtime_error("Unexpected image height");
    }
    if( (bytes_per_pixel != 2)
    &&  (bytes_per_pixel != 4) ){
        throw std::runtime_error("Unsupported bytes per pixel ("_s + std::to_string(bytes_per_pixel) + ")");
    }
    if( bits_per_pixel != (bytes_per_pixel * 8) ){
        throw std::runtime_error("Unsupported bits per pixel ("_s + std::to_string(bits_per_pixel) + ")");
    }
    if(!decompression_reqd){
        throw std::invalid_argument("Uncompressed data encountered. This routine expects compressed data");
    }

    // Lookup table.
    const auto lut_byte_length = extract_int32(is); // in bytes.
    if((4 * lut_byte_length) != (image_width * image_height - image_width)){
        throw std::runtime_error("Unexpected LUT length ("_s + std::to_string(lut_byte_length) + ")");
    }

    // The number of bytes to read for each pixel are encoded in 2-bits. We have to read as 8-bit and unpack them.
    std::vector<uint8_t> lut_vec;
    lut_vec.reserve(4 * lut_byte_length);
    for(long int i = 0; i < lut_byte_length; ++i){
        const auto raw = extract_uint8(is);
        lut_vec.emplace_back( (raw & 0b00000011) >> 0 );
        lut_vec.emplace_back( (raw & 0b00001100) >> 2 );
        lut_vec.emplace_back( (raw & 0b00110000) >> 4 );
        lut_vec.emplace_back( (raw & 0b11000000) >> 6 );
    }
    const auto pxl_buf_size = extract_int32(is); // number of bytes holding compressed pixel data.
    long int num_bytes_read = 4 * (image_width + 1L);

    if(debug) FUNCINFO("LUT vector length = " << lut_vec.size());

    // Pixel data.
    // read the first row.
    auto pixel_data = extract_int32_vec(is, image_width + 1L);
    pixel_data.reserve(image_width * image_height);

    long int lut_num = 0;
    for(long int pix_num = image_width + 2L; pix_num <= (image_width * image_height); ++pix_num){
        const auto lut_curr = lut_vec.at(lut_num);
        int32_t diff = 0;
        if(lut_curr == 0){
            diff = static_cast<int32_t>(extract_int8(is));
            num_bytes_read += 1;
        }else if(lut_curr == 1){
            diff = static_cast<int32_t>(extract_int16(is));
            num_bytes_read += 2;
        }else{
            diff = extract_int32(is);
            num_bytes_read += 4;
        }
        if(pxl_buf_size < num_bytes_read){
            throw std::logic_error("Ran out of pixel data to read");
        }
        pixel_data.emplace_back( diff + pixel_data.at(pix_num - 2L)
                                      + pixel_data.at(pix_num - image_width - 1L)
                                      - pixel_data.at(pix_num - image_width - 2L) );
        lut_num += 1;
    }

    const auto expanded_pxl_buf_size = extract_int32(is); // number of bytes holding uncompressed pixel data.

    if(debug) FUNCINFO("pxl_buf_size = " << pxl_buf_size);
    if(debug) FUNCINFO("num_bytes_read = " << num_bytes_read);
    if( pxl_buf_size != num_bytes_read ){
        throw std::runtime_error("Number of pixels read does not match expected number of bytes present");
    }

    // Note: these don't seem to match at the end. Not off by one either, since the image garbles otherwise.
    // Not sure if it's intentional?
    if(debug) FUNCINFO("lut_num = " << lut_num);
    if(debug) FUNCINFO("lut_vec.size() = " << lut_vec.size());

    if(debug) FUNCINFO("pixel_data.size() = " << pixel_data.size());
    if(debug) FUNCINFO("image_width * image_height = " << image_width * image_height);
    if( pixel_data.size() != (image_width * image_height) ){
        throw std::runtime_error("Expanded pixel data does not match expected image dimensions");
    }

    if(debug) FUNCINFO("expanded_pxl_buf_size = " << expanded_pxl_buf_size);
    if(debug) FUNCINFO("pixel_data.size() * bytes_per_pixel = " << pixel_data.size() * bytes_per_pixel);
    if( expanded_pxl_buf_size != (pixel_data.size() * bytes_per_pixel) ){
        throw std::runtime_error("Expanded pixel data does not match expected size reported by file");
    }

    if(debug) FUNCINFO("Done reading pixel data");

    // Embedded histogram.
    const auto num_hist_bins = extract_int32(is);
    if(debug) FUNCINFO("num_hist_bins = " << num_hist_bins);

    std::vector<int32_t> hist_data;
    if(0 < num_hist_bins){
        hist_data = extract_int32_vec(is, num_hist_bins);
    }
    if(debug) FUNCINFO("Done reading histogram data");

    // Metadata.
    const auto num_metadata = extract_int32(is);
    for(auto i = 0; i < num_metadata; ++i){
        const auto key_length = extract_int32(is);
        const auto key = extract_string(is, key_length);
        const auto val_type = extract_int32(is);
        std::string val;

        // Scalars.
        if(val_type == 0){
            val = std::to_string( extract_int32(is) );

        }else if(val_type == 1){
            val = std::to_string( extract_double(is) );

        // Arrays.
        }else if(val_type == 2){
            const auto val_length = extract_int32(is); // in bytes.
            val = extract_string(is, val_length);

        }else if(val_type == 4){
            const auto val_length = extract_int32(is); // in bytes.
            if( (val_length % 8) != 0){
                throw std::runtime_error("unexpected byte length for 'double' encoded metadata value array"); 
            }
            for(long int j = 0; (j * 8) < val_length; ++j){
                if(!val.empty()) val += ", ";
                val += std::to_string( extract_double(is) );
            }

        }else if(val_type == 5){
            const auto val_length = extract_int32(is); // in bytes.
            if( (val_length % 4) != 0){
                throw std::runtime_error("unexpected byte length for 'int32' encoded metadata value array"); 
            }
            for(long int j = 0; (j * 4) < val_length; ++j){
                if(!val.empty()) val += ", ";
                val += std::to_string( extract_int32(is) );
            }
            
        }else{
            throw std::invalid_argument("Unsupported and unknown metadata key-value encoding");
        }

        img.metadata[key] = val;
        if(debug) FUNCINFO("Read metadata key-value pair: '" << key << "' -- '" << val << "'");
    }

    // Inject DICOM-style metadata for consistency with the same images exported in DICOM format.
    // Note that this might not be valid in all cases! This was pieced together with samples from
    // 'AcquisitionSystemVersion' = '2.7.304.16' circa 2021-11-26.
    img.metadata["Modality"] = "RTIMAGE";

    if(time_mark t; t.Read_from_string( get_as<std::string>(img.metadata, "StartTime").value_or("1900-01-01") ) ){
        const auto datetime = t.Dump_as_postgres_string(); // "2013-11-30 13:05:35"
        const auto date_only = datetime.substr(0,10);
        //const auto time_only = datetime.substr(12);
        img.metadata["AcquisitionDate"] = date_only;
    }
    const auto is_MV = (get_as<long int>(img.metadata, "MVBeamOn").value_or(0) != 0);
    const auto is_kV = (get_as<long int>(img.metadata, "KVBeamOn").value_or(0) != 0);
    if( is_MV == is_kV ){
        throw std::logic_error("This implementation assumes either (exclusive) MV or kV imaging only");
    }

    // A placeholder Patient ID seems to be added only when exporting as DICOM.
    img.metadata["PatientID"] = "Unknown";
    const vec3<double> zero3(0.0, 0.0, 0.0);
    vec3<double> offset = zero3;
    vec3<double> anchor = zero3;
    if(is_MV){
        img.metadata["RTImagePlane"] = "NORMAL";

        // Note that these tags seem to be in cm rather than mm, which is why we scale 10x.
        // The MVSourceVrt should always be 1000 mm. The MVDetectorVrt seems to range from -82 to +20 or so, which is
        // why we need to subtract it from the SAD to get SID.
        const auto SAD = get_as<double>(img.metadata, "MVSourceVrt").value_or(100.0) * 10.0;
        const auto SID = SAD - get_as<double>(img.metadata, "MVDetectorVrt").value_or(0.0) * 10.0; 

        img.metadata["RadiationMachineSAD"] = std::to_string( SAD );
        img.metadata["RTImageSID"] = std::to_string( SID );

        // I'm not sure how to work this out at the moment. I think it depends on the detector panel dimensions, which I
        // don't think are included in the XIM metadata. For now I'll rely on a DICOM <--> XIM match and hope it works.
        //
        // Also note that pitch and rotation are completely ignored here.
        //
        // FIXME TODO
        const auto lat = get_as<double>(img.metadata, "MVDetectorLat").value_or(0.0) * 10.0;
        const auto lng = get_as<double>(img.metadata, "MVDetectorLng").value_or(0.0) * 10.0;
        offset = vec3<double>( (-214.872 + lat) * (1499.94787 / SID),
                               ( 214.872 + lng) * (1499.94787 / SID),
                               ( SAD - SID ) ); // For consistency with XRayImageReceptorTranslation z-coord.

        // The following is a placeholder. It doesn't appear in my DICOM sample.
        // (Does it correspond with the image anchor?)
        img.metadata["IsocenterPosition"] = "0\\0\\0";
        anchor = zero3;
    }

    const auto pxl_dy = get_as<double>(img.metadata, "PixelHeight").value_or(0.1000) * 10.0; // Seems to be in cm.
    const auto pxl_dx = get_as<double>(img.metadata, "PixelWidth").value_or(0.1000) * 10.0; // Seems to be in cm.
    img.metadata["PixelSpacing"] = std::to_string(pxl_dx) + R"***(\)***" + std::to_string(pxl_dy);

    const vec3<double> row_unit(1,0,0);
    const vec3<double> col_unit(0,-1,0);
    img.init_orientation( col_unit, row_unit );
    img.init_spatial(pxl_dx, pxl_dy, 1.0, anchor, offset);
    img.init_buffer(image_height, image_width, 1);

    for(long int row = 0; row < image_height; ++row){
        for(long int col = 0; col < image_width; ++col){
            img.reference(row, col, 0) = static_cast<float>( pixel_data.at(row * image_width + col ) );
        }
    }

    // // Write the image to disk as a FITS file for easier viewing.
    // if(!WriteToFITS<float,double>(img, "/tmp/test.fits")){
    //     throw std::runtime_error("Unable to write image to disk as FITS file");
    // }

    return img;
}

bool Load_From_XIM_Files( Drover &DICOM_data,
                          const std::map<std::string,std::string> & /* InvocationMetadata */,
                          const std::string &,
                          std::list<std::filesystem::path> &Filenames ){

    // This routine will attempt to load XIM images on an individual file basis. Files that are not successfully loaded
    // are not consumed so that they can be passed on to the next loading stage as needed. 
    //
    // Note: This routine returns false only iff a file is suspected of being suited for this loader, but could not be
    //       loaded (e.g., the file seems appropriate, but a parsing failure was encountered).
    //
    if(Filenames.empty()) return true;

    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>() );

    size_t i = 0;
    const size_t N = Filenames.size();

    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        FUNCINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = *bfit;

        try{
            std::ifstream is(Filename.string(), std::ios::in | std::ios::binary);
            auto animg = read_xim_file(is);

            // Ensure a minimal amount of metadata is present for image purposes.
            auto l_meta = coalesce_metadata_for_basic_image(animg.metadata);
            l_meta.merge(animg.metadata);
            animg.metadata = l_meta;
            animg.metadata["Filename"] = Filename.string();

            FUNCINFO("Loaded XIM file with dimensions " 
                     << animg.rows << " x " << animg.columns);

            DICOM_data.image_data.back()->imagecoll.images.emplace_back( animg );
            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            FUNCINFO("Unable to load as XIM file: '" << e.what() << "'");
        };

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }
            
    //If nothing was loaded, do not post-process.
    const size_t N2 = Filenames.size();
    if(N == N2){
        DICOM_data.image_data.pop_back();
        return true;
    }

    return true;
}

