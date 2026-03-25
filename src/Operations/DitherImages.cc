//DitherImages.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <cstdint>
#include <cmath>
#include <limits>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../YgorImages_Functors/ConvenienceRoutines.h"

#include "DitherImages.h"
#include "YgorImages.h"
#include "YgorImagesDithering.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"


OperationDoc OpArgDocDitherImages(){
    OperationDoc out;
    out.name = "DitherImages";

    out.tags.emplace_back("category: image processing");

    out.desc = 
        "This operation applies Floyd-Steinberg error diffusion dithering to the selected images."
        " Dithering reduces the number of distinct pixel values (typically to two: a low and high value)"
        " while preserving the visual appearance of the image through error diffusion."
        " Pixels above the threshold are set to the high value, and pixels below are set to the low value."
        " The quantization error from each pixel is diffused to neighbouring pixels.";

    out.notes.emplace_back(
        "This routine operates on individual images independently."
    );
    out.notes.emplace_back(
        "If the threshold is not explicitly specified, it defaults to the midpoint between the low and high values."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The image channel to dither."
                           " If negative, all channels are dithered.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1", "0", "1", "2" };

    out.args.emplace_back();
    out.args.back().name = "Low";
    out.args.back().desc = "The low output value assigned to pixels below the threshold."
                           " The default is the lowest representable pixel value.";
    out.args.back().default_val = "nan";
    out.args.back().expected = true;
    out.args.back().examples = { "nan", "0.0", "-1.0", "-1000.0" };

    out.args.emplace_back();
    out.args.back().name = "High";
    out.args.back().desc = "The high output value assigned to pixels above the threshold."
                           " The default is the highest representable pixel value.";
    out.args.back().default_val = "nan";
    out.args.back().expected = true;
    out.args.back().examples = { "nan", "1.0", "1000.0", "3000.0" };

    out.args.emplace_back();
    out.args.back().name = "Threshold";
    out.args.back().desc = "The threshold value used for dithering."
                           " Pixels above this value are mapped to the high value and pixels below to the low value,"
                           " with the quantization error diffused to neighbouring pixels."
                           " If set to 'nan', the threshold defaults to the midpoint between the low and high values.";
    out.args.back().default_val = "nan";
    out.args.back().expected = true;
    out.args.back().examples = { "nan", "0.0", "0.5", "128.0", "500.0" };

    return out;
}

bool DitherImages(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                    const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    const auto LowStr = OptArgs.getValueStr("Low").value();
    const auto HighStr = OptArgs.getValueStr("High").value();
    const auto ThresholdStr = OptArgs.getValueStr("Threshold").value();

    //-----------------------------------------------------------------------------------------------------------------

    const auto low = (LowStr == "nan") ? std::numeric_limits<float>::lowest()
                                       : std::stof(LowStr);
    const auto high = (HighStr == "nan") ? std::numeric_limits<float>::max()
                                         : std::stof(HighStr);
    const auto threshold = (ThresholdStr == "nan") ? std::numeric_limits<long double>::quiet_NaN()
                                                   : static_cast<long double>(std::stod(ThresholdStr));

    // Determine which channels to dither.
    std::set<int64_t> chnls;
    if(0 <= Channel){
        chnls.insert(Channel);
    }
    // If chnls is empty, Floyd_Steinberg_Dither will dither all channels.

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        for(auto &animg : (*iap_it)->imagecoll.images){
            Floyd_Steinberg_Dither(animg, chnls, low, high, threshold);
            UpdateImageWindowCentreWidth( std::ref(animg) );
            UpdateImageDescription( std::ref(animg), "Dithered" );
        }
    }

    return true;
}
