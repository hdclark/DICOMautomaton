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
#include <random>

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

// Get a temporary directory, respecting TMPDIR/TMP/TEMP environment variables.
static fs::path get_temp_dir(){
    for(const char *var : {"TMPDIR", "TMP", "TEMP"}){
        const char *val = std::getenv(var);
        if(val != nullptr && val[0] != '\0'){
            std::error_code ec;
            if(fs::is_directory(val, ec)) return fs::path(val);
        }
    }
    return fs::temp_directory_path();
}

// Create a unique temporary file in the given directory. Returns the path.
// Thread-safe: uses a random suffix to avoid collisions.
static fs::path create_temp_file(const fs::path &dir){
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;

    for(int attempt = 0; attempt < 1000; ++attempt){
        std::ostringstream ss;
        ss << "dcma_deid_" << std::hex << dist(rng) << ".tmp";
        fs::path p = dir / ss.str();
        // Attempt to create the file exclusively.
        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        if(ofs.good()){
            ofs.close();
            return p;
        }
    }
    throw std::runtime_error("Failed to create temporary file in '" + dir.string() + "'");
}

// Strip trailing null and space padding from a DICOM string value.
static std::string strip_padding(const std::string &s){
    std::string out = s;
    while(!out.empty() && (out.back() == '\0' || out.back() == ' ')) out.pop_back();
    return out;
}

// Look up a DICOM tag value by keyword (e.g., "Modality", "StudyID") from a parsed tree.
// Returns the stripped value, or an empty string if not found.
static std::string lookup_tag_value_by_keyword(const DCMA_DICOM::Node &root,
                                                const std::string &keyword,
                                                const std::vector<const DCMA_DICOM::DICOMDictionary*> &dicts){
    // Search the dictionary for the keyword to find its (group, element).
    const auto &default_dict = DCMA_DICOM::get_default_dictionary();
    for(const auto &[key, entry] : default_dict){
        if(entry.keyword == keyword){
            const auto *node = root.find(key.first, key.second);
            if(node != nullptr){
                return strip_padding(node->val);
            }
            return "";
        }
    }
    // Also search any additional dictionaries.
    for(const auto *dict : dicts){
        if(dict == nullptr) continue;
        for(const auto &[key, entry] : *dict){
            if(entry.keyword == keyword){
                const auto *node = root.find(key.first, key.second);
                if(node != nullptr){
                    return strip_padding(node->val);
                }
                return "";
            }
        }
    }
    return "";
}

// Sanitize a string for use as a filename component: replace non-alphanumeric, non-dot,
// non-hyphen, non-underscore characters with underscores. Collapse consecutive dots and
// prevent leading dots/hyphens to mitigate path traversal risks.
static std::string sanitize_for_filename(const std::string &s){
    std::string out;
    out.reserve(s.size());
    for(const char c : s){
        if(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'){
            out += c;
        }else if(c == '.'){
            // Avoid consecutive dots (e.g., "..") and leading dots.
            if(!out.empty() && out.back() != '.'){
                out += c;
            }else{
                out += '_';
            }
        }else{
            out += '_';
        }
    }
    // Remove leading dots or hyphens.
    while(!out.empty() && (out.front() == '.' || out.front() == '-')){
        out.erase(out.begin());
    }
    // Remove trailing dots.
    while(!out.empty() && out.back() == '.'){
        out.pop_back();
    }
    return out;
}

// Expand a filename pattern using GNU mkstemp-like syntax:
//   ${VarName}  -- expanded to the value of the named DICOM tag (by keyword), sanitized
//   XXXXXXXX    -- replaced by the counter value, zero-padded to the number of X's
//
// Example: "${Modality}_${StudyID}-anon_XXXXXXXX.dcm"
// The counter is used for the X-placeholder expansion. The suffix (e.g., ".dcm") is preserved.
static std::string expand_filename_pattern(const std::string &pattern,
                                           const DCMA_DICOM::Node &root,
                                           const std::vector<const DCMA_DICOM::DICOMDictionary*> &dicts,
                                           int64_t counter){
    std::string result;
    result.reserve(pattern.size());
    size_t i = 0;
    while(i < pattern.size()){
        // Expand ${VarName} references.
        if(pattern[i] == '$' && (i + 1) < pattern.size() && pattern[i + 1] == '{'){
            const auto end = pattern.find('}', i + 2);
            if(end != std::string::npos){
                const std::string var_name = pattern.substr(i + 2, end - (i + 2));
                std::string value = lookup_tag_value_by_keyword(root, var_name, dicts);
                if(value.empty()) value = "Unknown";
                result += sanitize_for_filename(value);
                i = end + 1;
                continue;
            }
        }
        // Expand consecutive X placeholders.
        if(pattern[i] == 'X'){
            size_t x_start = i;
            while(i < pattern.size() && pattern[i] == 'X') ++i;
            const auto x_count = static_cast<int>(i - x_start);
            std::ostringstream ss;
            ss << std::setfill('0') << std::setw(x_count) << counter;
            result += ss.str();
            continue;
        }
        result += pattern[i];
        ++i;
    }
    return result;
}


int main(int argc, char **argv){
    std::string output_dir;
    std::string patient_id;
    std::string patient_name;
    std::string study_id;
    std::string uid_map_file;
    std::string filename_pattern = "${Modality}_XXXXXXXX.dcm";
    bool lenient = false;
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

    arger.push_back( ygor_arg_handlr_t(4, 'l', "lenient", false, "",
      "Enable lenient mode: skip VR validation checks during DICOM emission."
      " Useful for passing through non-conformant files that nevertheless need to be de-identified.",
      [&](const std::string &) -> void {
        lenient = true;
      })
    );

    arger.push_back( ygor_arg_handlr_t(5, 'f', "filename-pattern", true, "${Modality}_XXXXXXXX.dcm",
      "Output filename pattern. ${VarName} is expanded to DICOM tag values by keyword"
      " (e.g., ${Modality}, ${StudyID}). Consecutive X's are replaced with a zero-padded"
      " counter. Default: '${Modality}_XXXXXXXX.dcm'.",
      [&](const std::string &optarg) -> void {
        filename_pattern = optarg;
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

    YLOGINFO("Processing " << input_files.size() << " input file(s)"
             << (lenient ? " (lenient mode)" : ""));

    const auto &default_dict = DCMA_DICOM::get_default_dictionary();
    std::vector<const DCMA_DICOM::DICOMDictionary*> dicts = { &default_dict };
    DCMA_DICOM::DICOMDictionary mutable_dict;

    // Determine the temporary directory for safe emission.
    const auto tmpdir = get_temp_dir();
    YLOGDEBUG("Using temporary directory '" << tmpdir << "'");

    int64_t file_count = 0;
    int64_t success_count = 0;
    int64_t fail_count = 0;

    for(const auto &input_path : input_files){
        ++file_count;

        fs::path tmp_path;  // Track temp file for cleanup on failure.
        try{
            // Read the DICOM file.
            YLOGDEBUG("Reading DICOM file '" << input_path << "'");
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
            YLOGDEBUG("Detected encoding: " << (enc == DCMA_DICOM::Encoding::ELE ? "ELE" : "ILE"));

            // Apply de-identification.
            YLOGDEBUG("Applying de-identification to '" << input_path << "'");
            DCMA_DICOM::deidentify(root, params, uid_map);

            // Emit to a temporary file first. This avoids writing partially de-identified
            // files to the output directory, and avoids doubling peak memory usage for large
            // files (e.g., those with large Pixel Data).
            tmp_path = create_temp_file(tmpdir);
            YLOGDEBUG("Emitting de-identified DICOM to temp file '" << tmp_path << "'");
            {
                std::ofstream tmp_ofs(tmp_path, std::ios::binary);
                if(!tmp_ofs.good()){
                    throw std::runtime_error("Unable to open temporary file '"_s + tmp_path.string() + "' for writing.");
                }
                root.emit_DICOM(tmp_ofs, enc, true, lenient);
                tmp_ofs.close();
                if(!tmp_ofs.good()){
                    throw std::runtime_error("Write error to temporary file '"_s + tmp_path.string() + "'.");
                }
            }

            // Generate the output filename from the pattern, expanding ${Tag} variables
            // and X-placeholders. Use atomic copy to avoid TOCTOU races: try
            // copy_options::skip_existing and increment the counter on collision.
            fs::path output_path;
            bool copied = false;
            for(int64_t attempt = file_count; !copied; ++attempt){
                const auto fname_str = expand_filename_pattern(filename_pattern, root, dicts, attempt);
                output_path = fs::path(output_dir) / fname_str;
                // skip_existing returns false (without error) if the file already exists,
                // avoiding a TOCTOU race between existence check and copy.
                copied = fs::copy_file(tmp_path, output_path, fs::copy_options::skip_existing);
            }

            // Remove the temp file.
            std::error_code ec;
            fs::remove(tmp_path, ec);
            tmp_path.clear();

            ++success_count;
            YLOGINFO("  [" << file_count << "/" << input_files.size() << "] "
                     << input_path.filename() << " -> " << output_path.filename());

        }catch(const std::exception &e){
            YLOGERR("Error processing '" << input_path << "': " << e.what());

            // Clean up temp file if it exists.
            if(!tmp_path.empty()){
                std::error_code ec;
                fs::remove(tmp_path, ec);
            }

            ++fail_count;
            break;
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
