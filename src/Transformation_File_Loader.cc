//Transformation_File_Loader.cc - A part of DICOMautomaton 2021. Written by hal clark.
//
// This program saves and loads R^3 spatial transformations to/from files.
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
#include <filesystem>
#include <cstdlib>            //Needed for exit() calls.
#include <variant>

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorString.h"       //Needed for SplitStringToVector, Canonicalize_String2, SplitVector functions.
#include "YgorMathIOOBJ.h"

#include "Structs.h"
#include "Alignment_Rigid.h"
#include "Alignment_TPSRPM.h"
#include "Alignment_Field.h"

// Read the transformation from a custom file format.
bool
ReadTransform3(Transform3 &t3,
               std::istream &is ){

    if(!is.good()){
        throw std::runtime_error("Unable to read file.");
    }

    const auto reset = [&](){
        t3 = Transform3();
    };
    reset();

    std::string magic;
    std::string variant;

    std::string aline;
    while(!is.eof()){
        std::getline(is, aline);
        if(is.eof()) break;

        // Strip away comments and handle embedded metadata.
        const auto com_pos = aline.find_first_of("#");
        if(com_pos != std::string::npos){
            // Handle metadata packed into a comment line.
            auto kvp_opt = decode_metadata_kv_pair(aline);
            if(kvp_opt) t3.metadata.insert(kvp_opt.value());

            continue;
        }

        // Handle empty lines.
        aline = Canonicalize_String2(aline, CANONICALIZE::TRIM);
        if(aline.empty()) continue;

        // The first non-comment line should hold the 'magic' phrase. Confirm it does.
        if(magic.empty()){
            magic = aline;
            if(magic != "DCMA_TRANSFORM"){
                reset();
                return false;
            }
            continue;
        }

        // The second non-comment line should hold the transform variant's name.
        // Confirm it is valid and known.
        //
        // Note that the actual transform should immediately follow.
        if(variant.empty()){
            variant = aline;
            break;
        }
    }

    if(variant == "TRANSFORM_VARIANT_DISENGAGED"){
        // This is an empty transformation. However, it might contain useful metadata.
        return !t3.metadata.empty();

    }else if(variant == "TRANSFORM_VARIANT_AFFINE_3"){
        t3.transform.emplace< affine_transform<double> >(is);
        return true;

    }else if(variant == "TRANSFORM_VARIANT_THIN_PLATE_SPLINE_3"){
        t3.transform.emplace< thin_plate_spline >(is);
        return true;

    }else if(variant == "TRANSFORM_VARIANT_DEFORMATION_FIELD_3"){
        t3.transform.emplace< deformation_field >(is);
        return true;

    }else{
        YLOGWARN("Transform variant not understood");
    }
    reset();
    return false;
}


// Write the transformation to a custom file format.
bool
WriteTransform3(const Transform3 &t3,
                std::ostream &os ){

    os << "DCMA_TRANSFORM" << std::endl;

    for(const auto &mp : t3.metadata){
        os << "# " << encode_metadata_kv_pair(mp) << std::endl;
    }

//    // Maximize precision prior to emitting the vertices.
//    const auto original_precision = os.precision();
//    os.precision( std::numeric_limits<double>::max_digits10 );

    std::visit([&](auto && t){
        using V = std::decay_t<decltype(t)>;
        if constexpr (std::is_same_v<V, std::monostate>){
            os << "TRANSFORM_VARIANT_DISENGAGED" << std::endl;

        // Affine transformations.
        }else if constexpr (std::is_same_v<V, affine_transform<double>>){
            YLOGINFO("Exporting affine transformation now");
            os << "TRANSFORM_VARIANT_AFFINE_3" << std::endl;
            if(!(t.write_to(os))){
                std::runtime_error("Unable to write to file. Cannot continue.");
            }

        // Thin-plate spline transformations.
        }else if constexpr (std::is_same_v<V, thin_plate_spline>){
            YLOGINFO("Exporting thin-plate spline transformation now");
            os << "TRANSFORM_VARIANT_THIN_PLATE_SPLINE_3" << std::endl;
            if(!(t.write_to(os))){
                std::runtime_error("Unable to write to file. Cannot continue.");
            }

        // Vector deformation fields.
        }else if constexpr (std::is_same_v<V, deformation_field>){
            YLOGINFO("Exporting vector deformation field now");
            os << "TRANSFORM_VARIANT_DEFORMATION_FIELD_3" << std::endl;
            if(!(t.write_to(os))){
                std::runtime_error("Unable to write to file. Cannot continue.");
            }

        }else{
            static_assert(std::is_same_v<V,void>, "Transformation not understood.");
        }
        return;
    }, t3.transform);

//    // Reset the precision on the stream.
//    os.precision( original_precision );
//    os.flush();

    return(!os.bad());
}


bool Load_Transforms_From_Files( Drover &DICOM_data,
                                 std::map<std::string,std::string> & /* InvocationMetadata */,
                                 const std::string &,
                                 std::list<std::filesystem::path> &Filenames ){

    //This routine will attempt to load transforms.
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

        DICOM_data.trans_data.emplace_back( std::make_shared<Transform3>() );
        try{
            //////////////////////////////////////////////////////////////
            // Attempt to load the file.
            std::ifstream FI(Filename, std::ios::in);
            const bool read_ok = ReadTransform3(*(DICOM_data.trans_data.back()), FI);

            if(!read_ok){
                throw std::runtime_error("Unable to read transform from file.");
            }
            //////////////////////////////////////////////////////////////

            YLOGINFO("Loaded transform");
            bfit = Filenames.erase( bfit ); 
            continue;

        }catch(const std::exception &e){
            YLOGINFO("Unable to load as transform file");
            DICOM_data.trans_data.pop_back();
        };

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }

    return true;
}
