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

    of << "artifact : artifact" << std::endl
       << "axilla : axilla" << std::endl
       << "bladder : bladder" << std::endl
       << "body : body" << std::endl
       << "bolus : bolus" << std::endl
       << "bone : bone" << std::endl
       << "both brachial plexuses : both brachial plexuses" << std::endl
       << "both eyes : both eyes" << std::endl
       << "both femoral heads : both femoral heads" << std::endl
       << "both kidneys : both kidneys" << std::endl
       << "both lenses : both lenses" << std::endl
       << "both lungs : both lungs" << std::endl
       << "both parotids : both parotids" << std::endl
       << "both renal cortexes : both renal cortexes" << std::endl
       << "both renal hilum : both renal hilum" << std::endl
       << "brain : brain" << std::endl
       << "brainstem : brainstem" << std::endl
       << "carina : carina" << std::endl
       << "cauda equina : cauda equina" << std::endl
       << "chest wall : chest wall" << std::endl
       << "cochlea : cochlea" << std::endl
       << "contralateral lung : contralateral lung" << std::endl
       << "contralateral parotid : contralateral parotid" << std::endl
       << "ctv : ctv" << std::endl
       << "duodenum : duodenum" << std::endl
       << "esophagus : esophagus" << std::endl
       << "extended ptv : extended ptv" << std::endl
       << "fiducials : fiducials" << std::endl
       << "genitalia : genitalia" << std::endl
       << "great vessels : great vessels" << std::endl
       << "gtv : gtv" << std::endl
       << "heart : heart" << std::endl
       << "ipsilateral brachial plexus : ipsilateral brachial plexus" << std::endl
       << "ipsilateral bronchus : ipsilateral bronchus" << std::endl
       << "ipsilateral femoral head : ipsilateral femoral head" << std::endl
       << "ipsilateral lung : ipsilateral lung" << std::endl
       << "ipsilateral parotid : ipsilateral parotid" << std::endl
       << "isodose : isodose" << std::endl
       << "large bowel : large bowel" << std::endl
       << "laryngopharynx : laryngopharynx" << std::endl
       << "left anterior chamber : left anterior chamber" << std::endl
       << "left brachial plexus : left brachial plexus" << std::endl
       << "left breast : left breast" << std::endl
       << "left cochlea : left cochlea" << std::endl
       << "left eye : left eye" << std::endl
       << "left femoral head : left femoral head" << std::endl
       << "left iliac crest : left iliac crest" << std::endl
       << "left kidney : left kidney" << std::endl
       << "left lacrimal gland : left lacrimal gland" << std::endl
       << "left lens : left lens" << std::endl
       << "left lenses : left lenses" << std::endl
       << "left lung : left lung" << std::endl
       << "left optic nerve : left optic nerve" << std::endl
       << "left parotid : left parotid" << std::endl
       << "left renal cortex : left renal cortex" << std::endl
       << "left renal hilum : left renal hilum" << std::endl
       << "left retina : left retina" << std::endl
       << "left submandibular : left submandibular" << std::endl
       << "left temporal lobe : left temporal lobe" << std::endl
       << "lips : lips" << std::endl
       << "liver : liver" << std::endl
       << "mandible : mandible" << std::endl
       << "optic chiasm : optic chiasm" << std::endl
       << "optics : optics" << std::endl
       << "oral cavity : oral cavity" << std::endl
       << "pharynx : pharynx" << std::endl
       << "planning : planning" << std::endl
       << "proximal bronchial tree : proximal bronchial tree" << std::endl
       << "proximal trachea : proximal trachea" << std::endl
       << "ptv : ptv" << std::endl
       << "right anterior chamber : right anterior chamber" << std::endl
       << "right brachial plexus : right brachial plexus" << std::endl
       << "right breast : right breast" << std::endl
       << "right cochlea : right cochlea" << std::endl
       << "right eye : right eye" << std::endl
       << "right femoral head : right femoral head" << std::endl
       << "right iliac crest : right iliac crest" << std::endl
       << "right kidney : right kidney" << std::endl
       << "right lacrimal gland : right lacrimal gland" << std::endl
       << "right lens : right lens" << std::endl
       << "right lung : right lung" << std::endl
       << "right optic nerve : right optic nerve" << std::endl
       << "right parotid : right parotid" << std::endl
       << "right renal cortex : right renal cortex" << std::endl
       << "right renal hilum : right renal hilum" << std::endl
       << "right retina : right retina" << std::endl
       << "right submandibular : right submandibular" << std::endl
       << "right temporal lobe : right temporal lobe" << std::endl
       << "sacral canal : sacral canal" << std::endl
       << "sacral plexus : sacral plexus" << std::endl
       << "seminal vessicles : seminal vessicles" << std::endl
       << "skin : skin" << std::endl
       << "skull : skull" << std::endl
       << "small bowel : small bowel" << std::endl
       << "spinal canal : spinal canal" << std::endl
       << "spinal cord : spinal cord" << std::endl
       << "stomach : stomach" << std::endl
       << "support : support" << std::endl
       << "thyroid : thyroid" << std::endl
       << "unknown : unknown" << std::endl
       << "urethra : urethra" << std::endl
       << "uterus : uterus" << std::endl;
    if(!of) throw std::runtime_error("Unable to create default lexicon file");

    return p;
}

