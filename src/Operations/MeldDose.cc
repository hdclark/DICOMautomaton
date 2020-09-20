//MeldDose.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <map>
#include <string>    

#include "../Dose_Meld.h"
#include "../Structs.h"
#include "MeldDose.h"



OperationDoc OpArgDocMeldDose(){
    OperationDoc out;
    out.name = "MeldDose";

    out.desc = 
        "This operation melds all available dose image data. At a high level, dose melding sums overlapping pixel values"
        " for multi-part dose arrays. For more information about what this specifically entails, refer to the appropriate"
        " subroutine.";

    return out;
}



Drover MeldDose(Drover DICOM_data,
                const OperationArgPkg& /*OptArgs*/,
                const std::map<std::string, std::string>& /*InvocationMetadata*/,
                const std::string& /*FilenameLex*/){

    DICOM_data = Meld_Only_Dose_Data(DICOM_data);

    return DICOM_data;
}
