//Imebra_Shim.cc - DICOMautomaton 2012-2013. Written by hal clark.
//
//This file is supposed to be a kind of 'shim' or 'wrapper' around the Imebra library.
//
//Why is it needed? **Strictly for convenice.** Compilation of the Imembra library requires
// a lot of time, and the library must be compiled without linking parts of code which 
// use Imebra stuff. This is not a design flaw, but it is an inconvenience when one 
// wants to (write) (compile) (test), (write) (compile) (test), etc.. because it becomes
// more like (write) (       c   o   m   p   i   l   e       ) (test), etc..
//
//NOTE: that we do not properly handle unicode. Everything is stuffed into a std::string.
//

#include <iostream>
#include <vector>
#include <string>
#include <utility>        //Needed for std::pair.
#include <memory>         //Needed for std::unique_ptr.
#include <algorithm>      //Needed for std::sort.
#include <list>
#include <tuple>
#include <map>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
#include "imebra.h"
#pragma GCC diagnostic pop

#include "YgorMisc.h"       //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"       //Needed for 'vec3' class.
#include "YgorString.h"     //Needed for Canonicalize_String2().
#include "YgorContainers.h" //Needed for 'bimap' class.

#include "Structs.h"
#include "Imebra_Shim.h"

//------------------ General ----------------------
//This is used to grab the contents of a single DICOM tag. It can be used for whatever. Some routines
// use it to grab specific things. Each invocation involves disk access and file parsing.
//
//NOTE: On error, the output will be an empty string.
std::string get_tag_as_string(const std::string &filename, size_t U, size_t L){
    using namespace puntoexe;
    ptr<puntoexe::stream> readStream(new puntoexe::stream);
    readStream->openFile(filename.c_str(), std::ios::in);
    if(readStream == nullptr) return std::string("");

    ptr<puntoexe::streamReader> reader(new puntoexe::streamReader(readStream));
    ptr<imebra::dataSet> TopDataSet = imebra::codecs::codecFactory::getCodecFactory()->load(reader);
    if(TopDataSet == nullptr) return std::string("");
    return TopDataSet->getString(U, 0, L, 0);
}

std::string get_modality(const std::string &filename){
    //Should exist in each DICOM file.
    return get_tag_as_string(filename,0x0008,0x0060);
}

std::string get_patient_ID(const std::string &filename){
    //Should exist in each DICOM file.
    return get_tag_as_string(filename,0x0010,0x0020);
}

//------------------ General ----------------------
//Mass top-level tag enumeration, for ingress into database.
//
//NOTE: May not be complete. Add additional tags as needed!
std::map<std::string,std::string> get_metadata_top_level_tags(const std::string &filename){
    std::map<std::string,std::string> out;
    const auto ctrim = CANONICALIZE::TRIM_ENDS;

    //Attempt to parse the DICOM file and harvest the elements of interest. We are only interested in
    // top-level elements specifying metadata (i.e., not pixel data) and will not need to recurse into 
    // any DICOM sequences.
    puntoexe::ptr<puntoexe::stream> readStream(new puntoexe::stream);
    readStream->openFile(filename.c_str(), std::ios::in);
    if(readStream == nullptr){
        FUNCWARN("Could not parse file '" << filename << "'. Is it valid DICOM? Cannot continue");
        return std::move(out);
    }

    puntoexe::ptr<puntoexe::streamReader> reader(new puntoexe::streamReader(readStream));
    puntoexe::ptr<puntoexe::imebra::dataSet> tds = puntoexe::imebra::codecs::codecFactory::getCodecFactory()->load(reader);

    //We pull out all the data we need as strings. For single element strings, the SQL engine can directly perform
    // the type casting. The benefit of this is twofold: (1) the SQL engine hides the checking code, simplifying
    // this implementation, and (2) the SQL engine can appropriately handle SQL NULLs without extra conditionals
    // and workarounds here.
    //
    // Multi-element data are a little trickier, especially with Imebra apparently unable to get the whole element
    // in the raw DICOM representation as string. We break these items into individual elements and then delimit 
    // such that they appear just as they would if read from the DICOM file (with '\'s separating) as needed.
    //
    // To ensure that there are no duplicated tags (at this single level) we exclusively use 'emplace'.
    //
    // TODO: Properly handle missing strings. In many cases, Imebra will feel obligated to return a NON-EMPTY string
    //       if a tag is missing. For example, the date '0000-00-00' is returned instead of an empty string for date
    //       fields. Instead of handling boneheaded practices like this, use the Imebra member function
    //           ptr< data > puntoexe::imebra::dataSet::getTag(imbxUint16 groupId, imbxUint16 order,
    //                                                         imbxUint16 tagId, bool bCreate = false)
    //       to check if the tag is present before asking for a string. Then, of course, we need to check later if
    //       the item is present. If using a map, I think the default empty string will suffice to communicate this
    //       info.
     
    //DICOM logical hierarchy fields.
    out.emplace("PatientID"                    , Canonicalize_String2(tds->getString(0x0010, 0, 0x0020, 0), ctrim));
    out.emplace("StudyInstanceUID"             , Canonicalize_String2(tds->getString(0x0020, 0, 0x000D, 0), ctrim));
    out.emplace("SeriesInstanceUID"            , Canonicalize_String2(tds->getString(0x0020, 0, 0x000E, 0), ctrim));
    out.emplace("SOPInstanceUID"               , Canonicalize_String2(tds->getString(0x0008, 0, 0x0018, 0), ctrim));

    //DICOM data collection, additional or fallback linkage metadata.
    out.emplace("InstanceNumber"               , Canonicalize_String2(tds->getString(0x0020, 0, 0x0013, 0), ctrim));
    out.emplace("InstanceCreationDate"         , Canonicalize_String2(tds->getString(0x0008, 0, 0x0012, 0), ctrim));
    out.emplace("InstanceCreationTime"         , Canonicalize_String2(tds->getString(0x0008, 0, 0x0013, 0), ctrim));

    out.emplace("StudyDate"                    , Canonicalize_String2(tds->getString(0x0008, 0, 0x0020, 0), ctrim));
    out.emplace("StudyTime"                    , Canonicalize_String2(tds->getString(0x0008, 0, 0x0030, 0), ctrim));
    out.emplace("StudyID"                      , Canonicalize_String2(tds->getString(0x0020, 0, 0x0010, 0), ctrim));
    out.emplace("StudyDescription"             , Canonicalize_String2(tds->getString(0x0008, 0, 0x1030, 0), ctrim));

    out.emplace("SeriesDate"                   , Canonicalize_String2(tds->getString(0x0008, 0, 0x0021, 0), ctrim));
    out.emplace("SeriesTime"                   , Canonicalize_String2(tds->getString(0x0008, 0, 0x0031, 0), ctrim));
    out.emplace("SeriesNumber"                 , Canonicalize_String2(tds->getString(0x0020, 0, 0x0011, 0), ctrim));
    out.emplace("SeriesDescription"            , Canonicalize_String2(tds->getString(0x0008, 0, 0x103e, 0), ctrim));

    out.emplace("AcquisitionDate"              , Canonicalize_String2(tds->getString(0x0008, 0, 0x0022, 0), ctrim));
    out.emplace("AcquisitionTime"              , Canonicalize_String2(tds->getString(0x0008, 0, 0x0032, 0), ctrim));
    out.emplace("AcquisitionNumber"            , Canonicalize_String2(tds->getString(0x0020, 0, 0x0012, 0), ctrim));

    out.emplace("ContentDate"                  , Canonicalize_String2(tds->getString(0x0008, 0, 0x0023, 0), ctrim));
    out.emplace("ContentTime"                  , Canonicalize_String2(tds->getString(0x0008, 0, 0x0033, 0), ctrim));

    out.emplace("BodyPartExamined"             , Canonicalize_String2(tds->getString(0x0018, 0, 0x0015, 0), ctrim));
    out.emplace("ScanningSequence"             , Canonicalize_String2(tds->getString(0x0018, 0, 0x0020, 0), ctrim));
    out.emplace("SequenceVariant"              , Canonicalize_String2(tds->getString(0x0018, 0, 0x0021, 0), ctrim));
    out.emplace("ScanOptions"                  , Canonicalize_String2(tds->getString(0x0018, 0, 0x0022, 0), ctrim));
    out.emplace("MRAcquisitionType"            , Canonicalize_String2(tds->getString(0x0018, 0, 0x0023, 0), ctrim));



    //DICOM image, dose map specifications and metadata.
    out.emplace("SliceThickness"               , Canonicalize_String2(tds->getString(0x0018, 0, 0x0050, 0), ctrim));
    out.emplace("SliceNumber"                  , Canonicalize_String2(tds->getString(0x2001, 0, 0x100a, 0), ctrim));
    out.emplace("SliceLocation"                , Canonicalize_String2(tds->getString(0x0020, 0, 0x1041, 0), ctrim));

    out.emplace("ImageIndex"                   , Canonicalize_String2(tds->getString(0x0054, 0, 0x1330, 0), ctrim));

    out.emplace("SpacingBetweenSlices"         , Canonicalize_String2(tds->getString(0x0018, 0, 0x0088, 0), ctrim));

    out.emplace("ImagePositionPatient"         , Canonicalize_String2(tds->getString(0x0020, 0, 0x0032, 0), ctrim) + "\\"_s +
                                                 Canonicalize_String2(tds->getString(0x0020, 0, 0x0032, 1), ctrim) + "\\"_s +
                                                 Canonicalize_String2(tds->getString(0x0020, 0, 0x0032, 2), ctrim) + "\\"_s );
    //out.emplace("ImagePositionPatient"         , Canonicalize_String2(tds->getString(0x0020, 0, 0x0032, 0), ctrim));

    out.emplace("ImageOrientationPatient"      , Canonicalize_String2(tds->getString(0x0020, 0, 0x0037, 0), ctrim) + "\\"_s +
                                                 Canonicalize_String2(tds->getString(0x0020, 0, 0x0037, 1), ctrim) + "\\"_s +
                                                 Canonicalize_String2(tds->getString(0x0020, 0, 0x0037, 2), ctrim) + "\\"_s +
                                                 Canonicalize_String2(tds->getString(0x0020, 0, 0x0037, 3), ctrim) + "\\"_s +
                                                 Canonicalize_String2(tds->getString(0x0020, 0, 0x0037, 4), ctrim) + "\\"_s +
                                                 Canonicalize_String2(tds->getString(0x0020, 0, 0x0037, 5), ctrim) );
    //out.emplace("ImageOrientationPatient"      , Canonicalize_String2(tds->getString(0x0020, 0, 0x0037, 0), ctrim));

    out.emplace("FrameofReferenceUID"          , Canonicalize_String2(tds->getString(0x0020, 0, 0x0052, 0), ctrim));
    out.emplace("PositionReferenceIndicator"   , Canonicalize_String2(tds->getString(0x0020, 0, 0x1040, 0), ctrim));
    out.emplace("SamplesPerPixel"              , Canonicalize_String2(tds->getString(0x0028, 0, 0x0002, 0), ctrim));
    out.emplace("PhotometricInterpretation"    , Canonicalize_String2(tds->getString(0x0028, 0, 0x0004, 0), ctrim));

    out.emplace("NumberofFrames"               , Canonicalize_String2(tds->getString(0x0028, 0, 0x0008, 0), ctrim));
    out.emplace("FrameIncrementPointer"        , Canonicalize_String2(tds->getString(0x0028, 0, 0x0009, 0), ctrim)); 
    out.emplace("Rows"                         , Canonicalize_String2(tds->getString(0x0028, 0, 0x0010, 0), ctrim));
    out.emplace("Columns"                      , Canonicalize_String2(tds->getString(0x0028, 0, 0x0011, 0), ctrim));

    out.emplace("PixelSpacing"                 , Canonicalize_String2(tds->getString(0x0028, 0, 0x0030, 0), ctrim) + "\\"_s +
                                                 Canonicalize_String2(tds->getString(0x0028, 0, 0x0030, 1), ctrim) );
    //out.emplace("PixelSpacing"                 , Canonicalize_String2(tds->getString(0x0028, 0, 0x0030, 0), ctrim));

    out.emplace("BitsAllocated"                , Canonicalize_String2(tds->getString(0x0028, 0, 0x0100, 0), ctrim));
    out.emplace("BitsStored"                   , Canonicalize_String2(tds->getString(0x0028, 0, 0x0101, 0), ctrim));
    out.emplace("HighBit"                      , Canonicalize_String2(tds->getString(0x0028, 0, 0x0102, 0), ctrim));
    out.emplace("PixelRepresentation"          , Canonicalize_String2(tds->getString(0x0028, 0, 0x0103, 0), ctrim));

    out.emplace("DoseUnits"                    , Canonicalize_String2(tds->getString(0x3004, 0, 0x0002, 0), ctrim));
    out.emplace("DoseType"                     , Canonicalize_String2(tds->getString(0x3004, 0, 0x0004, 0), ctrim));
    out.emplace("DoseSummationType"            , Canonicalize_String2(tds->getString(0x3004, 0, 0x000a, 0), ctrim));
    out.emplace("DoseGridScaling"              , Canonicalize_String2(tds->getString(0x3004, 0, 0x000e, 0), ctrim));

    std::string GridFrameOffsetVectorTEMP;
    {
      long int frame=0;
      while(true){
          try{
              const auto anumber = Canonicalize_String2(tds->getString(0x3004, 0, 0x000c, frame), ctrim);
              if(frame != 0) GridFrameOffsetVectorTEMP += "\\";
              GridFrameOffsetVectorTEMP += anumber;
              ++frame;
              if(anumber.empty()) break;
          }catch(const std::exception &e){
              break;
          }
      }
    }
    out.emplace("GridFrameOffsetVector"        , GridFrameOffsetVectorTEMP);;
    //out.emplace("GridFrameOffsetVector"        , Canonicalize_String2(tds->getString(0x3004, 0, 0x000c, 0), ctrim));


    out.emplace("TemporalPositionIdentifier"   , Canonicalize_String2(tds->getString(0x0020, 0, 0x0100, 0), ctrim));
    out.emplace("NumberofTemporalPositions"    , Canonicalize_String2(tds->getString(0x0020, 0, 0x0105, 0), ctrim));

    out.emplace("TemporalResolution"           , Canonicalize_String2(tds->getString(0x0020, 0, 0x0110, 0), ctrim));

    out.emplace("TemporalPositionIndex"        , Canonicalize_String2(tds->getString(0x0020, 0, 0x9128, 0), ctrim));

    out.emplace("FrameReferenceTime"           , Canonicalize_String2(tds->getString(0x0054, 0, 0x1300, 0), ctrim));

    out.emplace("FrameTime"                    , Canonicalize_String2(tds->getString(0x0018, 0, 0x1063, 0), ctrim));

    out.emplace("TriggerTime"                  , Canonicalize_String2(tds->getString(0x0018, 0, 0x1060, 0), ctrim));
    out.emplace("TriggerTimeOffset"            , Canonicalize_String2(tds->getString(0x0018, 0, 0x1069, 0), ctrim));

    out.emplace("PerformedProcedureStepStartDate", Canonicalize_String2(tds->getString(0x0040, 0, 0x0244, 0), ctrim));
    out.emplace("PerformedProcedureStepStartTime", Canonicalize_String2(tds->getString(0x0040, 0, 0x0245, 0), ctrim));
    out.emplace("PerformedProcedureStepEndDate",   Canonicalize_String2(tds->getString(0x0040, 0, 0x0250, 0), ctrim));
    out.emplace("PerformedProcedureStepEndTime",   Canonicalize_String2(tds->getString(0x0040, 0, 0x0251, 0), ctrim));

    out.emplace("Exposure"                     , Canonicalize_String2(tds->getString(0x0018, 0, 0x1152, 0), ctrim));
    out.emplace("ExposureTime"                 , Canonicalize_String2(tds->getString(0x0018, 0, 0x1150, 0), ctrim));
    out.emplace("ExposureInMicroAmpereSeconds" , Canonicalize_String2(tds->getString(0x0018, 0, 0x1153, 0), ctrim));
    out.emplace("XRayTubeCurrent"              , Canonicalize_String2(tds->getString(0x0018, 0, 0x1151, 0), ctrim));


    out.emplace("RepetitionTime"               , Canonicalize_String2(tds->getString(0x0018, 0, 0x0080, 0), ctrim));
    out.emplace("EchoTime"                     , Canonicalize_String2(tds->getString(0x0018, 0, 0x0081, 0), ctrim));
    out.emplace("NumberofAverages"             , Canonicalize_String2(tds->getString(0x0018, 0, 0x0083, 0), ctrim));
    out.emplace("ImagingFrequency"             , Canonicalize_String2(tds->getString(0x0018, 0, 0x0084, 0), ctrim));
    out.emplace("ImagedNucleus"                , Canonicalize_String2(tds->getString(0x0018, 0, 0x0085, 0), ctrim));
    out.emplace("EchoNumbers"                  , Canonicalize_String2(tds->getString(0x0018, 0, 0x0086, 0), ctrim));
    out.emplace("MagneticFieldStrength"        , Canonicalize_String2(tds->getString(0x0018, 0, 0x0087, 0), ctrim));
    out.emplace("NumberofPhaseEncodingSteps"   , Canonicalize_String2(tds->getString(0x0018, 0, 0x0089, 0), ctrim));
    out.emplace("EchoTrainLength"              , Canonicalize_String2(tds->getString(0x0018, 0, 0x0091, 0), ctrim));
    out.emplace("PercentSampling"              , Canonicalize_String2(tds->getString(0x0018, 0, 0x0093, 0), ctrim));
    out.emplace("PercentPhaseFieldofView"      , Canonicalize_String2(tds->getString(0x0018, 0, 0x0094, 0), ctrim));
    out.emplace("PixelBandwidth"               , Canonicalize_String2(tds->getString(0x0018, 0, 0x0095, 0), ctrim));
    out.emplace("DeviceSerialNumber"           , Canonicalize_String2(tds->getString(0x0018, 0, 0x1000, 0), ctrim));

    out.emplace("ProtocolName"                 , Canonicalize_String2(tds->getString(0x0018, 0, 0x1030, 0), ctrim));

    out.emplace("ReceiveCoilName"              , Canonicalize_String2(tds->getString(0x0018, 0, 0x1250, 0), ctrim));
    out.emplace("TransmitCoilName"             , Canonicalize_String2(tds->getString(0x0018, 0, 0x1251, 0), ctrim));
    out.emplace("InplanePhaseEncodingDirection", Canonicalize_String2(tds->getString(0x0018, 0, 0x1312, 0), ctrim));
    out.emplace("FlipAngle"                    , Canonicalize_String2(tds->getString(0x0018, 0, 0x1314, 0), ctrim));
    out.emplace("SAR"                          , Canonicalize_String2(tds->getString(0x0018, 0, 0x1316, 0), ctrim));
    out.emplace("dB_dt"                        , Canonicalize_String2(tds->getString(0x0018, 0, 0x1318, 0), ctrim));
    out.emplace("PatientPosition"              , Canonicalize_String2(tds->getString(0x0018, 0, 0x5100, 0), ctrim));
    out.emplace("AcquisitionDuration"          , Canonicalize_String2(tds->getString(0x0018, 0, 0x9073, 0), ctrim));
    out.emplace("Diffusion_bValue"             , Canonicalize_String2(tds->getString(0x0018, 0, 0x9087, 0), ctrim));
    out.emplace("DiffusionGradientOrientation" , Canonicalize_String2(tds->getString(0x0018, 0, 0x9089, 0), ctrim) + "\\"_s +
                                                 Canonicalize_String2(tds->getString(0x0018, 0, 0x9089, 1), ctrim) + "\\"_s +
                                                 Canonicalize_String2(tds->getString(0x0018, 0, 0x9089, 2), ctrim) );

    out.emplace("DiffusionDirection"           , Canonicalize_String2(tds->getString(0x2001, 0, 0x1004, 0), ctrim));

    out.emplace("WindowCenter"                 , Canonicalize_String2(tds->getString(0x0028, 0, 0x1050, 0), ctrim));
    out.emplace("WindowWidth"                  , Canonicalize_String2(tds->getString(0x0028, 0, 0x1051, 0), ctrim));
    out.emplace("RescaleIntercept"             , Canonicalize_String2(tds->getString(0x0028, 0, 0x1052, 0), ctrim));
    out.emplace("RescaleSlope"                 , Canonicalize_String2(tds->getString(0x0028, 0, 0x1053, 0), ctrim));
    out.emplace("RescaleType"                  , Canonicalize_String2(tds->getString(0x0028, 0, 0x1054, 0), ctrim));

    //DICOM radiotherapy plan metadata.
    out.emplace("RTPlanLabel"                  , Canonicalize_String2(tds->getString(0x300a, 0, 0x0002, 0), ctrim));
    out.emplace("RTPlanName"                   , Canonicalize_String2(tds->getString(0x300a, 0, 0x0003, 0), ctrim));
    out.emplace("RTPlanDescription"            , Canonicalize_String2(tds->getString(0x300a, 0, 0x0004, 0), ctrim));
    out.emplace("RTPlanDate"                   , Canonicalize_String2(tds->getString(0x300a, 0, 0x0006, 0), ctrim));
    out.emplace("RTPlanTime"                   , Canonicalize_String2(tds->getString(0x300a, 0, 0x0007, 0), ctrim));
    out.emplace("RTPlanGeometry"               , Canonicalize_String2(tds->getString(0x300a, 0, 0x000c, 0), ctrim));

    //DICOM patient, physician, operator metadata.
    out.emplace("PatientsName"                 , Canonicalize_String2(tds->getString(0x0010, 0, 0x0010, 0), ctrim));
    out.emplace("PatientsBirthDate"            , Canonicalize_String2(tds->getString(0x0010, 0, 0x0030, 0), ctrim));
    out.emplace("PatientsGender"               , Canonicalize_String2(tds->getString(0x0010, 0, 0x0040, 0), ctrim));
    out.emplace("PatientsWeight"               , Canonicalize_String2(tds->getString(0x0010, 0, 0x1030, 0), ctrim));

    out.emplace("OperatorsName"                , Canonicalize_String2(tds->getString(0x0008, 0, 0x1070, 0), ctrim));

    out.emplace("ReferringPhysicianName"       , Canonicalize_String2(tds->getString(0x0008, 0, 0x0090, 0), ctrim));

    //DICOM categorical fields.
    out.emplace("SOPClassUID"                  , Canonicalize_String2(tds->getString(0x0008, 0, 0x0016, 0), ctrim));
    out.emplace("Modality"                     , Canonicalize_String2(tds->getString(0x0008, 0, 0x0060, 0), ctrim));

    //DICOM machine/device, institution fields.
    out.emplace("Manufacturer"                 , Canonicalize_String2(tds->getString(0x0008, 0, 0x0070, 0), ctrim));
    out.emplace("StationName"                  , Canonicalize_String2(tds->getString(0x0008, 0, 0x1010, 0), ctrim));
    out.emplace("ManufacturersModelName"       , Canonicalize_String2(tds->getString(0x0008, 0, 0x1090, 0), ctrim));
    out.emplace("SoftwareVersions"             , Canonicalize_String2(tds->getString(0x0018, 0, 0x1020, 0), ctrim));

    out.emplace("InstitutionName"              , Canonicalize_String2(tds->getString(0x0008, 0, 0x0080, 0), ctrim));
    out.emplace("InstitutionalDepartmentName"  , Canonicalize_String2(tds->getString(0x0008, 0, 0x1040, 0), ctrim));

    return std::move(out);
}



//------------------ Contours ---------------------

//Returns a bimap with the (raw) ROI tags and their corresponding ROI numbers. The ROI numbers are
// arbitrary identifiers used within the DICOM file to identify contours more conveniently.
bimap<std::string,long int> get_ROI_tags_and_numbers(const std::string &FilenameIn){
    using namespace puntoexe;
    ptr<puntoexe::stream> readStream(new puntoexe::stream);
    readStream->openFile(FilenameIn.c_str(), std::ios::in);

    ptr<puntoexe::streamReader> reader(new puntoexe::streamReader(readStream));
    ptr<imebra::dataSet> TopDataSet = imebra::codecs::codecFactory::getCodecFactory()->load(reader);
    ptr<imebra::dataSet> SecondDataSet;

    size_t i=0, j;
    std::string ROI_name;
    long int ROI_number;
    bimap<std::string,long int> the_pairs;
 
    do{
         //See gdcmdump output of an RS file  OR  Dicom documentation for these
         // numbers. 0x3006,0x0020 defines the top-level ROI sequence. 0x3006,0x0026
         // defines the name for each item (of which there might be many with the same number..)
         SecondDataSet = TopDataSet->getSequenceItem(0x3006, 0, 0x0020, i);
         if(SecondDataSet != nullptr){
            //Loop over all items within this data set. There should not be more than one, but I am 
            // suspect of 'data from the wild.'
            j=0;
            do{
                ROI_name   = SecondDataSet->getString(0x3006, 0, 0x0026, j);
                ROI_number = static_cast<long int>(SecondDataSet->getSignedLong(0x3006, 0, 0x0022, j));
                if(ROI_name.size()){
                    the_pairs[ROI_number] = ROI_name;
                }
                ++j;
            }while((ROI_name.size() != 0));
        }

        ++i;
    }while(SecondDataSet != NULL);
    return the_pairs;
}


//Returns contour data from a DICOM RS file sorted into organ-specific collections.
std::unique_ptr<Contour_Data> get_Contour_Data(const std::string &filename){
    std::unique_ptr<Contour_Data> output (new Contour_Data());
    bimap<std::string,long int> tags_names_and_numbers = get_ROI_tags_and_numbers(filename);

    auto FileMetadata = get_metadata_top_level_tags(filename);


    using namespace puntoexe;
    ptr<puntoexe::stream> readStream(new puntoexe::stream);
    readStream->openFile(filename.c_str(), std::ios::in);
    ptr<puntoexe::streamReader> reader(new puntoexe::streamReader(readStream));
    ptr<imebra::dataSet> TopDataSet = imebra::codecs::codecFactory::getCodecFactory()->load(reader);
    ptr<imebra::dataSet> SecondDataSet, ThirdDataSet;

    //Collect the data into a container of contours with meta info. It may be unordered (within the file).
    std::map<std::tuple<std::string,long int>, contour_collection<double>> mapcache;
    for(size_t i=0; (SecondDataSet = TopDataSet->getSequenceItem(0x3006, 0, 0x0039, i)) != nullptr; ++i){
        long int Last_ROI_Numb = 0;
        for(size_t j=0; (ThirdDataSet = SecondDataSet->getSequenceItem(0x3006, 0, 0x0040, j)) != nullptr; ++j){
            long int ROI_number = static_cast<long int>(SecondDataSet->getSignedLong(0x3006, 0, 0x0084, j));
            if(ROI_number == 0){
                ROI_number = Last_ROI_Numb;
            }else{
                Last_ROI_Numb = ROI_number;
            }

            if((ThirdDataSet = SecondDataSet->getSequenceItem(0x3006, 0, 0x0040, j)) == nullptr){
                continue;
            }

            ptr<puntoexe::imebra::handlers::dataHandler> the_data_handler;
            for(size_t k=0; (the_data_handler = ThirdDataSet->getDataHandler(0x3006, 0, 0x0050, k, false)) != nullptr; ++k){
                contour_of_points<double> shtl;
                shtl.closed = true;

                //This is the number of coordinates we will get (ie. the number of doubles).
                const long int numb_of_coordinates = the_data_handler->getSize();
                for(long int N = 0; N < numb_of_coordinates; N += 3){
                    const double x = the_data_handler->getDouble(N + 0);
                    const double y = the_data_handler->getDouble(N + 1);
                    const double z = the_data_handler->getDouble(N + 2);
                    shtl.points.push_back(vec3<double>(x,y,z));
                }
                shtl.Reorient_Counter_Clockwise();
                shtl.metadata = FileMetadata;

                auto ROIName = tags_names_and_numbers[ROI_number];
                shtl.metadata["ROINumber"] = std::to_string(ROI_number);
                shtl.metadata["ROIName"] = ROIName;
                
                const auto key = std::make_tuple(ROIName, ROI_number);
                mapcache[key].contours.push_back(std::move(shtl));
            }
        }
    }


    //Now sort the contours into contour_with_metas. We sort based on ROI number.
    for(auto m_it = mapcache.begin(); m_it != mapcache.end(); ++m_it){
        output->ccs.push_back( contours_with_meta() ); //std::move(m_it->second) ) );
        output->ccs.back() = m_it->second; 

        output->ccs.back().Raw_ROI_name       = std::get<0>(m_it->first);
        output->ccs.back().ROI_number         = std::get<1>(m_it->first);
        output->ccs.back().Minimum_Separation = -1.0; //min_spacing;
        //output->ccs.back().Segmentation_History = ...empty...;
    }

    //Find the minimum separation between contours (which isn't zero).
    double min_spacing = 1E30;
    for(auto cc_it = output->ccs.begin(); cc_it != output->ccs.end(); ++cc_it){ 
        if(cc_it->contours.size() < 2) continue;

        for(auto c1_it = cc_it->contours.begin(); c1_it != --(cc_it->contours.end()); ++c1_it){
            auto c2_it = c1_it;
            ++c2_it;

            const double height1 = c1_it->Average_Point().Dot(vec3<double>(0.0,0.0,1.0));
            const double height2 = c2_it->Average_Point().Dot(vec3<double>(0.0,0.0,1.0));
            const double spacing = YGORABS(height2-height1);

            if((spacing < min_spacing) && (spacing > 1E-3)) min_spacing = spacing;
        }
    }
    //FUNCINFO("The minimum spacing found was " << min_spacing);
    for(auto cc_it = output->ccs.begin(); cc_it != output->ccs.end(); ++cc_it){
        cc_it->Minimum_Separation = min_spacing;
        for(auto & cc : cc_it->contours) cc.metadata["MinimumSeparation"] = std::to_string(min_spacing);
//        output->ccs.back().metadata["MinimumSeparation"] = std::to_string(min_spacing);
    }

    return std::move(output);
}


//-------------------- Images ----------------------
//This routine will often result in an array with only a single image. So collate output as needed.
//
// NOTE: I believe this routine is only valid for single frame images, like common CT and MR images.
//       PT and US have not been tested. RTDOSE files should use the Load_Dose_Array code, which 
//       handles multi-frame images (and thus might be adaptable for other non-RTDOSE multi-frame 
//       images).
std::unique_ptr<Image_Array> Load_Image_Array(const std::string &FilenameIn){
    std::unique_ptr<Image_Array> out(new Image_Array());

    out->filename   = FilenameIn;

    using namespace puntoexe;
    ptr<puntoexe::stream> readStream(new puntoexe::stream);
    readStream->openFile(FilenameIn.c_str(), std::ios::in);

    ptr<puntoexe::streamReader> reader(new puntoexe::streamReader(readStream));
    ptr<imebra::dataSet> TopDataSet = imebra::codecs::codecFactory::getCodecFactory()->load(reader);

    // --------------------------------------- Sort key ordering ---------------------------------------------
    //Using the InstanceNumber as the default sort key will (should?) guarantee that the key is unique,
    // but will not necessarily sort in the specific order you want! Think about MR where the various
    // slices can be collected in interleaved ways, or if the order is localized for easy cache access
    // in the DICOM writer.
    const auto instance_number  = static_cast<int64_t>(TopDataSet->getUnsignedLong(0x0020, 0, 0x0013, 0)); // "InstanceNumber"

    //This number is not necessarily unique to a series. For 4D images, the slice number can be shared by
    // slices with the same geometrical specifications, but differing temporal specifications.
    const auto slice_number     = static_cast<int64_t>(TopDataSet->getUnsignedLong(0x2001, 0, 0x100a, 0)); // "SliceNumber"

    //ImageIndex: An index identifying the position of this image within a PET Series.
    const auto image_index      = static_cast<int64_t>(TopDataSet->getUnsignedLong(0x0054, 0, 0x1330, 0)); // "ImageIndex"

    int64_t sort_key_A = 0; 
    int64_t sort_key_B = 0; 

    if(slice_number > 0){
        sort_key_A = slice_number;
    }else if(image_index > 0){
        sort_key_A = image_index;
    }
    sort_key_B = instance_number;

    // -------------------------------------- Temporal ordering ---------------------------------------------
    const auto content_date = TopDataSet->getString(0x0008, 0, 0x0023, 0); //e.g., "20150228" as a string.
    const auto content_time = TopDataSet->getString(0x0008, 0, 0x0033, 0); //e.g., "142301" as a string.

    const auto temporal_pos_indc = static_cast<int64_t>(TopDataSet->getUnsignedLong(0x0020, 0, 0x0100, 0)); // "TemporalPositionIdentifier"
    const auto temporal_max_numb = static_cast<int64_t>(TopDataSet->getUnsignedLong(0x0020, 0, 0x0105, 0)); // "NumberOfTemporalPositions"

    //Appears rather similar in concept to the "TemporalPositionIdentifier".
    const auto temporal_pos_indx = static_cast<int64_t>(TopDataSet->getUnsignedLong(0x0020, 0, 0x9128, 0)); // "TemporalPositionIndex"

    //Time when PET image pixel values 'occurred', in milliseconds since the SeriesReferenceTime.
    const auto frame_ref_time = static_cast<int64_t>(TopDataSet->getUnsignedLong(0x0054, 0, 0x1300, 0)); // "FrameReferenceTime"

    //Nominal time taken per frame.
    const auto frame_nom_time = static_cast<int64_t>(TopDataSet->getUnsignedLong(0x0018, 0, 0x1063, 0)); // "FrameTime" 

    //This number appears to be used for PET-CT scans where to denote consecutive scans. It is similar to
    // the 'TemporalPositionIdentifier' but does not have an accompanying 'NumberOfTemporalPositions' for 
    // scaling.
    const auto acquisition_number = static_cast<int64_t>(TopDataSet->getUnsignedLong(0x0020, 0, 0x0012, 0)); // "AcquisitionNumber"

    //These appear to refer to the moment the DICOM data was assembled or transmission began. Not terribly
    // useful for anything like a keyframe.
    //(0008,0012) DA [20150225]                                     # 8,1 Instance Creation Date
    //(0008,0013) TM [142116]                                       # 6,1 Instance Creation Time

    //These would be preferred to content date and time, but the only place I've seen them present
    // is on a Philips scanner that just set them equal to the study date and time. What a shame!
    //(0040,0244) DA [20150225]                                         # 8,1 Performed Procedure Step Start Date
    //(0040,0245) TM [125151]                                           # 6,1 Performed Procedure Step Start Time
    //(0040,0250) DA [20150225]                                         # 8,1 Performed Procedure Step End Date
    //(0040,0251) TM [125151]                                           # 6,1 Performed Procedure Step End Time

    double start_time = 0.0;
    double end_time = 0.0;
    if(frame_ref_time > 0){
        start_time = static_cast<double>(frame_ref_time);
        end_time   = start_time + static_cast<double>(frame_nom_time);
    }else if((temporal_max_numb > 0) && (temporal_pos_indc >= 0)){
        start_time = static_cast<double>(temporal_pos_indc)/static_cast<double>(temporal_max_numb);
        end_time   = (static_cast<double>(temporal_pos_indc)+0.5)/static_cast<double>(temporal_max_numb);

    }else if(temporal_pos_indx > 0){
        start_time = static_cast<double>(temporal_pos_indx);
        end_time   = start_time + static_cast<double>(frame_nom_time);
        //end_time   = static_cast<double>(temporal_pos_indx) + 0.5;

    }else if(acquisition_number > 0){
        start_time = static_cast<double>(acquisition_number);
        end_time   = start_time + static_cast<double>(frame_nom_time);
        //end_time   = static_cast<double>(acquisition_number) + 0.5;
    }
    //Try playing with dates and times if the above is not sufficient.

    // ---------------------------------------- Image Metadata ----------------------------------------------
    const auto slice_thickness  = static_cast<double>(TopDataSet->getDouble(0x0018, 0, 0x0050, 0));
    const auto slice_height     = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x1041, 0));

    //These should exist in all files. They appear to be the same for CT and DS files of the same set. Not sure
    // if this is *always* the case.
    const auto image_pos_x = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0032, 0)); //"ImagePositionPatient".
    const auto image_pos_y = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0032, 1));
    const auto image_pos_z = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0032, 2));
    const vec3<double> image_pos(image_pos_x,image_pos_y,image_pos_z); //Only for first image!

    const auto image_orien_c_x = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 0)); 
    const auto image_orien_c_y = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 1));
    const auto image_orien_c_z = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 2));
    const vec3<double> image_orien_c = vec3<double>(image_orien_c_x,image_orien_c_y,image_orien_c_z).unit();

    const auto image_orien_r_x = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 3));
    const auto image_orien_r_y = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 4));
    const auto image_orien_r_z = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 5));
    const vec3<double> image_orien_r = vec3<double>(image_orien_r_x,image_orien_r_y,image_orien_r_z).unit();

    const vec3<double> image_anchor  = vec3<double>(0.0,0.0,0.0);

    //Determine how many frames there are in the pixel data. A CT scan may just be a 2d jpeg or something, 
    // but dose pixel data is 3d data composed of 'frames' of stacked 2d data.
    const auto frame_count = static_cast<unsigned long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0008, 0));
    if(frame_count != 0) FUNCERR("This routine only supports 2D images. Adapt the dose array loading code. Cannot continue");

    const auto image_rows  = static_cast<long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0010, 0));
    const auto image_cols  = static_cast<long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0011, 0));
    //const auto image_pxldx = static_cast<double>(TopDataSet->getDouble(0x0028, 0, 0x0030, 0)); //Spacing between adjacent rows.
    //const auto image_pxldy = static_cast<double>(TopDataSet->getDouble(0x0028, 0, 0x0030, 1)); //Spacing between adjacent columns.
    const auto image_pxldy = static_cast<double>(TopDataSet->getDouble(0x0028, 0, 0x0030, 0)); //Spacing between adjacent rows.
    const auto image_pxldx = static_cast<double>(TopDataSet->getDouble(0x0028, 0, 0x0030, 1)); //Spacing between adjacent columns.

    //For 2D images, there is often no thickness given. For CT we might have to compare to other files to figure this out.
    // For MR images, the thickness should be specified.
    double image_thickness = static_cast<double>(TopDataSet->getDouble(0x0018, 0, 0x0050, 0)); //"SliceThickness"
    if(image_thickness <= 0.0){
        image_thickness = 0.0;
        FUNCWARN("Image thickness not specified in DICOM file. Proceeding with zero thickness");
    }

    // -------------------------------------- Pixel Interpretation ------------------------------------------
    const auto pixel_representation = static_cast<unsigned long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0103, 0));

    //Sometimes Imebra returns something other the BitsStored or BitsAllocated. I cast values anyways, so it isn't
    // much of a concern and I've decided to just roll with it for now.
    //const auto image_bits  = static_cast<unsigned long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0101, 0)); //BitsStored.
    //const auto image_bits  = static_cast<unsigned long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0100, 0)); //BitsAllocated.
    //const auto image_chnls = static_cast<unsigned long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0002, 0)); //3 for RGB, 1 for monochrome.

    if( (TopDataSet->getTag(0x0040,0,0x9212) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x9216) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x9096) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x9211) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x9224) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x9225) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x9212) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x9210) != nullptr)
    ||  (TopDataSet->getTag(0x0028,0,0x3003) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x08EA) != nullptr) ){ 
        FUNCERR("This image contains a 'Real World Value' LUT (Look-Up Table), which is not presently"
                " not supported. You will need to fix the code to handle this");
        // NOTE: See DICOM Supplement 49 "Enhanced MR Image Storage SOP Class" at 
        //       ftp://medical.nema.org/medical/dicom/final/sup49_ft.pdf 
        //       (or another document if it has been superceded) for more info.
        //
        //       It should be rather easy to implement this. I just haven't needed to yet.
        //       You could potentially just get Imebra to do it for you. See the LUT code elsewhere
        //       in this routine.
    }

    double RescaleSlope = 1.0;
    double RescaleIntercept = 0.0;
    const bool RescaleSlopeInterceptPresent = (  (TopDataSet->getTag(0x0028,0,0x1052) != nullptr)
                                              && (TopDataSet->getTag(0x0028,0,0x1053) != nullptr) );
    if(RescaleSlopeInterceptPresent){
        RescaleIntercept = static_cast<double>(TopDataSet->getDouble(0x0028,0,0x1052,0));
        RescaleSlope     = static_cast<double>(TopDataSet->getDouble(0x0028,0,0x1053,0));

        // NOTE: There are also private Philips tags with the same info. Not sure if there is any point
        //       trying to fall back on them, though.
    }


    // --------------------------------------- Image Pixel Data ---------------------------------------------
    {    
        out->imagecoll.images.emplace_back();

        //--------------------------------------------------------------------------------------------------
        //Retrieve the pixel data from file. This is an excessively long exercise!
        ptr<puntoexe::imebra::image> firstImage = TopDataSet->getImage(0);
        if(firstImage == nullptr) FUNCERR("This file does not have accessible pixel data. Double check the file");
    
        //Process image using modalityVOILUT transform to convert its pixel values into meaningful values.
        // From what I can tell, this conversion is necessary to transform the raw data from a possibly
        // manufacturer-specific, propietary format into something physically meaningful for us. 
        //
        // I have not experimented with disabling this conversion. Leaving it intact causes the datum from
        // a Philips "Interra" machine's PAR/REC format to coincide with the exported DICOM data.
        ptr<imebra::transforms::transform> modVOILUT(new imebra::transforms::modalityVOILUT(TopDataSet));
        imbxUint32 width, height;
        firstImage->getSize(&width, &height);
        ptr<imebra::image> convertedImage(modVOILUT->allocateOutputImage(firstImage, width, height));
        modVOILUT->runTransform(firstImage, 0, 0, width, height, convertedImage, 0, 0);


        //Print a description of the VOI/LUT if available.
        if(!modVOILUT->isEmpty()){
            //FUNCINFO("Applied a Modality VOI/LUT");
        }else{
            FUNCINFO("Found no Modality VOI/LUT");
        }
    
        //Convert the 'convertedImage' into an image suitable for the viewing on screen. The VOILUT transform 
        // applies the contrast suggested by the dataSet to the image. Apply the first one we find. Relevant
        // DICOM tags reside around (0x0028,0x3010) and (0x0028,0x1050).
        //
        // This conversion uses the first suggested transformation found in the DICOM file, and will vary
        // from file to file. Generally, the transformation scales the pixel values to cover the range of the
        // available pixel range (i.e., u16). The transformation *CAN* induce clipping or truncation which 
        // cannot be recovered from!
        //
        // Therefore, in my opinion, it is never worthwhile to perform this conversion. If you want to window
        // or scale the values, you should do so as needed using the WindowCenter and WindowWidth values 
        // directly.
        //
        // Report available conversions:
        if(false){
            ptr<imebra::transforms::VOILUT> myVoiLut(new imebra::transforms::VOILUT(TopDataSet));
            std::vector<imbxUint32> VoiLutIds;
            for(imbxUint32 i = 0;  ; ++i){
                const auto VoiLutId = myVoiLut->getVOILUTId(i);
                if(VoiLutId == 0) break;
                VoiLutIds.push_back(VoiLutId);
            }
            //auto VoiLutIds = myVoiLut->getVOILUTIds();
            for(auto VoiLutId : VoiLutIds){
                const std::wstring VoiLutDescriptionWS = myVoiLut->getVOILUTDescription(VoiLutId);
                const std::string VoiLutDescription(VoiLutDescriptionWS.begin(), VoiLutDescriptionWS.end());
                FUNCINFO("Found 'presentation' VOI/LUT with description '" << VoiLutDescription << "' (not applying it!)");

                //Print the center and width of the VOI/LUT.
                imbxInt32 VoiLutCenter = std::numeric_limits<imbxInt32>::max();
                imbxInt32 VoiLutWidth  = std::numeric_limits<imbxInt32>::max();
                myVoiLut->getCenterWidth(&VoiLutCenter, &VoiLutWidth);
                if((VoiLutCenter != std::numeric_limits<imbxInt32>::max())
                || (VoiLutWidth  != std::numeric_limits<imbxInt32>::max())){
                    FUNCINFO("    - 'Presentation' VOI/LUT has centre = " << VoiLutCenter << " and width = " << VoiLutWidth);
                }
            }
        }
        //
        // Disable Imebra conversion:
        ptr<imebra::image> presImage(convertedImage);
        //
        // Enable Imebra conversion:
        //ptr<imebra::transforms::VOILUT> myVoiLut(new imebra::transforms::VOILUT(TopDataSet));
        //imbxUint32 lutId = myVoiLut->getVOILUTId(0);
        //myVoiLut->setVOILUT(lutId);
        //ptr<imebra::image> presImage(myVoiLut->allocateOutputImage(convertedImage, width, height));
        //myVoiLut->runTransform(convertedImage, 0, 0, width, height, presImage, 0, 0);
        //{
        //  //Print a description of the VOI/LUT if available.
        //  //const std::wstring VoiLutDescriptionWS = myVoiLut->getVOILUTDescription(lutId);
        //  //const std::string VoiLutDescription(VoiLutDescriptionWS.begin(), VoiLutDescriptionWS.end());
        //  //FUNCINFO("Using VOI/LUT with description '" << VoiLutDescription << "'");
        //
        //  //Print the center and width of the VOI/LUT.
        //  imbxInt32 VoiLutCenter = std::numeric_limits<imbxInt32>::max();
        //  imbxInt32 VoiLutWidth  = std::numeric_limits<imbxInt32>::max();
        //  myVoiLut->getCenterWidth(&VoiLutCenter, &VoiLutWidth);
        //  if((VoiLutCenter != std::numeric_limits<imbxInt32>::max())
        //  || (VoiLutWidth  != std::numeric_limits<imbxInt32>::max())){
        //      FUNCINFO("Using VOI/LUT with centre = " << VoiLutCenter << " and width = " << VoiLutWidth);
        //  }
        //}

 
        //Get the image in terms of 'RGB'/'MONOCHROME1'/'MONOCHROME2'/'YBR_FULL'/etc.. channels.
        //
        // This allows up to transform the data into a desired format before allocating any space.
        //
        // NOTE: The 'Photometric Interpretation' is specified in the DICOM file at 0x0028,0x0004 as a
        //       string. For instance "MONOCHROME2" is present in some MR images at the time of writing.
        //       It's not clear that I will want Imebra to transform the data under any circumstances, but
        //       to simplify things for now I'll assume we always want 'MONOCHROME2' format.
        //
        // NOTE: After some further digging, I believe letting Imebra convert to monochrome will allow
        //       us to handle compressed images without any extra work.
        puntoexe::imebra::transforms::colorTransforms::colorTransformsFactory*  pFactory = 
            puntoexe::imebra::transforms::colorTransforms::colorTransformsFactory::getColorTransformsFactory();
        ptr<puntoexe::imebra::transforms::transform> myColorTransform = 
            pFactory->getTransform(presImage->getColorSpace(), L"MONOCHROME2");//L"RGB");
        if(myColorTransform != 0){ //If we get a '0', we do not need to transform the image.
            ptr<puntoexe::imebra::image> rgbImage(myColorTransform->allocateOutputImage(presImage,width,height));
            myColorTransform->runTransform(presImage, 0, 0, width, height, rgbImage, 0, 0);
            presImage = rgbImage;
        }
    
        //Get a 'dataHandler' to access the image data waiting in 'presImage.' Get some image metadata.
        imbxUint32 rowSize, channelPixelSize, channelsNumber, sizeX, sizeY;
        ptr<puntoexe::imebra::handlers::dataHandlerNumericBase> myHandler = 
            presImage->getDataHandler(false, &rowSize, &channelPixelSize, &channelsNumber);
        presImage->getSize(&sizeX, &sizeY);
        //----------------------------------------------------------------------------------------------------

        if((static_cast<long int>(sizeX) != image_cols) || (static_cast<long int>(sizeY) != image_rows)){
            FUNCWARN("sizeX = " << sizeX << ", sizeY = " << sizeY << " and image_cols = " << image_cols << ", image_rows = " << image_rows);
            FUNCERR("The number of rows and columns in the image data differ when comparing sizeX/Y and img_rows/cols. Please verify");
            //If this issue arises, I have likely confused definition of X and Y. The DICOM standard specifically calls (0028,0010) 
            // a 'row'. Perhaps I've got many things backward...
        }

        out->imagecoll.images.back().metadata = get_metadata_top_level_tags(FilenameIn);
        out->imagecoll.images.back().init_orientation(image_orien_r,image_orien_c);

        const auto img_chnls = static_cast<long int>(channelsNumber);
        out->imagecoll.images.back().init_buffer(image_rows, image_cols, img_chnls); //Underlying type specifies per-pixel space allocated.

        const auto img_pxldz = image_thickness;
        out->imagecoll.images.back().init_spatial(image_pxldx,image_pxldy,img_pxldz, image_anchor, image_pos);

        //Sometimes Imebra returns a different number of bits than the DICOM header specifies. Presumably this
        // is for some reason (maybe even simplification of implementation, which is fair). Since I convert to
        // a float or uint32_t, the only practical concern is whether or not it will fit.
        const auto img_bits  = static_cast<unsigned int>(channelPixelSize*8); //16 bit, 32 bit, 8 bit, etc..
        if(img_bits > 32){
            FUNCERR("The number of bits returned by Imebra (" << img_bits << ") is too large to fit in uint32_t"
                    "You can increase this if needed, or try to scale down to 32 bits");
        }
        out->bits = img_bits;
        //if(img_bits != image_bits){
        //    FUNCERR("The number of bits in each channel varies between the DICOM header ("
        //            << image_bits << ") and the transformed image data (" << img_bits << ")");
        //    //If this ever comes up, you need to decide whether you actually want Imebra resampling the data. See the
        //    // above code block which transforms to MONOCHROME2. Most likely, you should go with the Imebra result b/c
        //    // it is most likely what you want. Ultimately you will convert to an unsigned int anyways.
        //}

        //Write the data to our allocated memory. We do it pixel-by-pixel because the 'PixelRepresentation' could mean
        // the pixel locality is laid out in various ways (two ways?). This approach abstracts the issue away.
        imbxUint32 data_index = 0;
        //for(imbxUint32 scanX = 0; scanX < sizeX; ++scanX){ //Rows.
        //    for(imbxUint32 scanY = 0; scanY < sizeY; ++scanY){ //Columns.
        //        for(imbxUint32 scanChannel = 0; scanChannel < channelsNumber; ++scanChannel){ //Channels.
        const bool pixelsAreSigned = (myHandler->isSigned());
        for(long int row = 0; row < image_rows; ++row){
            for(long int col = 0; col < image_cols; ++col){
                for(long int chnl = 0; chnl < img_chnls; ++chnl){

                    //The are a number of ways we can pack this data into the destination buffer. 
                    // Using uint32_t, int32_t, or float make the most sense. Be aware that CT images will
                    // generally have negative pixel values, because CT Hounsfield units range from
                    // -1000 HU (air) to +4000 HU (metals). Water is 0 HU so we will generally encounter 
                    // negatives!
                    //
                    //Approach A: the more 'usual' (unsigned) integer-valued pixel value image.
                    //const imbxUint32 UnsignedChannelValue = myHandler->getUnsignedLong(data_index);
                    //out->imagecoll.images.back().reference(row,col,chnl) = static_cast<uint32_t>(UnsignedChannelValue);
                    //++data_index;
                    //
                    //Approach B: the more 'usual' (signed) integer-valued pixel value image.
                    //const imbxInt32 UnsignedChannelValue = myHandler->getSignedLong(data_index);
                    //if(SignedChannelValue < 0) FUNCERR("Encountered a negative pixel value at row,col,chnl = " << row << "," << col << "," << chnl);
                    //out->imagecoll.images.back().reference(row,col,chnl) = static_cast<uint32_t>(SignedChannelValue);
                    //++data_index;
                    //
                    //Approach C: the generally safe and sane approach of using floating-point types. We may,
                    // in extreme cases, lose some precision, but for clinical CTs floats will be sufficient to 
                    // represent the full HU range.
                    //float OutgoingPixelValue = std::numeric_limits<float>::quiet_NaN();
                    ////if(pixel_representation == static_cast<unsigned long int>(0)){ //Unsigned pixel representation.
                    //if(!pixelsAreSigned){ //Unsigned pixel representation.
                    //    const imbxUint32 UnsignedChannelValue = myHandler->getUnsignedLong(data_index);
                    //    //const auto Rescaled = (RescaleSlopeInterceptPresent) 
                    //    //                      ? RescaleIntercept + RescaleSlope * static_cast<double>(UnsignedChannelValue) 
                    //    //                      : static_cast<double>(UnsignedChannelValue);
                    //    //OutgoingPixelValue = static_cast<float>(Rescaled);
                    //    OutgoingPixelValue = static_cast<float>(UnsignedChannelValue);
                    ////}else if(pixel_representation == static_cast<unsigned long int>(1)){ //Two's complement pixel representation.
                    //}else{ //Two's complement pixel representation.
                    //    //Technically this is not just 'signed' but specifically two's complement representation. 
                    //    // I think the best way is to verify by directly testing the implementation (e.g., compare 
                    //    // -1 (negative one) with ~0 (bitwise complemented zero) to verify two's complement. Before 
                    //    // doing so, check if C++11/14/+ has any way to static_assert this, such as in feature-test 
                    //    // macros, numerical limits, or by the implementation exposing a macro of some sort.
                    //    const imbxInt32 SignedChannelValue = myHandler->getSignedLong(data_index);
                    //    //const auto Rescaled = (RescaleSlopeInterceptPresent) 
                    //    //                      ? RescaleIntercept + RescaleSlope * static_cast<double>(SignedChannelValue)  
                    //    //                      : static_cast<double>(SignedChannelValue);
                    //    //OutgoingPixelValue = static_cast<float>(Rescaled);
                    //    OutgoingPixelValue = static_cast<float>(SignedChannelValue);
                    //}
                    //
                    //Approach D: let Imebra work out the conversersion by asking for a double. Hope it can be narrowed
                    // if necessary!
                    const auto DoubleChannelValue = myHandler->getDouble(data_index);
                    const float OutgoingPixelValue = static_cast<float>(DoubleChannelValue);

                    out->imagecoll.images.back().reference(row,col,chnl) = OutgoingPixelValue;
                    ++data_index;
                } //Loop over channels.
            } //Loop over columns.
        } //Loop over rows.
    }
    return std::move(out);
}

//These 'shared' pointers will actually be unique. This routine just converts from unique to shared for you.
std::list<std::shared_ptr<Image_Array>>  Load_Image_Arrays(const std::list<std::string> &filenames){
    std::list<std::shared_ptr<Image_Array>> out;
    for(auto it = filenames.begin(); it != filenames.end(); ++it){
        out.push_back(std::move(Load_Image_Array(*it)));
    }
    return std::move(out);
}

//Since many images must be loaded individually from a file, we will often have to collate them together.
//
//Note: Returns a nullptr if the collation was not successful. The input data will not be restored to the
//      exact way it was passed in. Returns a valid pointer to an empty Image_Array if there was no data
//      to collate.
//
//Note: Despite using shared_ptrs, if the collation fails some images may be collated while others weren't. 
//      Deep-copy images beforehand if this is something you aren't prepared to deal with.
//
std::unique_ptr<Image_Array> Collate_Image_Arrays(std::list<std::shared_ptr<Image_Array>> &in){
    std::unique_ptr<Image_Array> out(new Image_Array);
    if(in.empty()) return std::move(out);

    //Start from the end and work toward the beginning so we can easily pop the end. Keep all images in
    // the original list to ease collating to the first element.
    while(!in.empty()){
        auto pic_it = in.begin();
        const bool GeometricalOverlapOK = true;
        if(!out->imagecoll.Collate_Images((*pic_it)->imagecoll, GeometricalOverlapOK)){
            //We've encountered an issue and the images won't collate. Push the successfully collated
            // images back into the list and return a nullptr.
            in.push_back(std::move(out));
            return nullptr;
        }
        pic_it = in.erase(pic_it);
    }
    return std::move(out);
}



//--------------------- Dose -----------------------
//This routine reads a single DICOM dose file and outputs an array pointing to a Dose_Array (ie. ~a list
// of image
std::unique_ptr<Dose_Array>  Load_Dose_Array(const std::string &FilenameIn){
    auto metadata = get_metadata_top_level_tags(FilenameIn);

    std::unique_ptr<Dose_Array> out(new Dose_Array());

    using namespace puntoexe;
    ptr<puntoexe::stream> readStream(new puntoexe::stream);
    readStream->openFile(FilenameIn.c_str(), std::ios::in);

    ptr<puntoexe::streamReader> reader(new puntoexe::streamReader(readStream));
    ptr<imebra::dataSet> TopDataSet = imebra::codecs::codecFactory::getCodecFactory()->load(reader);

    //These are CT-ish only (not for dose files,) but they should just return a zero nicely when we query and
    // they are not there. If this were not the case, we would simply need to check the modality before querying.
    //const auto slice_thickness  = static_cast<double>(TopDataSet->getDouble(0x0018, 0, 0x0050, 0));
    //const auto slice_height     = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x1041, 0));

    //These should exist in all files. They appear to be the same for CT and DS files of the same set. Not sure
    // if this is *always* the case.
    const auto image_pos_x = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0032, 0));
    const auto image_pos_y = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0032, 1));
    const auto image_pos_z = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0032, 2));
    const vec3<double> image_pos(image_pos_x,image_pos_y,image_pos_z); //Only for first image!

    const auto image_orien_c_x = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 0)); 
    const auto image_orien_c_y = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 1));
    const auto image_orien_c_z = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 2));
    const vec3<double> image_orien_c = vec3<double>(image_orien_c_x,image_orien_c_y,image_orien_c_z).unit();

    const auto image_orien_r_x = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 3));
    const auto image_orien_r_y = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 4));
    const auto image_orien_r_z = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 5));
    const vec3<double> image_orien_r = vec3<double>(image_orien_r_x,image_orien_r_y,image_orien_r_z).unit();

    const vec3<double> image_stack_unit = (image_orien_c.Cross(image_orien_r)).unit(); //Unit vector denoting direction to stack images.
    const vec3<double> image_anchor  = vec3<double>(0.0,0.0,0.0);

    //Determine how many frames there are in the pixel data. A CT scan may just be a 2d jpeg or something, 
    // but dose pixel data is 3d data composed of 'frames' of stacked 2d data.
    const auto frame_count = static_cast<unsigned long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0008, 0));
    if(frame_count == 0) FUNCERR("No frames were found in file '" << FilenameIn << "'. Is it a valid dose file?");

    //This is a redirection to another tag. I've never seen it be anything but (0x3004,0x000c).
    const auto frame_inc_pntrU  = static_cast<long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0009, 0));
    const auto frame_inc_pntrL  = static_cast<long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0009, 1));
    if((frame_inc_pntrU != static_cast<long int>(0x3004)) || (frame_inc_pntrL != static_cast<long int>(0x000c)) ){
        FUNCWARN(" frame increment pointer U,L = " << frame_inc_pntrU << "," << frame_inc_pntrL);
        FUNCERR("Dose file contains a frame increment pointer which we have not encountered before. Please ensure we can handle it properly");
    }

    std::list<double> gfov;
    for(unsigned long int i=0; i<frame_count; ++i){
        const auto val = static_cast<double>(TopDataSet->getDouble(0x3004, 0, 0x000c, i));
        gfov.push_back(val);
    }

    const double image_thickness = (gfov.size() > 1) ? ( *(++gfov.begin()) - *(gfov.begin()) ) : 1.0; //*NOT* the image separation!

    const auto image_rows  = static_cast<long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0010, 0));
    const auto image_cols  = static_cast<long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0011, 0));
    //const auto image_pxldx = static_cast<double>(TopDataSet->getDouble(0x0028, 0, 0x0030, 0)); //Spacing between adjacent rows.
    //const auto image_pxldy = static_cast<double>(TopDataSet->getDouble(0x0028, 0, 0x0030, 1)); //Spacing between adjacent columns.
    const auto image_pxldy = static_cast<double>(TopDataSet->getDouble(0x0028, 0, 0x0030, 0)); //Spacing between adjacent rows.
    const auto image_pxldx = static_cast<double>(TopDataSet->getDouble(0x0028, 0, 0x0030, 1)); //Spacing between adjacent columns.
    const auto image_bits  = static_cast<unsigned long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0101, 0));
    const auto grid_scale  = static_cast<double>(TopDataSet->getDouble(0x3004, 0, 0x000e, 0));

    const auto pixel_representation = static_cast<unsigned long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0103, 0));

    //Grab the image data for each individual frame.
    auto gfov_it = gfov.begin();
    for(unsigned long int curr_frame = 0; (curr_frame < frame_count) && (gfov_it != gfov.end()); ++curr_frame, ++gfov_it){
        out->imagecoll.images.emplace_back();

        //--------------------------------------------------------------------------------------------------
        //Retrieve the pixel data from file. This is an excessively long exercise!
        ptr<puntoexe::imebra::image> firstImage = TopDataSet->getImage(curr_frame); 	
        if(firstImage == nullptr) FUNCERR("This file does not have accessible pixel data. Double check the file");
    
        //Process image using modalityVOILUT transform to convert its pixel values into meaningful values.
        ptr<imebra::transforms::transform> modVOILUT(new imebra::transforms::modalityVOILUT(TopDataSet));
        imbxUint32 width, height;
        firstImage->getSize(&width, &height);
        ptr<imebra::image> convertedImage(modVOILUT->allocateOutputImage(firstImage, width, height));
        modVOILUT->runTransform(firstImage, 0, 0, width, height, convertedImage, 0, 0);
    
        //Convert the 'convertedImage' into an image suitable for the viewing on screen. The VOILUT transform 
        // applies the contrast suggested by the dataSet to the image. Apply the first one we find.
        //
        // I'm not sure how this affects dose values, if at all, so I've disabled it for now.
        //ptr<imebra::transforms::VOILUT> myVoiLut(new imebra::transforms::VOILUT(TopDataSet));
        //imbxUint32 lutId = myVoiLut->getVOILUTId(0);
        //myVoiLut->setVOILUT(lutId);
        //ptr<imebra::image> presImage(myVoiLut->allocateOutputImage(convertedImage, width, height));
        ptr<imebra::image> presImage = convertedImage;
        //myVoiLut->runTransform(convertedImage, 0, 0, width, height, presImage, 0, 0);
 
        //Get the image in terms of 'RGB'/'MONOCHROME1'/'MONOCHROME2'/'YBR_FULL'/etc.. channels.
        //
        // This allows up to transform the data into a desired format before allocating any space.
        puntoexe::imebra::transforms::colorTransforms::colorTransformsFactory*  pFactory = 
             puntoexe::imebra::transforms::colorTransforms::colorTransformsFactory::getColorTransformsFactory();
        ptr<puntoexe::imebra::transforms::transform> myColorTransform = 
             pFactory->getTransform(presImage->getColorSpace(), L"MONOCHROME2");//L"RGB");
        if(myColorTransform != 0){ //If we get a '0', we do not need to transform the image.
            ptr<puntoexe::imebra::image> rgbImage(myColorTransform->allocateOutputImage(presImage,width,height));
            myColorTransform->runTransform(presImage, 0, 0, width, height, rgbImage, 0, 0);
            presImage = rgbImage;
        }
    
        //Get a 'dataHandler' to access the image data waiting in 'presImage.' Get some image metadata.
        imbxUint32 rowSize, channelPixelSize, channelsNumber, sizeX, sizeY;
        ptr<puntoexe::imebra::handlers::dataHandlerNumericBase> myHandler = 
            presImage->getDataHandler(false, &rowSize, &channelPixelSize, &channelsNumber);
        presImage->getSize(&sizeX, &sizeY);
        //----------------------------------------------------------------------------------------------------

        if((static_cast<long int>(sizeX) != image_cols) || (static_cast<long int>(sizeY) != image_rows)){
            FUNCWARN("sizeX = " << sizeX << ", sizeY = " << sizeY << " and image_cols = " << image_cols << ", image_rows = " << image_rows);
            FUNCERR("The number of rows and columns in the image data differ when comparing sizeX/Y and img_rows/cols. Please verify");
            //If this issue arises, I have likely confused definition of X and Y. The DICOM standard specifically calls (0028,0010) 
            // a 'row'. Perhaps I've got many things backward...
        }

        out->imagecoll.images.back().metadata = metadata;
        out->imagecoll.images.back().init_orientation(image_orien_r,image_orien_c);

        const auto img_chnls = static_cast<long int>(channelsNumber);
        out->imagecoll.images.back().init_buffer(image_rows, image_cols, img_chnls);

        const auto img_pxldz = image_thickness;
        const auto gvof_offset = static_cast<double>(*gfov_it);  //Offset along \hat{z} from 
        const auto img_offset = image_pos + image_stack_unit * gvof_offset;
        out->imagecoll.images.back().init_spatial(image_pxldx,image_pxldy,img_pxldz, image_anchor, img_offset);

        out->imagecoll.images.back().metadata["GridFrameOffset"] = std::to_string(gvof_offset);
        out->imagecoll.images.back().metadata["Frame"] = std::to_string(curr_frame);
        out->imagecoll.images.back().metadata["ImagePositionPatient"] = img_offset.to_string();


        const auto img_bits  = static_cast<unsigned int>(channelPixelSize*8); //16 bit, 32 bit, 8 bit, etc..
        if(img_bits != image_bits){
            FUNCERR("The number of bits in each channel varies between the DICOM header and the transformed image data");
            //Not sure what to do if this happens. Perhaps just go with the imebra result?
        }

        //Write the data to our allocated memory.
        imbxUint32 data_index = 0;
        //for(imbxUint32 scanX = 0; scanX < sizeX; ++scanX){ //Rows.
        //    for(imbxUint32 scanY = 0; scanY < sizeY; ++scanY){ //Columns.
        //        for(imbxUint32 scanChannel = 0; scanChannel < channelsNumber; ++scanChannel){ //Channels.
        const bool pixelsAreSigned = (myHandler->isSigned());
        for(long int row = 0; row < image_rows; ++row){
            for(long int col = 0; col < image_cols; ++col){
                for(long int chnl = 0; chnl < img_chnls; ++chnl){
                    //NOTE: In earlier code, I kept pixel values as the raw DICOM-packed integers and used a separate Dose_Array
                    //      member called 'grid_scale' to perform the scaling when I needed dose. When I switched to a floating-
                    //      point pixel type, I decided it made the most sense (reducing complexity, conversion to-from plain 
                    //      images) to just have the pixels directly express dose. Therefore, the grid_scale member is now set 
                    //      to 1.0 ALWAYS for compatibility. It could be removed entirely (and the Dose_Array class) safely.
                    ////if(pixel_representation == static_cast<unsigned long int>(0)){ //Unsigned pixel representation.
                    //if(!pixelsAreSigned){ //Unsigned pixel representation.
                    //    const imbxUint32 UnsignedChannelValue = myHandler->getUnsignedLong(data_index);
                    //    out->imagecoll.images.back().reference(row,col,chnl) = static_cast<float>(UnsignedChannelValue) 
                    //                                                           * static_cast<float>(grid_scale);
                    ////}else if(pixel_representation == static_cast<unsigned long int>(1)){ //Signed pixel representation.
                    //}else{ //Signed pixel representation.
                    //    //Technically this is not just 'signed' but specifically two's complement representation. 
                    //    // I think the best way is to verify by directly testing the implementation (e.g., compare 
                    //    // -1 (negative one) with ~0 (bitwise complemented zero) to verify two's complement. Before 
                    //    // doing so, check if C++11/14/+ has any way to static_assert this, such as in feature-test 
                    //    // macros, numerical limits, or by the implementation exposing a macro of some sort.
                    //    const imbxInt32 SignedChannelValue = myHandler->getSignedLong(data_index);
                    //    out->imagecoll.images.back().reference(row,col,chnl) = static_cast<float>(SignedChannelValue)
                    //                                                           * static_cast<float>(grid_scale);
                    //}
                    //
                    //Approach D: let Imebra work out the conversersion by asking for a double. Hope it can be narrowed
                    // if necessary!
                    const auto DoubleChannelValue = myHandler->getDouble(data_index);
                    const float OutgoingPixelValue = static_cast<float>(DoubleChannelValue) 
                                                     * static_cast<float>(grid_scale);
                    out->imagecoll.images.back().reference(row,col,chnl) = OutgoingPixelValue;

                    ++data_index;
                } //Loop over channels.
            } //Loop over columns.
        } //Loop over rows.
    } //Loop over frames.

    //Finally, pass the collection-specific items out.

    out->bits       = image_bits;
    out->grid_scale = 1.0; //grid_scale; <-- NOTE: pixels now hold dose directly and do not require scaling!
    out->filename   = FilenameIn;
    return std::move(out);
}

//These 'shared' pointers will actually be unique. This routine just converts from unique to shared for you.
std::list<std::shared_ptr<Dose_Array>>  Load_Dose_Arrays(const std::list<std::string> &filenames){
    std::list<std::shared_ptr<Dose_Array>> out;
    for(auto it = filenames.begin(); it != filenames.end(); ++it){
        out.push_back(std::move(Load_Dose_Array(*it)));
    }
    return std::move(out);
}

