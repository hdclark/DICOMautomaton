//GenerateVirtualDataLineSampleV1.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <cmath>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <cstdint>

#include "YgorMath.h"         //Needed for samples_1D class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

//#include "../Imebra_Shim.h"
#include "../Structs.h"
#include "../Metadata.h"

#include "GenerateVirtualDataLineSampleV1.h"


OperationDoc OpArgDocGenerateVirtualDataLineSampleV1(){
    OperationDoc out;
    out.name = "GenerateVirtualDataLineSampleV1";

    out.tags.emplace_back("category: line sample processing");
    out.tags.emplace_back("category: generator");
    out.tags.emplace_back("category: virtual phantom");

    out.desc = 
        "This operation generates a deterministic synthetic line sample with a Gaussian distribution."
        " It can be used for testing how line sample data is transformed or processed.";

    return out;
}

bool GenerateVirtualDataLineSampleV1(Drover &DICOM_data,
                                     const OperationArgPkg&,
                                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                                     const std::string& /*FilenameLex*/){

    // Gaussian model parameters, fixed for V1 virtual phantom.
    const double centre       = 50.0;
    const double magnitude    = 1.0;
    const double sigma        = 10.0;
    const int64_t num_samples = 150;

    auto ls_ptr = std::make_shared<Line_Sample>();

    // Asign baseline metadata.
    ls_ptr->line.metadata["PatientID"] = "VirtualDataLineSampleVersion1";
    ls_ptr->line.metadata["LineName"] = "GaussianDistribution";
    ls_ptr->line.metadata["Description"] = "Synthetic Gaussian line sample";
    ls_ptr->line.metadata["ContentDate"] = "20260127";
    ls_ptr->line.metadata["ContentTime"] = "204137";
    ls_ptr->line.metadata["OriginFilename"] = "/dev/null";

    ls_ptr->line.metadata = coalesce_metadata_for_lsamp(ls_ptr->line.metadata);

    ls_ptr->line.metadata["DistributionGaussianCentre"] = std::to_string(centre);
    ls_ptr->line.metadata["DistributionGaussianMagnitude"] = std::to_string(magnitude);
    ls_ptr->line.metadata["DistributionGaussianSigma"] = std::to_string(sigma);

    const double x_min = centre - 5.0 * sigma;
    const double x_max = centre + 5.0 * sigma;
    const double dx = (x_max - x_min) / static_cast<double>(num_samples - 1);
    for(int64_t i = 0; i < num_samples; ++i){
        const double x = x_min + i * dx;
        
        // Gaussian formula: magnitude * exp(-(x - centre)^2 / (2 * sigma^2))
        const double exponent = std::pow(x - centre, 2.0) / (2.0 * std::pow(sigma, 2.0));
        const double y = magnitude * std::exp(-exponent);
        
        ls_ptr->line.push_back({ x, 0.0, y, 0.0 });
    }

    DICOM_data.lsamp_data.emplace_back(ls_ptr);

    return true;
}
