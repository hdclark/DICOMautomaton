//MeldDose.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <map>
#include <string>    

#include "../Dose_Meld.h"
#include "../Structs.h"
#include "MeldDose.h"



OperationDoc OpArgDocMeldDose(){
    OperationDoc out;
    out.name = "MeldDose";

    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: radiation dose");

    out.desc = 
        "This operation melds all available dose image data. At a high level, dose melding sums overlapping pixel values"
        " for multi-part dose arrays. For more information about what this specifically entails, refer to the appropriate"
        " subroutine.";

    return out;
}



bool MeldDose(Drover &DICOM_data,
                const OperationArgPkg& /*OptArgs*/,
                std::map<std::string, std::string>& /*InvocationMetadata*/,
                const std::string& /*FilenameLex*/){

    DICOM_data = Meld_Only_Dose_Data(DICOM_data);

    return true;
}
