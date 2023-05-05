//EQDXTable.cc - A part of DICOMautomaton 2023. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "Explicator.h"

#include "YgorMisc.h"
#include "YgorLog.h"

#include "../Structs.h"
#include "../Metadata.h"
#include "../Regex_Selectors.h"
#include "../String_Parsing.h"
#include "../BED_Conversion.h"
#include "../Tables.h"

#include "EQDXTable.h"



OperationDoc OpArgDocEQDXTable(){
    OperationDoc out;
    out.name = "EQDXTable";

    out.desc = 
        "This operation transforms a given fractionated high dose rate radiotherapy dose to an"
        " Equivalent Dose with 'x'-Dose per fraction (EQDx)."
        " A table with various $\\alpha/\\beta$ and variations is generated."
        " Currently, only photon external beam therapy conversions are supported.";
        
    out.notes.emplace_back(
        "This operation transforms a single scalar dose. For an operation that transforms an image array,"
        " consider the 'BEDConvert' operation."
    );
    out.notes.emplace_back(
        "The default is an 'EQD2' transformation, with 2 Gy per fraction (i.e., EQDx with $x=2$)."
    );
    out.notes.emplace_back(
        "This operation currently assumes a linear-quadratic BED model that disregards time delays,"
        " in particular tissue repopulation. Specifically Withers' formula is used: "
        " $EQD_{x} = nd(d + \\alpha/\\beta)/(x + \\alpha/\\beta)$."
    );


    out.args.emplace_back();
    out.args.back().name = "TargetDosePerFraction";
    out.args.back().desc = "The desired dose per fraction 'x' for an EQDx conversion."
                           "\n\n"
                           " Note that the recommended units are Gy. However, the only requirement is to be"
                           " consistent with the dose parameter's units and the $\\alpha/\\beta$."
                           " For an 'EQD2' conversion, this value should be 2 Gy and the input dose should also be"
                           " in units of Gy. For an 'EQD3.5' conversion, this value should be 3.5 Gy.";
    out.args.back().default_val = "2.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1.8", "2.0", "5.0", "8.0" };


    out.args.emplace_back();
    out.args.back().name = "NumberOfFractions";
    out.args.back().desc = "The number of fractions over which the dose distribution was (or will be) delivered."
                           " Decimal fractions are supported to accommodate multi-pass BED conversions.";
    out.args.back().default_val = "35";
    out.args.back().expected = true;
    out.args.back().examples = { "10", "20.5", "35", "40.123" };


    out.args.emplace_back();
    out.args.back().name = "Dose";
    out.args.back().desc = "The dose to be transformed. It should be a dose that was (or will be) delivered"
                           " (e.g., a point dose delivered to a voxel, or a prescription delivered to a PTV)."
                           "\n\n"
                           " Note that the recommended units are Gy. However, the only requirement is to be"
                           " consistent with the 'x' dose (i.e., the $x$ in EQDx) and the $\\alpha/\\beta$."
                           " For a 70 Gy dose provide the value '70'."
                           "\n\n"
                           " Note that if the dose is a prescription dose, then the result should be considered"
                           " a virtual dose or even a sort of 'ballpark estimate' since the prescribed tissues will"
                           " only nominally receive the prescription dose."
                           " Also note that the specified dose need not actually exist;"
                           " it can be purely virtual to accommodate multiple/compound conversions.";
    out.args.back().default_val = "70";
    out.args.back().expected = true;
    out.args.back().examples = { "5.0", "15", "22.5", "45.0", "66", "70.001" };


    out.args.emplace_back();
    out.args.back().name = "AlphaBetaRatios";
    out.args.back().desc = "A list of $\\alpha/\\beta$ to use, where each $\\alpha/\\beta$ is separated with a ';'."
                           " A conversion will be performed separately for each $\\alpha/\\beta$."
                           "\n\n"
                           " Note that the recommended units are Gy. However, the only requirement is to be"
                           " consistent with the 'x' dose (i.e., the $x$ in EQDx) and the dose parameter's units.";
    out.args.back().default_val = "1;2;3;5;6;8;10";
    out.args.back().expected = true;
    out.args.back().examples = { "2,0", "1;2;3", "0.1;25" };


    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "TableLabel@EQDx";
    

    out.args.emplace_back();
    out.args.back().name = "TableLabel";
    out.args.back().desc = "A label to attach to table if and only if a new table is created.";
    out.args.back().default_val = "EQDx";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "xyz", "sheet A" };

    return out;
}



bool EQDXTable(Drover &DICOM_data,
               const OperationArgPkg& OptArgs,
               std::map<std::string, std::string>& /*InvocationMetadata*/,
               const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TargetDosePerFraction = std::stod(OptArgs.getValueStr("TargetDosePerFraction").value());

    const auto NumberOfFractions = std::stod(OptArgs.getValueStr("NumberOfFractions").value());
    const auto Dose = std::stod(OptArgs.getValueStr("Dose").value());
    const auto ABRs_str = OptArgs.getValueStr("AlphaBetaRatios").value();

    const auto TableLabel = OptArgs.getValueStr("TableLabel").value();
    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    auto STs_all = All_STs( DICOM_data );
    auto STs = Whitelist( STs_all, TableSelectionStr );

    const auto ABRs = parse_numbers(";\n\r\t", ABRs_str);

    if(1UL < STs.size()){
        throw std::invalid_argument("More than one table selected, refusing to continue");

    }else if(NumberOfFractions <= 0){
        throw std::invalid_argument("NumberOfFractions is not valid");

    }else if(TargetDosePerFraction <= 0){
        throw std::invalid_argument("TargetDosePerFraction is not valid");

    }else if(Dose < 0){
        throw std::invalid_argument("Dose is not valid");

    }else if(ABRs.empty()){
        throw std::invalid_argument("No valid alpha/beta provided");

    }
    //-----------------------------------------------------------------------------------------------------------------
    const auto NormalizedTableLabel = X(TableLabel);

    const auto compute_EQDx = []( double d_target,
                                  double n,
                                  double D_point,
                                  double abr ){
        const auto numer = D_point * ((D_point / n) + abr);
        const auto denom = (d_target + abr);
        const auto EQD = numer / denom;
        return EQD;
    };

    const bool create_new_table = STs.empty() || (*(STs.back()) == nullptr);
    std::shared_ptr<Sparse_Table> st = create_new_table ? std::make_shared<Sparse_Table>()
                                                        : *(STs.back());

    // Emit a header.
    int64_t row = st->table.next_empty_row();
    if(create_new_table){
        ++row;
        int64_t col = 1;

        st->table.inject(row, col++, "Dose");
        st->table.inject(row, col++, "n");
        st->table.inject(row, col++, "alpha/beta");
        st->table.inject(row, col++, "EQD"_s + Xtostring(TargetDosePerFraction));
        st->table.inject(row, col++, "EQD"_s + Xtostring(TargetDosePerFraction) + " with Dose -/+ 2%");
        st->table.inject(row, col++, "EQD"_s + Xtostring(TargetDosePerFraction) + " with Dose -/+ 5%");
        st->table.inject(row, col++, "EQD"_s + Xtostring(TargetDosePerFraction) + " with Dose -/+ 10%");
    }

    // Fill in the table.
    for(const auto& abr : ABRs){
        ++row;
        int64_t col = 1;
        const auto EQD    = compute_EQDx(TargetDosePerFraction, NumberOfFractions, Dose, abr);
        const auto EQD_m2 = compute_EQDx(TargetDosePerFraction, NumberOfFractions, Dose * 0.98, abr);
        const auto EQD_p2 = compute_EQDx(TargetDosePerFraction, NumberOfFractions, Dose * 1.02, abr);
        const auto EQD_m5 = compute_EQDx(TargetDosePerFraction, NumberOfFractions, Dose * 0.95, abr);
        const auto EQD_p5 = compute_EQDx(TargetDosePerFraction, NumberOfFractions, Dose * 1.05, abr);
        const auto EQD_mA = compute_EQDx(TargetDosePerFraction, NumberOfFractions, Dose * 0.90, abr);
        const auto EQD_pA = compute_EQDx(TargetDosePerFraction, NumberOfFractions, Dose * 1.10, abr);

        st->table.inject(row, col++, Xtostring(Dose));
        st->table.inject(row, col++, Xtostring(NumberOfFractions));
        st->table.inject(row, col++, Xtostring(abr));
        st->table.inject(row, col++, Xtostring(EQD));
        st->table.inject(row, col++, Xtostring(EQD_m2) + " -- " + Xtostring(EQD_p2));
        st->table.inject(row, col++, Xtostring(EQD_m5) + " -- " + Xtostring(EQD_p5));
        st->table.inject(row, col++, Xtostring(EQD_mA) + " -- " + Xtostring(EQD_pA));
    }

    // Inject the result into the Drover if not already.
    if(create_new_table){
        auto meta = coalesce_metadata_for_basic_table({}, meta_evolve::iterate);
        st->table.metadata = meta;
        st->table.metadata["TableLabel"] = TableLabel;
        st->table.metadata["NormalizedTableLabel"] = NormalizedTableLabel;
        st->table.metadata["Description"] = "Generated table";

        DICOM_data.table_data.emplace_back( st );
    }

    return true;
}
