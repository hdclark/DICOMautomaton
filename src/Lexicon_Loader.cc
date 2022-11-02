//Lexicon_Loader.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <stdexcept>
#include <string>
#include <list> 
#include <filesystem>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for SplitStringToVector, Canonicalize_String2, SplitVector functions.
#include "YgorFilesDirs.h"

// This function attempts to locate a lexicon file. If none are available, an empty string is returned.
std::string Locate_Lexicon_File(){

    std::list<std::string> trial = { 
            // General, all-purpose lexicon suitable for 'standard' photon external beam therapy.
            "20201007_standard_sites.lexicon",
            "Lexicons/20201007_standard_sites.lexicon",
            "/usr/share/explicator/lexicons/20201007_standard_sites.lexicon",

            // Updated H&N-specific lexicon derived from a large cohort of study patients.
            "20191212_SGF_and_SGFQ_tags.lexicon",
            "Lexicons/20191212_SGF_and_SGFQ_tags.lexicon",
            "/usr/share/explicator/lexicons/20191212_SGF_and_SGFQ_tags.lexicon",

            // Classic H&N-specific lexicons derived from a large cohort of study patients.
            "20150925_SGF_and_SGFQ_tags.lexicon",
            "Lexicons/20150925_SGF_and_SGFQ_tags.lexicon",
            "/usr/share/explicator/lexicons/20150925_20150925_SGF_and_SGFQ_tags.lexicon",

            // Older fallbacks.
            "/usr/share/explicator/lexicons/20130319_SGF_filter_data_deciphered5.lexicon",
            "/usr/share/explicator/lexicons/20121030_SGF_filter_data_deciphered4.lexicon" };

    for(const auto & f : trial){
        if(Does_File_Exist_And_Can_Be_Read(f)){
            return f;
        }
    }

    return std::string();
}


// This function creates a default lexicon file in a temporary location. The full path is returned.
std::string Create_Default_Lexicon_File(){

    const auto dir = (std::filesystem::temp_directory_path() / "dcma_").string();
    const auto p = Get_Unique_Filename(dir, 6, ".lexicon");

    // Seed the lexicon with some reasonable defaults.
    std::ofstream of(p);

    const std::vector<std::string> cleans {{
        "artifact",
        "axilla",
        "bladder",
        "body",
        "bolus",
        "bone",
        "both brachial plexuses",
        "both eyes",
        "both femoral heads",
        "both kidneys",
        "both lenses",
        "both lungs",
        "both parotids",
        "both renal cortexes",
        "both renal hilum",
        "brain",
        "brainstem",
        "carina",
        "cauda equina",
        "chest wall",
        "cochlea",
        "contralateral lung",
        "contralateral parotid",
        "ctv",
        "duodenum",
        "esophagus",
        "extended ptv",
        "fiducials",
        "genitalia",
        "great vessels",
        "gtv",
        "heart",
        "ipsilateral brachial plexus",
        "ipsilateral bronchus",
        "ipsilateral femoral head",
        "ipsilateral lung",
        "ipsilateral parotid",
        "isodose",
        "large bowel",
        "laryngopharynx",
        "left anterior chamber",
        "left brachial plexus",
        "left breast",
        "left cochlea",
        "left eye",
        "left femoral head",
        "left iliac crest",
        "left kidney",
        "left lacrimal gland",
        "left lens",
        "left lenses",
        "left lung",
        "left optic nerve",
        "left parotid",
        "left renal cortex",
        "left renal hilum",
        "left retina",
        "left submandibular",
        "left temporal lobe",
        "lips",
        "liver",
        "mandible",
        "optic chiasm",
        "optics",
        "oral cavity",
        "pharynx",
        "planning",
        "proximal bronchial tree",
        "proximal trachea",
        "ptv",
        "right anterior chamber",
        "right brachial plexus",
        "right breast",
        "right cochlea",
        "right eye",
        "right femoral head",
        "right iliac crest",
        "right kidney",
        "right lacrimal gland",
        "right lens",
        "right lung",
        "right optic nerve",
        "right parotid",
        "right renal cortex",
        "right renal hilum",
        "right retina",
        "right submandibular",
        "right temporal lobe",
        "sacral canal",
        "sacral plexus",
        "seminal vessicles",
        "skin",
        "skull",
        "small bowel",
        "spinal canal",
        "spinal cord",
        "stomach",
        "support",
        "thyroid",
        "unknown",
        "urethra",
        "uterus" }};
    
    for(const auto&c : cleans){
        of << c << " : " << c;
        
        // Add some common abbreviations.
        //
        // Replace 'left ' with 'l ', and 'right ' with 'r ', if present.
        const auto l = ReplaceAllInstances(c, "left ", "l ");
        const auto r = ReplaceAllInstances(c, "right ", "r ");
        if(l != c) of << c << " : " << l;
        if(r != c) of << c << " : " << r;
    }

    if(!of) throw std::runtime_error("Unable to create default lexicon file");

    return p;
}

