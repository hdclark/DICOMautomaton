//DICOM_File_Loader.cc - A part of DICOMautomaton 2016. Written by hal clark.
//
// This program loads data from DICOM files without involving a PACS or other entity.
//

#include <exception>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <string>    
//#include <cfenv>              //Needed for std::feclearexcept(FE_ALL_EXCEPT).

#include <boost/algorithm/string/predicate.hpp>
#include <filesystem>
#include <algorithm>
#include <cstdlib>            //Needed for exit() calls.

#include "Explicator.h"       //Needed for Explicator class.
#include "Imebra_Shim.h"      //Wrapper for Imebra library. Black-boxed to speed up compilation.
#include "Structs.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


static
std::unique_ptr<Contour_Data>
Concatenate_Contour_Data(std::unique_ptr<Contour_Data> A,
                         std::unique_ptr<Contour_Data> B){
    //This routine concatenates A and B's contour collections. No internal checking is performed.
    // No copying is performed, but A and B are consumed. A is returned as if it were a new pointer.
    A->ccs.splice( A->ccs.end(), std::move(B->ccs) );
    return A;
}


bool Load_From_DICOM_Files( Drover &DICOM_data,
                            const std::map<std::string,std::string> & /* InvocationMetadata */,
                            const std::string &FilenameLex,
                            std::list<std::filesystem::path> &Filenames ){

    //This routine will attempt to load DICOM files on an individual file basis. Files that are not successfully loaded
    // are not consumed so that they can be passed on to the next loading stage as needed. 
    //
    // Note: This routine returns false only iff a file is suspected of being suited for this loader, but could not be
    //       loaded (e.g., the file seems appropriate, but a parsing failure was encountered).
    //
    if(Filenames.empty()) return true;

    using loaded_imgs_storage_t = decltype(DICOM_data.image_data);
    std::list<loaded_imgs_storage_t> loaded_imgs_storage;
    using loaded_dose_storage_t = decltype(DICOM_data.image_data);
    std::list<loaded_dose_storage_t> loaded_dose_storage;
    std::shared_ptr<Contour_Data> loaded_contour_data_storage = std::make_shared<Contour_Data>();

    //This routine currently assumes ALL image files are part of the same image set. Same for dose files.
    // (To change this behaviour, it will suffice to emplace_back() the storage lists as needed.)
    loaded_imgs_storage.emplace_back();
    loaded_dose_storage.emplace_back();

    size_t i = 0;
    const size_t N = Filenames.size();

    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        FUNCINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "% \t" << *bfit);
        ++i;

        const auto Filename = bfit->string();
        std::string Modality;
        try{
            Modality = get_modality(Filename);
        }catch(const std::exception &){
            Modality = "";
        };

        if(boost::iequals(Modality,"RTRECORD")){
            FUNCWARN("RTRECORD file encountered. "
                     "DICOMautomaton currently is not equipped to read RTRECORD-modality DICOM files. "
                     "Disregarding it");

            bfit = Filenames.erase( bfit );  // Consume the file; we know what it is, but cannot make use of it.

        }else if(boost::iequals(Modality,"REG")){
            FUNCWARN("REG file encountered. "
                     "DICOMautomaton currently is not equipped to read REG-modality DICOM files. "
                     "Disregarding it");

            bfit = Filenames.erase( bfit );  // Consume the file; we know what it is, but cannot make use of it.

        }else if(boost::iequals(Modality,"RTPLAN")){
            FUNCWARN("RTPLAN file support is experimental");

            auto tplan = Load_TPlan_Config(Filename);
            DICOM_data.tplan_data.emplace_back( std::move(tplan) );

            bfit = Filenames.erase( bfit ); 

        }else if(boost::iequals(Modality,"RTSTRUCT")){
            const auto preloadcount = loaded_contour_data_storage->ccs.size();
            try{
                auto combined = Concatenate_Contour_Data( loaded_contour_data_storage->Duplicate(),
                                                          get_Contour_Data(Filename));
                loaded_contour_data_storage = std::move(combined);

            }catch(const std::exception &e){
                FUNCWARN("Difficulty encountered during contour data loading: '" << e.what() << "'. Ignoring file and continuing");
                //loaded_contour_data_storage.back().pop_back();
                bfit = Filenames.erase( bfit ); 
                continue;
            }

            const auto postloadcount = loaded_contour_data_storage->ccs.size();
            if(postloadcount == preloadcount){
                FUNCWARN("RTSTRUCT file was loaded, but contained no ROIs");
                return false;
                //If you get here, it isn't necessarily an error. But something has most likely gone wrong. Why bother
                // to load an RTSTRUCT file if it is empty? If you know what you're doing, you can safely disable this
                // error and pop the last-added data. Otherwise, try examining the contour loading code and file data.
            }

            bfit = Filenames.erase( bfit ); 

        }else if(boost::iequals(Modality,"RTDOSE")){
            try{
                loaded_dose_storage.back().push_back( Load_Dose_Array(Filename));
            }catch(const std::exception &e){
                FUNCWARN("Difficulty encountered during dose array loading: '" << e.what() << "'. Ignoring file and continuing");
                //loaded_dose_storage.back().pop_back();
                bfit = Filenames.erase( bfit ); 
                continue;
            }

            bfit = Filenames.erase( bfit ); 

        }else if(  boost::iequals(Modality,"CT")
                || boost::iequals(Modality,"OT")
                || boost::iequals(Modality,"US")
                || boost::iequals(Modality,"MR")
                || boost::iequals(Modality,"RTIMAGE")
                || boost::iequals(Modality,"PT") ){

            try{
                loaded_imgs_storage.back().push_back( Load_Image_Array(Filename));
            }catch(const std::exception &e){
                FUNCWARN("Difficulty encountered during image array loading: '" << e.what() << "'. Ignoring file and continuing");
                //loaded_imgs_storage.back().pop_back();
                bfit = Filenames.erase( bfit ); 
                continue;
            }

            if(loaded_imgs_storage.back().back()->imagecoll.images.size() != 1){
                FUNCWARN("More or less than one image loaded into the image array. You'll need to tweak the code to handle this");
                return false;
                //If you get here, you've tried to load a file that contains more than one image slice. This is OK,
                // (and is legitimate behaviour) but you'll need to update the following code to ensure each file's 
                // metadata is set accordingly. This is all you need to do at the time of writing, but take a look over
                // the rest of the code to ensure the code doesn't assume too much.
            }
            
            bfit = Filenames.erase( bfit ); 

            //If we want to add any additional image metadata, or replace the default Imebra_Shim.cc populated metadata
            // with, say, the non-null PostgreSQL metadata, it should be done here.
            loaded_imgs_storage.back().back()->imagecoll.images.back().metadata["Filename"] = Filename;
            loaded_imgs_storage.back().back()->imagecoll.images.back().metadata["dt"] = "0.0";
            // ... more metadata operations ...

        }else{
            //Skip the file. It might be destined for some other loader.
            ++bfit;
        }
    }
            
    //If nothing was loaded, do not post-process.
    const size_t N2 = Filenames.size();
    if(N == N2) return true;


    // ----------------------------------------------- Post-processing -----------------------------------------------

    //Attempt contour name normalization using the selected lexicon.
    {
        Explicator X(FilenameLex);
        for(auto & cc : loaded_contour_data_storage->ccs){
             for(auto & c : cc.contours){
                 const auto NormalizedROIName = X(c.metadata["ROIName"]); //Could be cached, externally or internally.
                 c.metadata["NormalizedROIName"] = NormalizedROIName;
             }
        }
    }

    //Concatenate contour data into the Drover instance.
    {
        if(DICOM_data.contour_data == nullptr) DICOM_data.contour_data = std::make_shared<Contour_Data>();
        auto combined = Concatenate_Contour_Data( DICOM_data.contour_data->Duplicate(),
                                                  loaded_contour_data_storage->Duplicate());
        DICOM_data.contour_data = std::move(combined);
    }

    //Collate each group of images into a single set, if possible. Also stuff the correct contour data in the same set.
    // Also load dose data into the fray.
    for(auto &loaded_img_set : loaded_imgs_storage){
        if(loaded_img_set.empty()) continue;

        auto collated_imgs = Collate_Image_Arrays(loaded_img_set);
        if(!collated_imgs){
            FUNCWARN("Unable to collate images. It is possible to continue, but only if you are able to handle this case");
            return false;
        }

        DICOM_data.image_data.emplace_back(std::move(collated_imgs));
    }
    FUNCINFO("Number of image set groups currently loaded = " << DICOM_data.image_data.size());

    for(auto &loaded_dose_set : loaded_dose_storage){
        if(loaded_dose_set.empty()) continue;

        //There are two options here, depending on what the user wishes to do: treat dose as a regular image, or as
        // special dose images. The more 'modern' way is to treat everything uniformly as images, but the old dose
        // computation methods require the distinction to be made. 

        // Option A: stuff the dose data into the Drover's Image_Array.
        for(auto &lds : loaded_dose_set){
            DICOM_data.image_data.emplace_back( std::move(lds) );
        }

        // Option B: stuff the dose data into the Drover's Image_Array so it can be more easily used with image
        // processing routines.
        //for(auto &lds : loaded_dose_set){
        //    DICOM_data.image_data.emplace_back();
        //    DICOM_data.image_data.back() = std::make_shared<Image_Array>(*lds);
        //}
    }


    //Sort the images in some reasonable way (opposed to the order they were located on disk -- arbitrary).
    if(true){
        for(auto & img_arr_ptr : DICOM_data.image_data){
            img_arr_ptr->imagecoll.Stable_Sort_on_Metadata_Keys_Value_Numeric<long int>("InstanceNumber");
            img_arr_ptr->imagecoll.Stable_Sort_on_Metadata_Keys_Value_Numeric<double>("SliceLocation");
            img_arr_ptr->imagecoll.Stable_Sort_on_Metadata_Keys_Value_Numeric<double>("dt");

            img_arr_ptr->imagecoll.Stable_Sort_on_Metadata_Keys_Value_Lexicographic("Modality");
            img_arr_ptr->imagecoll.Stable_Sort_on_Metadata_Keys_Value_Lexicographic("PatientID");
        }
    }

    return true;
}
