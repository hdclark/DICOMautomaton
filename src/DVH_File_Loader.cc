//DVH_File_Loader.cc - A part of DICOMautomaton 2020. Written by hal clark.
//
// This program loads line sample data from 'tabular DVH format' files as exported by a major linac vendor.
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

#include <boost/filesystem.hpp>
#include <cstdlib>            //Needed for exit() calls.

#include "Explicator.h"       //Needed for Explicator class.

#include "Structs.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for SplitStringToVector, Canonicalize_String2, SplitVector functions.

void Consume_BOM(std::istream &is){
   // Consume the first three bytes in the stream IFF they are: 0xEF, 0xBB, 0xBF (i.e., utf-8).
   //
   // Note: The IETF states that an implementation "SHOULD forbid the use of U+FEFF as a signature."
   //       Also note that utf-8 can not have endianness. Nevertheless, it can be encountered in practice.
   const auto initial_pos = is.tellg();

   // Consume the first three bytes prospectively.
   std::array<char, 4> bytes;
   for(size_t i = 0; i < 3; ++i){
       if(!is.get(bytes[i])){
           // Too short to be a BOM, so reset the stream and return.
           is.clear();
           is.seekg( initial_pos );
           return;
       }
   }

   // If a utf-8 BOM is found, consume it.
   if( (bytes[0] == static_cast<char>(0xEF))
   &&  (bytes[1] == static_cast<char>(0xBB))
   &&  (bytes[2] == static_cast<char>(0xBF)) ){
       return;
   }

   // Check other BOMs just in case. If they match, complain since we can only handle utf-8.
   if( (bytes[0] == static_cast<char>(0xFE))
   &&  (bytes[1] == static_cast<char>(0xFF)) ){
       throw std::invalid_argument("Detected utf-16 big-endian BOM. Only utf-8 is supported.");
   }
   if( (bytes[0] == static_cast<char>(0xFF))
   &&  (bytes[1] == static_cast<char>(0xFE)) ){
       throw std::invalid_argument("Detected utf-16 little-endian BOM. Only utf-8 is supported.");
   }

   // Check for 4-byte BOMs, iff a fourth byte can be read.
   const bool has_fourth = static_cast<bool>(is) && (is.get(bytes[3]));
   if( has_fourth
   &&  (bytes[0] == static_cast<char>(0x00))
   &&  (bytes[1] == static_cast<char>(0x00))
   &&  (bytes[2] == static_cast<char>(0xFE))
   &&  (bytes[3] == static_cast<char>(0xFF)) ){
       throw std::invalid_argument("Detected utf-32 big-endian BOM. Only utf-8 is supported.");
   }
   if( has_fourth
   &&  (bytes[0] == static_cast<char>(0xFF))
   &&  (bytes[1] == static_cast<char>(0xFE))
   &&  (bytes[2] == static_cast<char>(0x00))
   &&  (bytes[3] == static_cast<char>(0x00)) ){
       throw std::invalid_argument("Detected utf-32 little-endian BOM. Only utf-8 is supported.");
   }

   // No discernable BOM was found, so reset the stream before returning.
   is.clear();
   is.seekg( initial_pos );
   return;
}

std::map<std::string, std::string> Read_Header_Block(std::istream &is,
                                                     Explicator &X,
                                                     std::map<std::string, std::string> metadata){
    // Parses a metadata block, reading a block of lines until a whitespace-only line is encountered.
    // The provided metadata will be combined with (and overwritten by) the locally parsed metadata.
    //
    // This block comprises only metadata and consists of 'key : values' in which the value might be multi-lined.
    // Multi-line values are assumed to NOT contain a ':' character. Metadata blocks are NOT indented.
    //
    std::map<std::string, std::string> l_metadata;

    std::string previous_key; // Used to handle multi-line metadata.
    std::string l; // Line iterator.
    while(!is.eof()){
        std::getline(is, l);
        if(is.eof()) break;

        // Strip away any extra '\r' characters at the end.
        if( !l.empty() && (l.back() == '\r') ) l = l.substr(0, l.size() - 1);

        // Handle empty lines, which indicate the end of the block.
        const auto trimmed_l = Canonicalize_String2(l, CANONICALIZE::TRIM_ENDS);
        if(trimmed_l.empty()) break;

        // Determine if this line is a continuation.
        // If the first character is NOT whitespace AND there is a ':', it is a new key.
        // If the first character is whitespace, it is a continuation.
        const auto first_not_whitespace = l.find_first_not_of(" ");
        const auto first_semicolon = l.find_first_of(":");
        if(false){
        }else if( (first_not_whitespace == 0 ) 
              &&  (first_semicolon != std::string::npos) ){
            // Not a continuation.
            const auto key = Canonicalize_String2( l.substr(0, first_semicolon), CANONICALIZE::TRIM_ENDS);
            const auto val = Canonicalize_String2( l.substr(first_semicolon+2) , CANONICALIZE::TRIM_ENDS);
            if(!key.empty() && !val.empty()){
                //FUNCINFO("Parsed key-value = '" << key << "' : '" << val << "'");
                l_metadata[key] = val; // This intentionally overwrites existing key-values.
                previous_key = key;
            }

        }else if( (first_not_whitespace != 0 ) 
              &&  (first_semicolon == std::string::npos) ){
            // Is a continuation.
            const auto val = Canonicalize_String2( l, CANONICALIZE::TRIM_ENDS);
            if(l_metadata.count(previous_key) == 0){
                throw std::invalid_argument("Encountered value continuation without a valid key.");
            }
            if(!val.empty()){
                l_metadata[previous_key] += " "_s + val; // This intentionally overwrites existing key-values.
                //FUNCINFO("Parsed value continuation = '" << previous_key << "' : '" << l_metadata[previous_key] << "'");
            }

        }else{
            // Something isn't right. Maybe a multi-line value that contains a ':' ?
            //FUNCINFO("Ignoring line: '" << l << "'");            
            throw std::runtime_error("Key-value structure not understood.");
        }

    }

    // Replace raw l_metadata keys with DCMA-consistent keys and prioritize l_metadata over user-provided metadata.
    for(const auto &lkv : l_metadata){
        const auto key = lkv.first;
        const auto val = lkv.second;

        // Top-level header block (patient-wide).
        if(false){
        }else if(key == "Patient Name"){        metadata["PatientsName"] = val;
        }else if(key == "Patient ID"){          metadata["PatientID"] = val;
        }else if(key == "Comment"){             metadata["TabularDVHComment"] = val;
        }else if(key == "Date"){                metadata["InstanceCreationDateTime"] = val;
        }else if(key == "Exported by"){         metadata["OperatorsName"] = val;
        }else if(key == "Type"){
            metadata["Modality"] = "Histogram";
            //metadata["AbscissaScaling"] = "None";
            //metadata["OrdinateScaling"] = "None";

            if(false){
            }else if(val == "Cumulative Dose Volume Histogram"){
                metadata["HistogramType"] = "Cumulative";
            }else if(val == "Differential Dose Volume Histogram"){
                metadata["HistogramType"] = "Differential";
            }else{
                throw std::runtime_error("Histogram type not recognized.");
            }
        }else if(key == "Description"){         metadata["TabularDVHDescription"] = val;

        // Plan-level header block.
        }else if(key == "Plan"){                metadata["RTPlanLabel"] = val;
                                                metadata["RTPlanName"] = val;
        }else if(key == "Course"){              metadata["StudyID"] = val;
        }else if(key == "Plan Status"){         metadata["RTPlanApprovalStatus"] = val;
        }else if(key == "Total dose [cGy]"){
            const auto D = std::to_string( std::stod(val) / 100.0 ); // Convert from cGy to Gy.
            metadata["TabularDVHReferenceDose"] = D;

        }else if(key == R"***(% for dose (%))***"){
            const auto N = std::to_string( std::stod(val) / 100.0 ); // Convert from % to decimal.
            metadata["TabularDVHReferenceDoseNormalization"] = N;

        // Uncertainty plan header block.
        }else if(key == "Uncertainty plan"){    metadata["RTPlanLabel"] = val;
                                                metadata["RTPlanName"] = val;

        // Structure-level header block.
        }else if(key == "Structure"){
            metadata["LineName"] = val;
            metadata["ROIName"] = val;
            metadata["NormalizedROIName"] = X(val);

        }else if(key == "Volume [cm³]"){
            metadata["ROIVolume"] = std::to_string( std::stod(val) * 1000.0 ); // Convert from cm^3 to mm^3.

        }else if(key == "Min Dose [cGy]"){
            const auto D = std::to_string( std::stod(val) / 100.0 ); // Convert from cGy to Gy.
            metadata["DistributionMin"] = D;

        }else if(key == "Mean Dose [cGy]"){
            const auto D = std::to_string( std::stod(val) / 100.0 ); // Convert from cGy to Gy.
            metadata["DistributionMean"] = D;

        }else if(key == "Max Dose [cGy]"){
            const auto D = std::to_string( std::stod(val) / 100.0 ); // Convert from cGy to Gy.
            metadata["DistributionMax"] = D;

        }else if(key == "Min Dose [%]"){
            const auto D_ref = std::stod( metadata.at("TabularDVHReferenceDose") ); // Assume this is already available.
            const auto D = std::to_string( D_ref * std::stod(val) / 100.0 ); // Convert from % to Gy.
            metadata["DistributionMin"] = D;

        }else if(key == "Mean Dose [%]"){
            const auto D_ref = std::stod( metadata.at("TabularDVHReferenceDose") ); // Assume this is already available.
            const auto D = std::to_string( D_ref * std::stod(val) / 100.0 ); // Convert from % to Gy.
            metadata["DistributionMean"] = D;

        }else if(key == "Max Dose [%]"){
            const auto D_ref = std::stod( metadata.at("TabularDVHReferenceDose") ); // Assume this is already available.
            const auto D = std::to_string( D_ref * std::stod(val) / 100.0 ); // Convert from % to Gy.
            metadata["DistributionMax"] = D;
        }
    }

    return metadata;
}


samples_1D<double> Read_Histogram(std::istream &is,
                                  std::map<std::string, std::string> metadata){

    // Read the first line of the table to extract column and layout information.
    enum class vol_units_t { 
        unknown,
        cm3,
        pcnt } vol_units = vol_units_t::unknown;
    size_t i_D_abs = std::numeric_limits<size_t>::max();
    size_t i_V     = std::numeric_limits<size_t>::max();

    {
        std::string l;
        std::getline(is, l);

        // Strip away any extra '\r' characters at the end.
        if( !l.empty() && (l.back() == '\r') ) l = l.substr(0, l.size() - 1);

        const auto trimmed_l = Canonicalize_String2(l, CANONICALIZE::TRIM);
        if(!is.good() || trimmed_l.empty()){
            throw std::invalid_argument("Unable to find histogram block.");
        }

        std::vector<std::string> tokens = { trimmed_l };
        tokens = SplitVector(tokens, ']', 'l');
        if(tokens.size() != 3){
            throw std::invalid_argument("Unexpected number of columns in histogram block.");
        }

        for(size_t i = 0; i < tokens.size(); ++i){
            tokens[i] = Canonicalize_String2(tokens[i], CANONICALIZE::TRIM);
            if(false){
            }else if(tokens[i] == "Dose [cGy]"){
                i_D_abs = i;
            }else if(tokens[i] == "Ratio of Total Structure Volume [%]"){
                i_V = i;
                vol_units = vol_units_t::pcnt;
            }else if(tokens[i] == "Structure Volume [cm³]"){
                i_V = i;
                vol_units = vol_units_t::cm3;
            }else if(tokens[i] == "Relative dose [%]"){
                // Do nothing.
            }else{
                //FUNCINFO("Offending column title: '" << tokens[i] << "'");
                throw std::runtime_error("Column name not recognized.");
            }
        }
        if(tokens.size() <= i_D_abs) throw std::invalid_argument("Unable to identfy absolute dose column.");
        if(tokens.size() <= i_V)     throw std::invalid_argument("Unable to identfy volume column.");
        if(vol_units == vol_units_t::unknown) throw std::invalid_argument("Volume column units not understood.");
    }

    const auto D_scale = 1.0 / 100.0; // Convert from cGy to Gy.

    const auto V_scale_abs = 1000.0; // Convert from cm^3 to mm^3.
    const auto V_ref = std::stod( metadata.at("ROIVolume") ); // in mm^3
    const auto V_scale_rel = V_ref / 100.0; // Convert from % to mm^3.
    const auto V_scale = (vol_units == vol_units_t::cm3) ? V_scale_abs : V_scale_rel;

    // Read the table, transforming the columns as needed.
    samples_1D<double> out;
    bool inhibit_sort = true;

    std::string l; // Line iterator.
    while(!is.eof()){
        std::getline(is, l);
        if(is.eof()) break;

        // Strip away any extra '\r' characters at the end.
        if( !l.empty() && (l.back() == '\r') ) l = l.substr(0, l.size() - 1);

        // Handle empty lines, which indicate the end of the block.
        const auto trimmed_l = Canonicalize_String2(l, CANONICALIZE::TRIM);
        if(trimmed_l.empty()) break;

        std::vector<std::string> tokens = { trimmed_l };
        tokens = SplitVector(tokens, ' ', 'd');
        if(tokens.size() != 3){
            throw std::invalid_argument("Unexpected number of columns in histogram block.");
        }

        const double D = std::stod( tokens[i_D_abs] ) * D_scale;
        const double V = std::stod( tokens[i_V] ) * V_scale;
        out.push_back(D, V, inhibit_sort);
    }

    if(out.samples.empty()){
        throw std::invalid_argument("Histogram contained no data.");
    }
    return out;
}


bool Load_From_DVH_Files( Drover &DICOM_data,
                          std::map<std::string,std::string> & /* InvocationMetadata */,
                          std::string &FilenameLex,
                          std::list<boost::filesystem::path> &Filenames ){

    //This routine will attempt to load DVH-format files. Files that are not successfully loaded
    // are not consumed so that they can be passed on to the next loading stage as needed. 
    //
    // Note: This routine returns false only iff a file is suspected of being suited for this loader, but could not be
    //       loaded (e.g., the file seems appropriate, but a parsing failure was encountered).
    //
    if(Filenames.empty()) return true;

    Explicator X(FilenameLex);

    size_t i = 0;
    const size_t N = Filenames.size();

    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        FUNCINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = bfit->string();

        try{
            std::list<std::shared_ptr<Line_Sample>> lsamp_data;

            //////////////////////////////////////////////////////////////
            // Attempt to load the file.
            std::ifstream FI(Filename.c_str(), std::ios::in);

            if(!FI.good()) throw std::runtime_error("Unable to read file.");

            // Consume the initial header metadata, which consists of (possibly) a BOM and two separate blocks.
            Consume_BOM(FI);
            auto top_level_metadata  = Read_Header_Block(FI, X, {});
            auto plan_level_metadata = Read_Header_Block(FI, X, top_level_metadata);

            // Consume the structure header and histogram table.
            //
            // Note that each header should overwrite the individual elements that have changed.
            // Normally each ROI transition alters nearly everything, but 'plan uncertainty' variants change very little.
            auto roi_level_metadata = plan_level_metadata;
            while(!FI.eof() && FI.good()){
                roi_level_metadata = Read_Header_Block(FI, X, roi_level_metadata);
                samples_1D<double> histogram = Read_Histogram(FI, roi_level_metadata);
                histogram.stable_sort();

                FUNCINFO("Loaded histogram with " << histogram.samples.size() << " samples");

                lsamp_data.emplace_back( std::make_shared<Line_Sample>() );
                lsamp_data.back()->line.samples.swap( histogram.samples );
                lsamp_data.back()->line.metadata = roi_level_metadata;

                // Purge unneeded samples.
                const auto x_eps = std::numeric_limits<double>::infinity();  // Ignore the abscissa.
                const auto y_eps = std::sqrt( 10.0 * std::numeric_limits<double>::epsilon() );
                lsamp_data.back()->line = lsamp_data.back()->line.Purge_Redundant_Samples(x_eps, y_eps); 

                // Check if there is any more data.
                FI.peek();
                if(!FI) break;
            }

            //////////////////////////////////////////////////////////////
            // Append the loaded DVH data .
            DICOM_data.lsamp_data.splice( std::end(DICOM_data.lsamp_data), lsamp_data);
            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            FUNCINFO("Unable to load as tabular DVH line sample file: " << e.what());
        };

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }

    return true;
}

