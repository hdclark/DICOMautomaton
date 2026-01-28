//GenerateVirtualDataLineSampleV1.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <cmath>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <cstdint>

#include "YgorMath.h"         //Needed for samples_1D class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Imebra_Shim.h"
#include "../Structs.h"
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

    // Gaussian parameters as specified in the problem statement
    const double centre = 50.0;
    const double magnitude = 1.0;
    const double sigma = 10.0;
    const int64_t num_samples = 150;

    // Create a new Line_Sample
    auto ls_ptr = std::make_shared<Line_Sample>();

    // Set deterministic metadata similar to other GenerateVirtualData operations
    ls_ptr->line.metadata["PatientID"] = "VirtualDataLineSampleVersion1";
    ls_ptr->line.metadata["LineName"] = "GaussianDistribution";
    ls_ptr->line.metadata["Description"] = "Synthetic Gaussian line sample";
    ls_ptr->line.metadata["Centre"] = std::to_string(centre);
    ls_ptr->line.metadata["Magnitude"] = std::to_string(magnitude);
    ls_ptr->line.metadata["Sigma"] = std::to_string(sigma);
    ls_ptr->line.metadata["NumberOfSamples"] = std::to_string(num_samples);

    // Generate the Gaussian samples
    // We'll sample over a range that covers the Gaussian well
    // Using approximately 5 sigma on each side of the centre
    const double x_min = centre - 5.0 * sigma;
    const double x_max = centre + 5.0 * sigma;
    const double dx = (x_max - x_min) / static_cast<double>(num_samples - 1);

    for(int64_t i = 0; i < num_samples; ++i){
        const double x = x_min + i * dx;
        
        // Gaussian formula: magnitude * exp(-(x - centre)^2 / (2 * sigma^2))
        const double exponent = -std::pow(x - centre, 2.0) / (2.0 * std::pow(sigma, 2.0));
        const double y = magnitude * std::exp(exponent);
        
        // Add sample to the line: {x_value, x_uncertainty, y_value, y_uncertainty}
        // Using 0.0 for uncertainties since this is deterministic synthetic data
        ls_ptr->line.push_back({ x, 0.0, y, 0.0 });
    }

    // Add the Line_Sample to the Drover object
    DICOM_data.lsamp_data.emplace_back(ls_ptr);

    return true;
}
