//Imebra_Shim.h.

#ifndef _IMEBRA_SHIM_H_DICOMAUTOMATON
#define _IMEBRA_SHIM_H_DICOMAUTOMATON

#include <stddef.h>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Structs.h"
#include "YgorContainers.h"  //Needed for bimap class.

class Contour_Data;
class Image_Array;

//------------------ General ----------------------
//One-offs.
std::string get_tag_as_string(const std::string &filename, size_t U, size_t L);
std::string get_modality(const std::string &filename);
std::string get_patient_ID(const std::string &filename);

//Mass top-level tag enumeration, for ingress into database.
//
//NOTE: May not be complete. Add additional tags as needed!
std::map<std::string,std::string> get_metadata_top_level_tags(const std::string &filename);


//------------------ Contours ---------------------
bimap<std::string,long int> get_ROI_tags_and_numbers(const std::string &filename);

std::unique_ptr<Contour_Data>  get_Contour_Data(const std::string &filename);


//-------------------- Images ----------------------
//This routine will often result in an array with only a single image. So collate output as needed.
std::unique_ptr<Image_Array> Load_Image_Array(const std::string &filename);

//These pointers will actually be unique. This just aims to convert from unique_ptr to shared_ptr for you.
std::list<std::shared_ptr<Image_Array>>  Load_Image_Arrays(const std::list<std::string> &filenames);

//Since many images must be loaded individually from a file, we will often have to collate them together.
std::unique_ptr<Image_Array> Collate_Image_Arrays(std::list<std::shared_ptr<Image_Array>> &in);


//--------------------- Dose -----------------------
std::unique_ptr<Image_Array> Load_Dose_Array(const std::string &filename);

//These pointers will actually be unique. This just aims to convert from unique_ptr to shared_ptr for you.
std::list<std::shared_ptr<Image_Array>>  Load_Dose_Arrays(const std::list<std::string> &filenames);

//-------------------- Plans ------------------------
std::unique_ptr<TPlan_Config> Load_TPlan_Config(const std::string &filename);

//-------------------- Export -----------------------
//Writes an Image_Array as if it were a dose matrix.
enum ParanoiaLevel {
    Low,      // Reuse many of the metadata tags.
    Medium,   // Forgo most of the metadata tags, but retain high-level linkage (e.g., PatientID).
    High      // Forgo all non-critical metadata tags. Note this should not be used for anonymization.
};

void Write_Dose_Array(std::shared_ptr<Image_Array> IA, 
                      const std::string &FilenameOut, 
                      ParanoiaLevel Paranoia = ParanoiaLevel::Low);

// Note: callback will be called once for each CT-modality DICOM file.
void Write_CT_Images(std::shared_ptr<Image_Array> IA, 
                     std::function<void(std::istream &is,
                                        long int filesize)> file_handler,
                     ParanoiaLevel Paranoia = ParanoiaLevel::Low);

void Write_Contours(std::list<std::reference_wrapper<contour_collection<double>>> CC,
                    std::function<void(std::istream &is,
                                       long int filesize)> file_handler,
                    ParanoiaLevel Paranoia = ParanoiaLevel::Low);

#endif
