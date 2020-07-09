//AccumulateRowsColumns.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <algorithm>
#include <array>
#include <exception>
#include <optional>
#include <fstream>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/ConvenienceRoutines.h"
#include "AccumulateRowsColumns.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.


OperationDoc OpArgDocAccumulateRowsColumns(){
    OperationDoc out;
    out.name = "AccumulateRowsColumns";

    out.desc = 
        "This operation generates row- and column-profiles of images in which the entire row or column has been summed"
        " together. It is useful primarily for detection of axes-aligned edges or ridges.";
        
    out.notes.emplace_back(
        "It is often useful to pre-process inputs by computing an in-image-plane derivative, gradient magnitude, or"
        " similar (i.e., something to emphasize edges) before calling this routine. It is not necessary, however."
    );
        
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    
    return out;
}

Drover AccumulateRowsColumns(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        std::vector<YgorMathPlottingGnuplot::Shuttle<samples_1D<double>>> row_sums;
        std::vector<YgorMathPlottingGnuplot::Shuttle<samples_1D<double>>> col_sums;
        {
            decltype((*iap_it)->imagecoll.images) shtl;
            for(const auto & animg : (*iap_it)->imagecoll.images){
                std::vector<double> row_sum(animg.rows, 0.0);
                std::vector<double> col_sum(animg.columns, 0.0);

                //Sum pixel values row- and column-wise.
                for(auto row = 0; row < animg.rows; ++row){
                    for(auto col = 0; col < animg.columns; ++col){
                        for(auto chan = 0; chan < animg.channels; ++chan){
                            const auto val = animg.value(row, col, chan);
                            row_sum[row] += val;
                            col_sum[col] += val;
                        }//Loop over channels.
                    } //Loop over cols
                } //Loop over rows

                //Record the data in the form of comparative plots.
                {
                    samples_1D<double> row_profile;
                    samples_1D<double> col_profile;

                    for(auto row = 0; row < animg.rows; ++row){
                        const auto pos = animg.position(row,0).Dot(animg.row_unit); //Relative to DICOM origin.
                        row_profile.push_back({ pos, 0.0, row_sum[row], 0.0 });
                    }
                    for(auto col = 0; col < animg.columns; ++col){
                        const auto pos = animg.position(0,col).Dot(animg.col_unit); //Relative to DICOM origin.
                        col_profile.push_back({ pos, 0.0, col_sum[col], 0.0 });
                    }
                    const auto row_area = row_profile.Integrate_Over_Kernel_unit()[0];
                    const auto col_area = col_profile.Integrate_Over_Kernel_unit()[0];
                    row_profile = row_profile.Multiply_With(1.0/row_area);
                    col_profile = col_profile.Multiply_With(1.0/col_area);

                    row_sums.emplace_back(row_profile, "Row Profile");
                    col_sums.emplace_back(col_profile, "Column Profile");
                }

                // Produce some images for the user to inspect.
                planar_image<float,double> row_prof(animg);
                planar_image<float,double> col_prof(animg);
                Stats::Running_MinMax<float> minmax_row;
                Stats::Running_MinMax<float> minmax_col;

                for(auto row = 0; row < animg.rows; ++row){
                    for(auto col = 0; col < animg.columns; ++col){
                        for(auto chan = 0; chan < animg.channels; ++chan){
                            row_prof.reference(row, col, chan) = row_sum[row];
                            col_prof.reference(row, col, chan) = col_sum[col];

                            minmax_row.Digest( row_sum[row] );
                            minmax_col.Digest( col_sum[col] );
                        }//Loop over channels.
                    } //Loop over cols
                } //Loop over rows

                const std::string row_desc = "Row-wise pixel accumulation";
                const std::string col_desc = "Column-wise pixel accumulation";
                UpdateImageDescription( std::ref(row_prof), row_desc );
                UpdateImageDescription( std::ref(col_prof), col_desc );
                UpdateImageWindowCentreWidth( std::ref(row_prof), minmax_row );
                UpdateImageWindowCentreWidth( std::ref(col_prof), minmax_col );

                shtl.emplace_back( row_prof );
                shtl.emplace_back( col_prof );
            }

            (*iap_it)->imagecoll.images.splice( (*iap_it)->imagecoll.images.end(),
                                                std::move(shtl) );
        }

        // Display the row and column sum profiles for visual estimation of edge coincidence.
        {
            try{
                YgorMathPlottingGnuplot::Plot<double>(row_sums, "Row sums", "DICOM position", "Pixel intensity");
                YgorMathPlottingGnuplot::Plot<double>(col_sums, "Column sums", "DICOM position", "Pixel intensity");
            }catch(const std::exception &e){
                FUNCWARN("Failed to plot: " << e.what());
            }
        }
    }

    return DICOM_data;
}
