//ExportDrover.cc - A part of DICOMautomaton 2023. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <vector>

#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorStats.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"

#ifndef DCMA_USE_THRIFT
    #error "Attempted to compile serialization operation without Apache Thrift, which is required"
#endif //DCMA_USE_THRIFT

#include "../rpc/Serialization.h"

#include "ExportDrover.h"


OperationDoc OpArgDocExportDrover(){
    OperationDoc out;
    out.name = "ExportDrover";

    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: RPC");
    out.tags.emplace_back("category: file export");

    out.desc = 
        "This operation serializes the current Drover to a file."
        " It uses Apache Thrift for serialization.";

    out.notes.emplace_back(
        "RPC functionality is currently alpha-quality code, and much is expected to change."
    );

    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The filename to write to.";
    out.args.back().default_val = "out.ts_dcma";
    out.args.back().expected = true;
    out.args.back().examples = { "out.ts_dcma", "/tmp/out.ts_dcma" };

    return out;
}


bool ExportDrover(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& InvocationMetadata,
                  const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto Filename = OptArgs.getValueStr("Filename").value();
    //-----------------------------------------------------------------------------------------------------------------

    try{
        std::string serialized;
        if(!Serialize_Drover_To_Thrift_JSON(DICOM_data, serialized)){
            throw std::runtime_error("Unable to serialize Drover");
        }

        std::ofstream ofs(Filename, std::ios::out | std::ios::binary | std::ios::trunc);
        if(!ofs){
            throw std::runtime_error("Unable to open file");
        }
        ofs.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
        if(!ofs){
            throw std::runtime_error("Unable to write file");
        }

        YLOGINFO("Serialized Drover object to '" << Filename << "'");

    }catch( const std::exception &e){
        YLOGWARN("Serialization failed: '" << e.what() << "'");
    }

    return true;
}
