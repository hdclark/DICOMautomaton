//PACS_Loader.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.
//
// This program loads DICOM files from a (custom DICOMautomaton) PACS database.
//

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>    
#include <vector>
#include <set> 
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
//#include <future>             //Needed for std::async(...)
#include <limits>
#include <cmath>
//#include <cfenv>              //Needed for std::feclearexcept(FE_ALL_EXCEPT).

#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

#include <boost/algorithm/string.hpp> //For boost:iequals().

#include <pqxx/pqxx>          //PostgreSQL C++ interface.

#include "Structs.h"
#include "Imebra_Shim.h"      //Wrapper for Imebra library. Black-boxed to speed up compilation.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"   //Needed for bimap class.
#include "YgorPerformance.h"  //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesPlotting.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorDICOMTools.h"   //Needed for Is_File_A_DICOM_File(...);


static
std::unique_ptr<Contour_Data> 
Concatenate_Contour_Data(std::unique_ptr<Contour_Data> A,
                         std::unique_ptr<Contour_Data> B){
    //This routine concatenates A and B's contour collections. No internal checking is performed.
    // No copying is performed, but A and B are consumed. A is returned as if it were a new pointer.
    A->ccs.splice( A->ccs.end(), std::move(B->ccs) );
    return std::move(A);
}


bool Load_From_PACS_DB( Drover &DICOM_data,
                        std::map<std::string,std::string> & /* InvocationMetadata */,
                        std::string &FilenameLex,
                        std::string &db_connection_params,
                        std::list<std::list<std::string>> &GroupedFilterQueryFiles ){

    std::set<std::string> FrameofReferenceUIDs;

    FUNCINFO("Executing database queries...");



    //Prepare separate storage space for each of the groups of filter query files. We keep them segregated based on the
    // user's grouping of input query files. This allows us to work on several distinct data sets per invocation, if
    // desired. (Often one just wants to open a single data set, though.)
    using loaded_imgs_storage_t = decltype(DICOM_data.image_data);
    std::list<loaded_imgs_storage_t> loaded_imgs_storage;
    using loaded_dose_storage_t = decltype(DICOM_data.dose_data);
    std::list<loaded_dose_storage_t> loaded_dose_storage;
    std::shared_ptr<Contour_Data> loaded_contour_data_storage = std::make_shared<Contour_Data>();

    try{
        //Loop over each group of filter query files.
        for(const auto &FilterQueryFiles : GroupedFilterQueryFiles){
            loaded_imgs_storage.emplace_back();
            loaded_dose_storage.emplace_back();

            //Unfortunately, it seems one cannot reset or deactivate/reactivate a connection. So we are forced to 
            // start anew each time.
            //
            // Also note that the libpqxx documentaton states that transactional connections are required if using
            // PostgreSQL large files.
            //
            pqxx::connection c(db_connection_params);
            pqxx::work txn(c);

            //-------------------------------------------------------------------------------------------------------------
            //Query1 stage: select records from the system pacs database.
            //
            //Whatever is in the file(s), let the database figure out if they're legal and valid.
            pqxx::result r1;
     
            std::stringstream ss;
            for(const auto &FilterQueryFile : FilterQueryFiles){ 
                ss << "'" << FilterQueryFile << "'"; //Save the names in case something goes wrong.
                const auto query1 = LoadFileToString(FilterQueryFile);
                r1 = txn.exec(query1);
            }
            if(r1.empty()){
                FUNCWARN("Database query1 stage " << ss.str() << " resulted in no records. Cannot continue");
                return false;
            }
    
    
            //-------------------------------------------------------------------------------------------------------------
            FUNCINFO("Query1 stage: number of records found = " << r1.size());
    
            //-------------------------------------------------------------------------------------------------------------
            //Query2 stage: process each record, loading whatever data is needed later into memory.
            for(pqxx::result::size_type i = 0; i != r1.size(); ++i){
                FUNCINFO("Parsing file #" << i+1 << "/" << r1.size() << " = " << 100*(i+1)/r1.size() << "%");

                //Get the returned pacsid.
                //const auto pacsid = r1[i]["pacsid"].as<long int>();
                const auto StoreFullPathName = (r1[i]["StoreFullPathName"].is_null()) ? 
                                               "" : r1[i]["StoreFullPathName"].as<std::string>();

                //Parse the file and/or try load the data. Push it into the list (we can collate later).
                // If we cannot ascertain the type then we will treat it as an image and hope it can be loaded.
                const auto Modality = r1[i]["Modality"].as<std::string>();
                if(boost::iequals(Modality,"RTSTRUCT")){
                    const auto preloadcount = loaded_contour_data_storage->ccs.size();
                    try{
                        auto combined = Concatenate_Contour_Data( std::move(loaded_contour_data_storage->Duplicate()),
                                                                  std::move(get_Contour_Data(StoreFullPathName)) );
                        loaded_contour_data_storage = std::move(combined);
                    }catch(const std::exception &e){
                        FUNCWARN("Difficulty encountered during contour data loading: '" << e.what() <<
                                 "'. Ignoring file and continuing");
                        //loaded_contour_data_storage.back().pop_back();
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


                }else if(boost::iequals(Modality,"RTDOSE")){
                    try{
                        loaded_dose_storage.back().push_back( std::move(Load_Dose_Array(StoreFullPathName)) );
                    }catch(const std::exception &e){
                        FUNCWARN("Difficulty encountered during dose array loading: '" << e.what() <<
                                 "'. Ignoring file and continuing");
                        //loaded_dose_storage.back().pop_back();
                        continue;
                    }

                }else{ //Image loading. 'CT' and 'MR' should work. Not sure about others.
                    try{
                        loaded_imgs_storage.back().push_back( std::move(Load_Image_Array(StoreFullPathName)) );
                    }catch(const std::exception &e){
                        FUNCWARN("Difficulty encountered during image array loading: '" << e.what() <<
                                 "'. Ignoring file and continuing");
                        //loaded_imgs_storage.back().pop_back();
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
 
                    //If we want to add any additional image metadata, or replace the default Imebra_Shim.cc populated metadata
                    // with, say, the non-null PostgreSQL metadata, it should be done here.
                    loaded_imgs_storage.back().back()->imagecoll.images.back().metadata["StoreFullPathName"] = StoreFullPathName;
                    if(!r1[i]["dt"].is_null()){
                        //DICOM_data.image_data.back()->imagecoll.images.back().metadata["dt"] = r1[i]["dt"].c_str();
                        //loaded_imgs_storage.back()->imagecoll.images.back().metadata["dt"] = r1[i]["dt"].c_str();
                        loaded_imgs_storage.back().back()->imagecoll.images.back().metadata["dt"] = r1[i]["dt"].c_str();
                    }
                    // ... more metadata operations ...
                }

                //Whatever file type, 
                if(!r1[i]["FrameofReferenceUID"].is_null()){
                    FrameofReferenceUIDs.insert(r1[i]["FrameofReferenceUID"].as<std::string>());
                }

            }

            //-------------------------------------------------------------------------------------------------------------
            //Finish the transaction and drop the connection.
            txn.commit();

        } // Loop over groups of query filter files.

    }catch(const std::exception &e){
        FUNCWARN("Exception caught: " << e.what());
        return false;
    }

    //Custom contour loading from an auxiliary database.
    if(!FrameofReferenceUIDs.empty()){
        try{
            pqxx::connection c(db_connection_params);
            pqxx::work txn(c);

            //Query for any contours matching the specific FrameofReferenceUID.
            std::stringstream ss;
            ss << "SELECT * FROM contours WHERE ";
            {
                bool first = true;
                for(const auto &FrameofReferenceUID : FrameofReferenceUIDs){
                    if(first){
                        first = false;
                        ss << "(FrameofReferenceUID = " << txn.quote(FrameofReferenceUID) << ") ";
                    }else{
                        ss << "OR (FrameofReferenceUID = " << txn.quote(FrameofReferenceUID) << ") ";
                    }
                }
            }
            ss << ";";
            pqxx::result res = txn.exec(ss.str());

            //Parse any matching contour collections. Store them for later. 
            for(pqxx::result::size_type i = 0; i != res.size(); ++i){
                const auto ROIName                 = res[i]["ROIName"].as<std::string>();
                const auto ContourCollectionString = res[i]["ContourCollectionString"].as<std::string>();
                const auto StudyInstanceUID        = res[i]["StudyInstanceUID"].as<std::string>();
                const auto FrameofReferenceUID     = res[i]["FrameofReferenceUID"].as<std::string>();
       
                 
                const auto keyA = std::make_pair(FrameofReferenceUID,StudyInstanceUID);
                contours_with_meta cc;
                if(!cc.load_from_string(ContourCollectionString)){
                    FUNCWARN("Unable to parse contour collection with ROIName '" << ROIName <<
                             "' and StudyInstanceUID '" << StudyInstanceUID << "'. Continuing");
                    continue;
                }else{
                    FUNCINFO("Loaded contour with StudyInstanceUID '" << StudyInstanceUID << "' and ROIName '" << ROIName << "'");

                    //Imbue the contours with their names and any other relevant metadata.
                    for(auto & contour : cc.contours){
                        contour.metadata["ROIName"] = ROIName;
                        contour.metadata["StudyInstanceUID"] = StudyInstanceUID;
                        contour.metadata["FrameofReferenceUID"] = FrameofReferenceUID;
                        // ...
                    }

                    // ---- Unmodified contours ----
                    //Pack into the group's existing contour collection.
                    loaded_contour_data_storage->ccs.emplace_back(cc);

                    /*
                    // ---- Modified contours ----
                    {
                      const auto CCCentroid = cc.Centroid();
                      const plane<double> slightlyaskewplane(vec3<double>(0.0,1.0,0.0), CCCentroid);
                      auto split_ccs = cc.Split_Along_Plane(slightlyaskewplane);
                     
                      for(auto & contour : split_ccs.front().contours){
                          contour.metadata["ROIName"] += "_POST"; 
                      }
                      for(auto & contour : split_ccs.back().contours){
                          contour.metadata["ROIName"] += "_ANT"; 
                      }
               
                      loaded_contour_data_storage->ccs.emplace_back(split_ccs.front());
                      loaded_contour_data_storage->ccs.emplace_back(split_ccs.back());
                    } */
                }
            }
        
            //No transaction needed. Read-only.
            //txn.commit();
        }catch(const std::exception &e){
            FUNCWARN("Unable to select contours: exception caught: " << e.what());
        }
    } //Loading custom contours from an auxiliary database.

    //-----------------------------------------------------------------------------------------------------------------

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
        auto combined = Concatenate_Contour_Data( std::move(DICOM_data.contour_data->Duplicate()),
                                                  std::move(loaded_contour_data_storage->Duplicate()) );
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
    FUNCINFO("Number of image set groups loaded = " << DICOM_data.image_data.size());

    for(auto &loaded_dose_set : loaded_dose_storage){
        if(loaded_dose_set.empty()) continue;

        //There are two options here, depending on what the user wishes to do: treat dose as a regular image, or as
        // special dose images. The more 'modern' way is to treat everything uniformly as images, but the old dose
        // computation methods require the distinction to be made. 

        // Option A: stuff the dose data into the Drover's Dose_Array.
        DICOM_data.dose_data.emplace_back( std::move(loaded_dose_set.back()) );

        // Option B: stuff the dose data into the Drover's Image_Array so it can be more easily used with image processing routines.
        //DICOM_data.image_data.emplace_back();
        //DICOM_data.image_data.back() = std::make_shared<Image_Array>(*(loaded_dose_set.back()));
    }

    return true;
}
