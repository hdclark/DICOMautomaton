//ExportLineSamples.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <asio.hpp>
#include <algorithm>
#include <experimental/optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "ExportLineSamples.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"

#include "../Surface_Meshes.h"


OperationDoc OpArgDocExportLineSamples(void){
    OperationDoc out;
    out.name = "ExportLineSamples";

    out.desc = 
        "This operation writes a line sample to a file.";


    out.args.emplace_back();
    out.args.back() = LSWhitelistOpArgDoc();
    out.args.back().name = "LineSelection";
    out.args.back().default_val = "last";
   

    out.args.emplace_back();
    out.args.back().name = "FilenameBase";
    out.args.back().desc = "The base filename that line samples will be written to."
                           " The file format is a 4-column text file that can be readily plotted."
                           " The columns are 'x dx f df' where dx (df) represents the uncertainty in x (f)"
                           " if available. Metadata is included, but will be base64 encoded if any non-printable"
                           " characters are detected. If no name is given, the default will be used."
                           " A '_', a sequentially-increasing number, and the '.dat' file suffix are"
                           " appended after the base filename.";
    out.args.back().default_val = "/tmp/dcma_exportlinesamples";
    out.args.back().expected = true;
    out.args.back().examples = { "line_sample", 
                                 "../somedir/data", 
                                 "/path/to/some/line_sample_to_plot" };
    out.args.back().mimetype = "text/plain";

    return out;
}



Drover ExportLineSamples(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto LineSelectionStr = OptArgs.getValueStr("LineSelection").value();
    const auto FilenameBaseStr = OptArgs.getValueStr("FilenameBase").value();

    //-----------------------------------------------------------------------------------------------------------------

    auto LSs_all = All_LSs( DICOM_data );
    const auto LSs = Whitelist( LSs_all, LineSelectionStr );
    const auto N_LSs = LSs.size();
    for(auto & lsp_it : LSs){

        // Determine which filename to use.
        const auto FN = Get_Unique_Sequential_Filename(FilenameBaseStr + "_", 6, ".dat");
        std::fstream FO(FN, std::fstream::out);

        // Write the data to file.
        if(!(*lsp_it)->line.Write_To_Stream(FO)){
            throw std::runtime_error("Unable to write line sample. Cannot continue.");
        }

        FUNCINFO("Line sample written to '" << FN << "'");
    }

    return DICOM_data;
}
