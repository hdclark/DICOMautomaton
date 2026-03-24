//TrainConditionalForest.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <cstdint>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Write_File.h"

#include "TrainConditionalForest.h"
#include "YgorMath.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...), _s literals.
#include "YgorStatsConditionalForests.h"


OperationDoc OpArgDocTrainConditionalForest(){
    OperationDoc out;
    out.name = "TrainConditionalForest";

    out.tags.emplace_back("category: table processing");
    out.tags.emplace_back("category: statistical modelling");
    out.tags.emplace_back("category: file export");

    out.desc = 
        "This operation trains a conditional random forest regression model using Strobl et al.'s"
        " conditional inference framework. Training data is drawn from the selected table(s)."
        " The table should contain numerical data arranged so that one column is the dependent variable (output)"
        " and the remaining columns are independent variables (features)."
        " The first row is assumed to be a header row."
        " The trained model is serialized and written to the specified file.";

    out.notes.emplace_back(
        "All table cells used for training must contain valid numerical data."
        " Non-numerical cells will cause an error."
    );
    out.notes.emplace_back(
        "The dependent variable column is specified by index (zero-based, excluding the header row)."
        " By default, the last column is treated as the dependent variable."
    );
    out.notes.emplace_back(
        "The conditional forest uses subsampling without replacement (Strobl et al. 2007) and"
        " permutation-based variable selection (Hothorn et al. 2006) to avoid selection bias."
    );

    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The filename to which the trained model will be written."
                           " Leave empty to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/model.cforest", "conditional_forest_model.txt" };

    out.args.emplace_back();
    out.args.back().name = "DependentColumnIndex";
    out.args.back().desc = "The column index (zero-based) of the dependent variable (output)."
                           " If set to -1, the last column in the table is used.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1", "0", "1", "5" };

    out.args.emplace_back();
    out.args.back().name = "NumTrees";
    out.args.back().desc = "The number of decision trees in the forest.";
    out.args.back().default_val = "100";
    out.args.back().expected = true;
    out.args.back().examples = { "10", "50", "100", "500", "1000" };

    out.args.emplace_back();
    out.args.back().name = "MaxDepth";
    out.args.back().desc = "The maximum depth of each decision tree.";
    out.args.back().default_val = "10";
    out.args.back().expected = true;
    out.args.back().examples = { "3", "5", "10", "20", "50" };

    out.args.emplace_back();
    out.args.back().name = "MinSamplesSplit";
    out.args.back().desc = "The minimum number of samples required to split a node.";
    out.args.back().default_val = "2";
    out.args.back().expected = true;
    out.args.back().examples = { "2", "5", "10", "20" };

    out.args.emplace_back();
    out.args.back().name = "Alpha";
    out.args.back().desc = "The significance threshold for the permutation test stopping criterion."
                           " Smaller values result in more conservative splitting (earlier stopping).";
    out.args.back().default_val = "0.05";
    out.args.back().expected = true;
    out.args.back().examples = { "0.01", "0.05", "0.10" };

    out.args.emplace_back();
    out.args.back().name = "NumPermutations";
    out.args.back().desc = "The number of permutations used in the statistical test for variable selection.";
    out.args.back().default_val = "1000";
    out.args.back().expected = true;
    out.args.back().examples = { "100", "500", "1000", "5000" };

    out.args.emplace_back();
    out.args.back().name = "MaxFeatures";
    out.args.back().desc = "The number of features to consider for each split."
                           " If set to -1, all features are considered at each split.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1", "1", "3", "5", "10" };

    out.args.emplace_back();
    out.args.back().name = "SubsampleFraction";
    out.args.back().desc = "The fraction of training samples to use for each tree."
                           " Per Strobl et al. (2007), subsampling without replacement at this fraction"
                           " avoids selection bias toward variables with many categories.";
    out.args.back().default_val = "0.632";
    out.args.back().expected = true;
    out.args.back().examples = { "0.5", "0.632", "0.75", "0.9" };

    out.args.emplace_back();
    out.args.back().name = "CorrelationThreshold";
    out.args.back().desc = "The absolute Pearson correlation threshold for identifying conditioning variables"
                           " in conditional importance estimation."
                           " Features with absolute correlation above this threshold are treated as conditioning"
                           " variables.";
    out.args.back().default_val = "0.20";
    out.args.back().expected = true;
    out.args.back().examples = { "0.10", "0.20", "0.30", "0.50" };

    out.args.emplace_back();
    out.args.back().name = "RandomSeed";
    out.args.back().desc = "The seed for the random number generator."
                           " Use a fixed value for reproducibility.";
    out.args.back().default_val = "42";
    out.args.back().expected = true;
    out.args.back().examples = { "42", "0", "12345" };

    out.args.emplace_back();
    out.args.back().name = "ImportanceMethod";
    out.args.back().desc = "The variable importance estimation method."
                           " Options are 'none', 'permutation', or 'conditional'."
                           " The 'permutation' method uses standard (marginal) out-of-bag permutation importance."
                           " The 'conditional' method (Strobl et al. 2008) permutes features only within groups"
                           " defined by correlated conditioning variables, correcting for bias when predictors"
                           " are correlated.";
    out.args.back().default_val = "none";
    out.args.back().expected = true;
    out.args.back().examples = { "none", "permutation", "conditional" };

    return out;
}

bool TrainConditionalForest(Drover &DICOM_data,
                              const OperationArgPkg& OptArgs,
                              std::map<std::string, std::string>& /*InvocationMetadata*/,
                              const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();
    auto Filename = OptArgs.getValueStr("Filename").value();
    const auto DependentColumnIndex = std::stol( OptArgs.getValueStr("DependentColumnIndex").value() );
    const auto NumTrees = std::stol( OptArgs.getValueStr("NumTrees").value() );
    const auto MaxDepth = std::stol( OptArgs.getValueStr("MaxDepth").value() );
    const auto MinSamplesSplit = std::stol( OptArgs.getValueStr("MinSamplesSplit").value() );
    const auto Alpha = std::stod( OptArgs.getValueStr("Alpha").value() );
    const auto NumPermutations = std::stol( OptArgs.getValueStr("NumPermutations").value() );
    const auto MaxFeatures = std::stol( OptArgs.getValueStr("MaxFeatures").value() );
    const auto SubsampleFraction = std::stod( OptArgs.getValueStr("SubsampleFraction").value() );
    const auto CorrelationThreshold = std::stod( OptArgs.getValueStr("CorrelationThreshold").value() );
    const auto RandomSeed = std::stoull( OptArgs.getValueStr("RandomSeed").value() );
    const auto ImportanceMethodStr = OptArgs.getValueStr("ImportanceMethod").value();
    //-----------------------------------------------------------------------------------------------------------------

    // Parse importance method.
    Stats::ConditionalImportanceMethod imp_method = Stats::ConditionalImportanceMethod::none;
    if(ImportanceMethodStr == "permutation"){
        imp_method = Stats::ConditionalImportanceMethod::permutation;
    }else if(ImportanceMethodStr == "conditional"){
        imp_method = Stats::ConditionalImportanceMethod::conditional;
    }else if(ImportanceMethodStr != "none"){
        throw std::invalid_argument("Unrecognized importance method: '"_s + ImportanceMethodStr + "'");
    }

    // Select tables.
    auto STs_all = All_STs( DICOM_data );
    auto STs = Whitelist( STs_all, TableSelectionStr );
    if(STs.empty()){
        throw std::invalid_argument("No tables selected. Cannot train model.");
    }

    // Extract numerical data from the first selected table.
    auto & table = (*STs.front())->table;
    const auto row_bounds = table.min_max_row();
    const auto col_bounds = table.min_max_col();

    const int64_t header_rows = 1;
    const int64_t data_row_min = row_bounds.first + header_rows;
    const int64_t data_row_max = row_bounds.second;
    const int64_t col_min = col_bounds.first;
    const int64_t col_max = col_bounds.second;

    if(data_row_max < data_row_min){
        throw std::invalid_argument("Table has insufficient data rows for training.");
    }

    const int64_t N = data_row_max - data_row_min + 1; // number of samples
    const int64_t total_cols = col_max - col_min + 1;

    if(total_cols < 2){
        throw std::invalid_argument("Table must have at least two columns (one dependent, one or more independent).");
    }

    // Determine the dependent column.
    const int64_t dep_col = (DependentColumnIndex < 0) ? col_max : (col_min + DependentColumnIndex);
    if(dep_col < col_min || dep_col > col_max){
        throw std::invalid_argument("Dependent column index is out of range.");
    }

    // Build the feature matrix X (NxM) and output vector y (Nx1).
    const int64_t M = total_cols - 1; // number of features (all columns except the dependent)
    num_array<double> X(N, M);
    num_array<double> y(N, 1);

    for(int64_t r = data_row_min; r <= data_row_max; ++r){
        const int64_t sample_idx = r - data_row_min;

        // Extract dependent variable.
        const auto dep_val_opt = table.value(r, dep_col);
        if(!dep_val_opt){
            throw std::invalid_argument("Missing value at row "_s + std::to_string(r) + ", col " + std::to_string(dep_col));
        }
        y.coeff(sample_idx, 0) = std::stod(dep_val_opt.value());

        // Extract features.
        int64_t feat_idx = 0;
        for(int64_t c = col_min; c <= col_max; ++c){
            if(c == dep_col) continue;
            const auto val_opt = table.value(r, c);
            if(!val_opt){
                throw std::invalid_argument("Missing value at row "_s + std::to_string(r) + ", col " + std::to_string(c));
            }
            X.coeff(sample_idx, feat_idx) = std::stod(val_opt.value());
            ++feat_idx;
        }
    }

    // Train the model.
    Stats::ConditionalRandomForests<double> model(NumTrees, MaxDepth, MinSamplesSplit,
                                                   Alpha, NumPermutations, MaxFeatures,
                                                   SubsampleFraction, CorrelationThreshold,
                                                   RandomSeed);

    model.set_importance_method(imp_method);
    model.fit(X, y);

    if(imp_method != Stats::ConditionalImportanceMethod::none){
        model.compute_importance(X, y);
    }

    // Write the model to file.
    if(Filename.empty()){
        Filename = Generate_Unique_tmp_Filename("dcma_conditional_forest_", ".cforest").string();
    }
    {
        std::ofstream of(Filename, std::ios::out | std::ios::binary);
        if(!of){
            throw std::runtime_error("Unable to open file for writing: '"_s + Filename + "'");
        }
        if(!model.write_to(of)){
            throw std::runtime_error("Failed to serialize model to file: '"_s + Filename + "'");
        }
    }
    YLOGINFO("Conditional forest model written to '" << Filename << "'");

    return true;
}
