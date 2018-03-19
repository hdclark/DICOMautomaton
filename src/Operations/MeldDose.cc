//MeldDose.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <list>
#include <map>
#include <string>    

#include "../Dose_Meld.h"
#include "../Structs.h"
#include "MeldDose.h"



std::list<OperationArgDoc> OpArgDocMeldDose(void){
    std::list<OperationArgDoc> out;

    // This operation melds all available dose image data. At a high level, dose melding sums overlapping pixel values
    // for multi-part dose arrays. For more information about what this specifically entails, refer to the appropriate
    // subroutine.

    return out;
}



Drover MeldDose(Drover DICOM_data, 
                OperationArgPkg /*OptArgs*/, 
                std::map<std::string,std::string> /*InvocationMetadata*/, 
                std::string /*FilenameLex*/ ){

    DICOM_data.dose_data = Meld_Dose_Data(DICOM_data.dose_data);

    return DICOM_data;
}
