//OrderImages.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "OrderImages.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocOrderImages(){
    OperationDoc out;
    out.name = "OrderImages";
    out.desc = 
        "This operation will order individual image slices within collections (Image_Arrays) based on the values"
        " of the specified metadata tags.";

    out.notes.emplace_back(
        "Images are moved, not copied."
    );

    out.notes.emplace_back(
        "Image groupings are retained, and the order of groupings is not altered."
    );

    out.notes.emplace_back(
        "Images that do not contain the specified metadata will be sorted after the end."
    );


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "all";


    out.args.emplace_back();
    out.args.back().name = "Key";
    out.args.back().desc = "Image metadata key to use for ordering."
                           " Images will be sorted according to the key's value 'natural' sorting order, which"
                           " compares sub-strings of numbers and characters separately."
                           " Note this ordering is expected to be stable, but may not always be on some systems.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "AcquisitionTime",
                                 "ContentTime",
                                 "SeriesNumber", 
                                 "SeriesDescription" };

    return out;
}


Drover OrderImages(Drover DICOM_data, 
                           const OperationArgPkg& OptArgs, 
                           const std::map<std::string,std::string>& /*InvocationMetadata*/, 
                           const std::string& /*FilenameLex*/ , const std::list<OperationArgPkg>& /*Children*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto KeyStr = OptArgs.getValueStr("Key").value();

    //-----------------------------------------------------------------------------------------------------------------
    using img_t = planar_image<float,double>;

    // --------
    // Define an ordering that will work for words/characters and numbers mixed together.
    const auto ordering = [KeyStr]( const img_t &A, const img_t &B ) -> bool {
        const auto A_opt = A.GetMetadataValueAs<std::string>(KeyStr);
        const auto B_opt = B.GetMetadataValueAs<std::string>(KeyStr);

        if(  A_opt && !B_opt ){
            return true;
        }else if( !A_opt &&  B_opt ){
            return false;
        }else if( !A_opt && !B_opt ){
            return true; // Non-sensical, because both are NA. Hopefully this at least preserves order.
        }else if(  A_opt &&  B_opt ){
            // A 'natural' sort algorithm that performs look-ahead for numerical values.

            const auto Break = [](std::string in) -> std::vector<std::string> {
                std::vector<std::string> out;
                std::string shtl;
                bool last_was_num = false;
                for(char i : in){
                    const auto as_int = static_cast<int>(i);
                    const auto is_num = ( isdigit(as_int) != 0 ) 
                                        || (!last_was_num && (i == '-'))
                                        || ( last_was_num && (i == '.')) ;  // TODO: Support exponential notation.

                    if( is_num == !last_was_num ){  // Iff there is a transition.
                        if(!shtl.empty()) out.emplace_back(shtl);
                        shtl.clear();
                    }
                    shtl += i;

                    last_was_num = is_num;
                }
                if(!shtl.empty()) out.emplace_back(shtl);
                return out;
            };

            const auto A_vec = Break(A_opt.value());
            const auto B_vec = Break(B_opt.value());

            size_t i = 0;
            while(true){
                // Check if either vectors have run out of tokens.
                if( (A_vec.size() <= i) && (B_vec.size() <= i)){
                    return true; // Strings were (effectively) identical.
                }else if(A_vec.size() <= i){
                    return true;
                }else if(B_vec.size() <= i){
                    return false;
                }

                // Check if either vectors can employ numeric sorting.
                const bool A_is_num = Is_String_An_X<double>(A_vec[i]);
                const bool B_is_num = Is_String_An_X<double>(B_vec[i]);
                if( !A_is_num && !B_is_num ){
                    if( A_vec[i] == B_vec[i] ){
                        ++i;
                        continue;
                    }
                    return (A_vec[i] < B_vec[i]);
                }else if(  A_is_num && !B_is_num ){
                    return true;
                }else if( !A_is_num &&  B_is_num ){
                    return false;
                }else if(  A_is_num &&  B_is_num ){
                    const auto A_num = stringtoX<double>(A_vec[i]);
                    const auto B_num = stringtoX<double>(B_vec[i]);
                    if( A_num == B_num ){
                        ++i;
                        continue;
                    }
                    return (A_num < B_num);
                }

                throw std::logic_error("Should never get here (1/3). Refusing to continue.");
                return true;
            }
            
            throw std::logic_error("Should never get here (2/3). Refusing to continue.");
            return true;
        }

        throw std::logic_error("Should never get here (3/3). Refusing to continue.");
        return true;
    };
    // --------

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        (*iap_it)->imagecoll.images.sort( ordering );
    }

    return DICOM_data;
}
