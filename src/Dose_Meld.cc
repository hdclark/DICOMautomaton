//Dose_Meld.cc - Project - DICOMautomaton. Written by hal clark in 2013.
//
//This code houses routines for converting collections of dose data (ie. stacked 2D images)
// into a single set of dose data. This is useful for some routines which are greatly
// simplified by involving a single dose data collection. In particular, computing the
// min and max with separated collections is nearly impossible to do in a reasonable way
// unless all of the data is melded into a single collection.
//
//NOTE: These routines should be called on an as-needed, single-purpose basis. If required
// some resampling is performed. It is not advised for the outgoing (melded) data to replace
// the original data except in special circumstances (such as for the sole purpose of 
// computing the min/max dose).
//

#include <list>
#include <memory>
#include <string>
//#include <cstdint>   //For int64_t.
//#include <utility>   //For std::pair.
//#include <algorithm> //std::min_element/max_element.
//#include <tuple>

#include "Structs.h"
#include "Regex_Selectors.h"

#include "Dose_Meld.h"

#include "YgorImages.h"
#include "YgorMisc.h"
#include "YgorString.h"

//Filter out all non-dose images, presenting a Drover with only dose image_data.
Drover
Isolate_Dose_Data(Drover d){

    //Gather only dose images.
    auto IAs_all = All_IAs( d );
    auto IAs = Whitelist( IAs_all, "Modality@RTDOSE" );
    if(IAs.empty()){
        throw std::invalid_argument("No dose arrays selected. Cannot continue.");
    }

    //Move the dose images to a list.
    std::list<std::shared_ptr<Image_Array>> dose_imgs;
    for(auto & iap_it : IAs){
        if(!((*iap_it)->imagecoll.images.empty())){
            dose_imgs.push_back( *iap_it );
        }
    }

    d.image_data.clear();
    for(auto &ptr : dose_imgs){
        d.image_data.emplace_back(ptr);
    }

    return d;
}

//This routine removes all dose images (i.e., modality = RTDOSE), melds them, and places only the melded result back.
Drover Meld_Only_Dose_Data(Drover d){

    //Gather only dose images.
    auto IAs_all = All_IAs( d );
    auto IAs = Whitelist( IAs_all, "Modality@RTDOSE" );
    if(IAs.empty()){
        throw std::invalid_argument("No dose arrays selected. Cannot continue.");
    }

    //Move the dose images to a list for melding.
    std::list<std::shared_ptr<Image_Array>> dose_imgs;
    for(auto & iap_it : IAs){
        if(!((*iap_it)->imagecoll.images.empty())){
            dose_imgs.push_back( *iap_it );
            d.image_data.erase( iap_it );
        }
    }

    //Merge the arrays as necessary.
    if(dose_imgs.empty()){
        throw std::invalid_argument("This routine requires at least one image array. Cannot continue");
    }
    dose_imgs = Meld_Image_Data(dose_imgs);

    // Re-attach the melded images.
    //if(dose_imgs.size() != 1){
    //    throw std::invalid_argument("Unable to meld images into a single image array. Cannot continue.");
    //}
    for(auto &ptr : dose_imgs){
        d.image_data.emplace_back(ptr);
    }

    return d;
    
}

//This routine will attempt to meld all data into a single unit. It may not be possible, so multiple data *may* be returned. 
std::list<std::shared_ptr<Image_Array>>  Meld_Image_Data(const std::list<std::shared_ptr<Image_Array>> &dalist){
    //Cycle through the data, checking if neighbouring collections have identical geometry. 
    // If they do, it is fairly safe to combine them. 
    //
    //We need to check if the dose volumes and voxels overlap exactly or not. If they do, we can compute the mean within each
    // separately and sum them afterward. If they don't, the situation becomes very tricky. It is possible to compute it, but
    // it is not done here (if they overlap with zero volume then it is again easy, but it doesn't fit this routine terribly well).
    //
    std::list<std::shared_ptr<Image_Array>> out(dalist);
    out.remove_if([](auto dap){
        return (dap == nullptr);
    });

    if(out.size() == 0) return out;
    if(out.size() == 1) return out;

    auto d2_it = out.begin(); //Note: d*_it are ~ std::list<std::shared_ptr<Image_Array>>::iterator
    auto d1_it = --(out.end());
    while((d1_it != out.end()) && (d2_it != out.end()) && (d1_it != d2_it)){
        //If the geometry is the same, we can easily meld the data. Do so and erase the superfluous data.
        if((*d1_it)->imagecoll.Spatially_eq((*d2_it)->imagecoll)){
            FUNCINFO("Image images are spatially equal. Performing the equivalent-geometry meld routine");

            //Put the melded data into the first position.
            *d1_it = Meld_Equal_Geom_Image_Data(*d1_it, *d2_it);
            d2_it = out.erase(d2_it);
            continue;

        //If the geometry is not the same, we have to further investigate whether we can handle it or not. 
        }else{
            FUNCINFO("Image images are not spatially equal. Performing the nonequivalent-geometry meld routine");

            *d1_it = Meld_Unequal_Geom_Image_Data(*d1_it, *d2_it);
            if(*d1_it != nullptr){
                d2_it = out.erase(d2_it);
                continue;
            }else{
                FUNCERR("Unable to meld nonequivalent-geometry images");
            }
        }
    }
  
    return out;
}

std::unique_ptr<Image_Array> Meld_Equal_Geom_Image_Data(const std::shared_ptr<Image_Array>& A, const std::shared_ptr<Image_Array>& B){
    std::unique_ptr<Image_Array> out(new Image_Array());
    *out = *A; //Performs a deep copy.

    //Now cycle through the voxel data, adjusting the integer dose. We can run through all data 
    // in one pass (each) because the geometry is the same.
    auto i0_it = out->imagecoll.images.begin();
    auto i1_it = A->imagecoll.images.begin();
    auto i2_it = B->imagecoll.images.begin();
    for(   ;    (i0_it != out->imagecoll.images.end()) 
             && (i1_it !=   A->imagecoll.images.end()) 
             && (i2_it !=   B->imagecoll.images.end()); ++i0_it, ++i1_it, ++i2_it){

        const auto rows     = i0_it->rows;
        const auto columns  = i0_it->columns;
        const auto channels = i0_it->channels;
        for(long int r = 0; r < rows; ++r){
            for(long int c = 0; c < columns; ++c){
                for(long int l = 0; l < channels; ++l){
                    //Get the (floating-point) dose from each image.
                    const double dose = static_cast<double>(i1_it->value(r,c,l)) 
                                      + static_cast<double>(i2_it->value(r,c,l));
                    const auto dose_int = static_cast<decltype(i0_it->value(r,c,l))>( dose );                  

                    //Set the new value of the channel.
                    i0_it->reference(r,c,l) = dose_int;
                }
            }
        }
    }

    for(auto &img : out->imagecoll.images){
        img.metadata["Description"] = "Equal-geometry dose melded.";
    }

    return out;
}

/*
//A typical case where dose data collections do *not* have the same geometry.

--(I) In function: get_Contour_Data: The minimum spacing found was 2.9991.
--(I) In function: Compare_Geom: rows     LHS: 71, RHS: 59.
--(I) In function: Compare_Geom: columns  LHS: 96, RHS: 96.
--(I) In function: Compare_Geom: channels LHS: 1, RHS: 1.
--(I) In function: Compare_Geom: pxl_dx   LHS: 5, RHS: 5.
--(I) In function: Compare_Geom: pxl_dy   LHS: 5, RHS: 5.
--(I) In function: Compare_Geom: pxl_dz   LHS: 2.999, RHS: 2.999.
--(I) In function: Compare_Geom: anchor   LHS: (0, 0, 0), RHS: (0, 0, 0).
--(I) In function: Compare_Geom: offset   LHS: (-235.05, -493.05, -1049.62), RHS: (-235.05, -463.05, -1049.62).
--(I) In function: Compare_Geom: row_unit LHS: (0, 1, 0), RHS: (0, 1, 0).
--(I) In function: Compare_Geom: col_unit LHS: (1, 0, 0), RHS: (1, 0, 0).
--(I) In function: Bounded_Image_General: Bits: this: 32 and rhs: 32.
--(I) In function: Bounded_Image_General: Grid scaling: this: 5.34888e-06 and rhs: 1.04901e-05.
--(E) In function: Bounded_Image_General: This function cannot handle data sets with multiple, distinctly-shaped dose data. Terminating program.
*/

//Returns a nullptr on failure to meld. This is a fairly risky operation, so be weary of the data coming
// from this function.
std::unique_ptr<Image_Array> Meld_Unequal_Geom_Image_Data(std::shared_ptr<Image_Array> A, const std::shared_ptr<Image_Array>& B){
    std::unique_ptr<Image_Array> out(new Image_Array());

    //Determine whether or not we can meld the data. Currently we can only handle the case where
    // a) all images in each set are same # of rows and columns as the others of the (same) set,
    // b) one set is larger/encompasses the other set (ie. one is larger),

    //------------------------------ Data verification/suitability inspection ------------------------------
    if(A->imagecoll.images.empty() || B->imagecoll.images.empty()){
        return nullptr;
    }

    for(auto i1_it = A->imagecoll.images.begin(), i2_it = --(A->imagecoll.images.begin());
                (i1_it != A->imagecoll.images.end()) && (i2_it != A->imagecoll.images.end());    ){
        if( (i1_it->channels != i2_it->channels)
                || ((i1_it->rows > i2_it->rows) && (i1_it->columns < i2_it->columns)) //One is larger than the other.
                || ((i1_it->rows < i2_it->rows) && (i1_it->columns > i2_it->columns))  ){
            FUNCWARN("Unable to meld - one data set (A) is not self-consistent");
            return nullptr;
        }
    }

    for(auto i1_it = B->imagecoll.images.begin(), i2_it = --(B->imagecoll.images.begin());
                (i1_it != B->imagecoll.images.end()) && (i2_it != B->imagecoll.images.end());    ){
        if( (i1_it->channels != i2_it->channels)
                || ((i1_it->rows > i2_it->rows) && (i1_it->columns < i2_it->columns)) //One is larger than the other.
                || ((i1_it->rows < i2_it->rows) && (i1_it->columns > i2_it->columns))  ){
            FUNCWARN("Unable to meld - one data set (B) is not self-consistent");
            return nullptr;
        }
    }


    //------------------------------------- Preparation for melding ----------------------------------------

    //Get the larger of the two images.
    decltype(A) larger = B;
    if( A->imagecoll.volume() >= B->imagecoll.volume() ){
        larger = A;
    }

    *out = *larger; //Make a deep copy of the data.

    //Now cycle through the voxel data, collecting the dose contributions from either A or B. 
    auto i0_it = out->imagecoll.images.begin();
    auto i1_it = A->imagecoll.images.begin();
    auto i2_it = B->imagecoll.images.begin();
    for(   ;    (i0_it != out->imagecoll.images.end())
             && (i1_it !=   A->imagecoll.images.end())
             && (i2_it !=   B->imagecoll.images.end()); ++i0_it, ++i1_it, ++i2_it){

        const auto rows     = i0_it->rows;
        const auto columns  = i0_it->columns;
        const auto channels = i0_it->channels;
        for(long int r = 0; r < rows; ++r){
            for(long int c = 0; c < columns; ++c){
                for(long int l = 0; l < channels; ++l){
                    //Clear the value in the channel.
                    const auto zero = static_cast<decltype(i0_it->value(r,c,l))>(0);
                    i0_it->reference(r,c,l) = zero;

                    //Get the (floating-point) dose from each image. If out of bounds, we will 
                    // get a safe zero. 
                    const auto pos = i0_it->position(r,c);
                    auto dose_sum = zero;

                    const auto indexA = i1_it->index(pos,l);
                    if(indexA != -1){
                        dose_sum += i1_it->value(indexA);
                    }

                    const auto indexB = i2_it->index(pos,l);
                    if(indexB != -1){
                        dose_sum += i2_it->value(indexB);
                    }

                    //Set the new value.
                    i0_it->reference(r,c,l) = dose_sum;
                }
            }
        }
    }

    for(auto &img : out->imagecoll.images){
        img.metadata["Description"] = "Unequal-geometry dose melded.";
    }

    return out;
}

