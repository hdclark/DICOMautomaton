//DICOMautomaton_Deidentify.cc -- A DICOM de-identification tool.
//
// This program de-identifies DICOM files following DICOM Supplement 142
// "Clinical Trial De-Identification". It reads input DICOM files, applies
// de-identification (erasing PHI tags, replacing names/dates, and consistently
// remapping UIDs), and writes the result to an output directory.
//
// UID mappings are persisted to a file so that consistent mappings are
// maintained across invocations.
//

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>

#include "YgorArguments.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include "DCMA_DICOM.h"

namespace fs = std::filesystem;

// Load a UID mapping from a text file. Format: one mapping per line as "old_uid new_uid".
static DCMA_DICOM::uid_mapping_t load_uid_mapping(const std::string &path){
    DCMA_DICOM::uid_mapping_t mapping;
    std::ifstream ifs(path);
    if(!ifs.good()) return mapping;

    std::string line;
    while(std::getline(ifs, line)){
        // Skip blank lines and comments.
        if(line.empty() || line.front() == '#') continue;
        std::istringstream ss(line);
        std::string old_uid, new_uid;
        if(ss >> old_uid >> new_uid){
            mapping[old_uid] = new_uid;
        }
    }
    return mapping;
}

// Save a UID mapping to a text file.
static void save_uid_mapping(const std::string &path, const DCMA_DICOM::uid_mapping_t &mapping){
    std::ofstream ofs(path, std::ios::trunc);
    if(!ofs.good()){
        YLOGWARN("Unable to write UID mapping file '" << path << "'");
        return;
    }
    ofs << "# DCMA DICOM UID mapping file. Format: old_uid new_uid\n";
    for(const auto &[old_uid, new_uid] : mapping){
        ofs << old_uid << " " << new_uid << "\n";
    }
}

// Collect input files from a list that may include both files and directories.
static std::vector<fs::path> collect_input_files(const std::vector<std::string> &inputs){
    std::vector<fs::path> files;
    for(const auto &input : inputs){
        const fs::path p(input);
        if(fs::is_directory(p)){
            for(const auto &entry : fs::directory_iterator(p)){
                if(entry.is_regular_file()){
                    files.push_back(entry.path());
                }
            }
        }else if(fs::is_regular_file(p)){
            files.push_back(p);
        }else{
            YLOGWARN("Ignoring non-file, non-directory input: '" << input << "'");
        }
    }
    return files;
}


int main(int argc, char **argv){
    std::string output_dir;
    std::string patient_id;
    std::string patient_name;
    std::string study_id;
    std::string uid_map_file;
    DCMA_DICOM::DeidentifyParams params;

    std::vector<std::string> inputs;

    class ArgumentHandler arger;
    arger.examples = { { "-p 'ANON001' -n 'Anonymous' -s 'STUDY01' -o /tmp/output /tmp/input/",
                         "De-identify all files in /tmp/input/ and write to /tmp/output/." },
                       { "-p 'ANON001' -n 'Anonymous' -s 'STUDY01' -m /tmp/uid_map.txt"
                         " -o /tmp/output file1.dcm file2.dcm",
                         "De-identify specific files, persisting UID mappings for cross-invocation consistency." }
                     };
    arger.description = "A DICOM de-identification tool following DICOM Supplement 142.";

    arger.default_callback = [](int, const std::string &optarg) -> void {
        YLOGERR("Unrecognized option with argument: '" << optarg << "'");
    };
    arger.optionless_callback = [&](const std::string &optarg) -> void {
        inputs.emplace_back(optarg);
    };

    arger.push_back( ygor_arg_handlr_t(0, 'o', "output-dir", true, "/tmp/output",
      "Output directory for de-identified files.",
      [&](const std::string &optarg) -> void {
        output_dir = optarg;
      })
    );

    arger.push_back( ygor_arg_handlr_t(1, 'p', "patient-id", true, "ANON001",
      "New patient ID (required).",
      [&](const std::string &optarg) -> void {
        patient_id = optarg;
      })
    );

    arger.push_back( ygor_arg_handlr_t(1, 'n', "patient-name", true, "Anonymous",
      "New patient name (required).",
      [&](const std::string &optarg) -> void {
        patient_name = optarg;
      })
    );

    arger.push_back( ygor_arg_handlr_t(1, 's', "study-id", true, "STUDY01",
      "New study ID (required).",
      [&](const std::string &optarg) -> void {
        study_id = optarg;
      })
    );

    arger.push_back( ygor_arg_handlr_t(2, 't', "study-description", true, "<optional>",
      "New study description (if omitted, tag is erased).",
      [&](const std::string &optarg) -> void {
        params.study_description = optarg;
      })
    );

    arger.push_back( ygor_arg_handlr_t(2, 'e', "series-description", true, "<optional>",
      "New series description (if omitted, tag is erased).",
      [&](const std::string &optarg) -> void {
        params.series_description = optarg;
      })
    );

    arger.push_back( ygor_arg_handlr_t(3, 'm', "uid-map", true, "/tmp/uid_map.txt",
      "UID mapping file (loaded and saved for cross-invocation consistency).",
      [&](const std::string &optarg) -> void {
        uid_map_file = optarg;
      })
    );

    arger.Launch(argc, argv);

    // Validate required parameters.
    if(patient_id.empty()){
        YLOGERR("Patient ID (-p) is required");
        return 1;
    }
    if(patient_name.empty()){
        YLOGERR("Patient name (-n) is required");
        return 1;
    }
    if(study_id.empty()){
        YLOGERR("Study ID (-s) is required");
        return 1;
    }
    if(output_dir.empty()){
        YLOGERR("Output directory (-o) is required");
        return 1;
    }
    if(inputs.empty()){
        YLOGERR("No input files or directories specified");
        return 1;
    }

    params.patient_id = patient_id;
    params.patient_name = patient_name;
    params.study_id = study_id;

    // Create the output directory if it does not exist.
    try{
        fs::create_directories(output_dir);
    }catch(const std::exception &e){
        YLOGERR("Error creating output directory: " << e.what());
        return 1;
    }

    // Load UID mapping if a mapping file was specified.
    DCMA_DICOM::uid_mapping_t uid_map;
    if(!uid_map_file.empty() && fs::exists(uid_map_file)){
        uid_map = load_uid_mapping(uid_map_file);
        YLOGINFO(uid_map.size() << " UID mappings loaded from '" << uid_map_file << "'");
    }

    // Collect all input files.
    const auto input_files = collect_input_files(inputs);
    if(input_files.empty()){
        YLOGERR("No input files found");
        return 1;
    }

    YLOGINFO("Processing " << input_files.size() << " input file(s)");

    const auto &default_dict = DCMA_DICOM::get_default_dictionary();
    std::vector<const DCMA_DICOM::DICOMDictionary*> dicts = { &default_dict };
    DCMA_DICOM::DICOMDictionary mutable_dict;

    int64_t file_count = 0;
    int64_t success_count = 0;
    int64_t fail_count = 0;

    for(const auto &input_path : input_files){
        ++file_count;

        try{
            // Read the DICOM file.
            std::ifstream ifs(input_path, std::ios::binary);
            if(!ifs.good()){
                YLOGWARN("Unable to read file '" << input_path << "', skipping");
                ++fail_count;
                continue;
            }

            DCMA_DICOM::Node root;
            root.read_DICOM(ifs, dicts, &mutable_dict);
            ifs.close();

            // Determine the encoding from the TransferSyntaxUID.
            DCMA_DICOM::Encoding enc = DCMA_DICOM::Encoding::ELE; // Default.
            const auto *ts_node = root.find(0x0002, 0x0010);
            if(ts_node != nullptr){
                // Strip trailing NUL and space padding from the UID value.
                std::string ts_uid = ts_node->val;
                while(!ts_uid.empty() && (ts_uid.back() == '\0' || ts_uid.back() == ' ')) ts_uid.pop_back();
                if(ts_uid == "1.2.840.10008.1.2"){
                    enc = DCMA_DICOM::Encoding::ILE; // Implicit VR Little Endian.
                }else{
                    enc = DCMA_DICOM::Encoding::ELE; // Explicit VR Little Endian (and variants).
                }
            }

            // Apply de-identification.
            DCMA_DICOM::deidentify(root, params, uid_map);

            // Generate a unique output filename that does not collide with existing files.
            // Use a sequential counter with .dcm extension for de-identified filenames.
            fs::path output_path;
            std::string fname_str;
            for(int64_t attempt = file_count; ; ++attempt){
                std::ostringstream fname_ss;
                fname_ss << "anon_" << std::setfill('0') << std::setw(6) << attempt << ".dcm";
                fname_str = fname_ss.str();
                output_path = fs::path(output_dir) / fname_str;
                if(!fs::exists(output_path)) break;
            }

            // Write the de-identified DICOM file.
            std::ofstream ofs(output_path, std::ios::binary);
            if(!ofs.good()){
                YLOGWARN("Unable to write file '" << output_path << "', skipping");
                ++fail_count;
                continue;
            }

            root.emit_DICOM(ofs, enc);
            ofs.close();

            ++success_count;
            YLOGINFO("  [" << file_count << "/" << input_files.size() << "] "
                     << input_path.filename() << " -> " << fname_str);

        }catch(const std::exception &e){
            YLOGWARN("Error processing '" << input_path << "': " << e.what());
            ++fail_count;
        }
    }

    YLOGINFO("De-identification complete: " << success_count << " succeeded, "
             << fail_count << " failed out of " << input_files.size() << " files");

    // Save UID mapping if a mapping file was specified.
    if(!uid_map_file.empty()){
        save_uid_mapping(uid_map_file, uid_map);
        YLOGINFO(uid_map.size() << " UID mappings saved to '" << uid_map_file << "'");
    }

    return (fail_count > 0) ? 1 : 0;
}
