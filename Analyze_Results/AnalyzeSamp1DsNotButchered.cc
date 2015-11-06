//AnalyzeSamp1Ds.cc -- Routines for analyzing records stored in the database. 

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <list>
#include <map>
#include <set>
#include <functional>

#include <pqxx/pqxx>         //PostgreSQL C++ interface.

#include <jansson.h>         //For JSON handling.

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorStats.h"
#include "YgorPlot.h"
#include "YgorString.h"
#include "YgorAlgorithms.h"


std::map<std::string,std::string> json_to_stdmap(std::string thejson){
    std::map<std::string,std::string> out;

    json_t *obj = json_loads(thejson.c_str(), 0, nullptr);
    if(!json_is_object(obj)) FUNCERR("JSON document is not as expected(1). Cannot continue");

    const char *key;
    json_t *value;
    json_object_foreach(obj, key, value){
        if(key == nullptr) FUNCERR("Given a nullptr by the json library. (Why?)");
        if(!json_is_string(value)) FUNCERR("JSON document is not as expected(2). Cannot continue");
        const auto map_key = std::string(key);
        const auto map_val = std::string(json_string_value(value));
        out[map_key] = map_val;
    }

    json_decref(obj);
    return out;
}


struct record {
    std::map<std::string,std::string> parameters;
    samples_1D<double> samples;
};


std::string Parameter_Diff(std::vector<struct record> records){
    //Figure out which tags the results differ in.
    if(records.size() < 2) return std::string();

    std::stringstream ss;
    std::map<std::string,std::set<std::string>> all;
    for(auto & record : records){
        for(auto & apair : record.parameters){
            all[apair.first].insert(apair.second);
        }
    }

    for(auto & apair : all){
        if( (apair.first != "Invocation")
        &&  (apair.first != "Title")
        &&  (apair.second.size() != 1) ){
            ss << "\tKey '" << apair.first << "'" << std::endl;
            for(auto & str : apair.second){
                ss << "\t\tValue: '" << str << "'" << std::endl;
            }
        }
    }
    return ss.str();
}



int main(int, char **){
    std::vector<struct record> records;   
 
    try{
        pqxx::connection c("dbname=pacs user=hal host=localhost port=5432");
        pqxx::work txn(c);

        std::stringstream ss;
        ss << "SELECT * FROM samples1D_for_bigart2015 "
           << "WHERE "
           //<< "      (Parameters->>'ROIName' ~* 'Right_Parotid') "
           //<< "      (Parameters->>'ROIName' = 'Left_Parotid') "
           //<< "      ((Parameters->>'ROIName' = 'Left_Parotid') OR (Parameters->>'ROIName' = 'Right_Parotid') OR (Parameters->>'ROIName' ~* 'Masseter'))"
           << "      ((Parameters->>'ROIName' = 'Left_Parotid') OR (Parameters->>'ROIName' = 'Right_Parotid'))"
           //<< "      ((Parameters->>'ROIName' = 'Left_Parotid') OR (Parameters->>'ROIName' = 'Right_Masseter'))"
           //<< "      ((Parameters->>'ROIName' = 'Left_Masseter') OR (Parameters->>'ROIName' = 'Right_Masseter'))"
           //<< "      ((Parameters->>'ROIName' ~* 'Parotid_ANT') OR (Parameters->>'ROIName' ~* 'Parotid_POST'))"
           << "  AND (Parameters->>'Description' ~* ' unnormalized') "
           << "  AND (Parameters->>'SpatialBoxr' = '2') "
           << "  AND (Parameters->>'MinimumDatum' = '3') "
           //<< "  AND (Parameters->>'Volunteer' = '02') "
           //<< "  AND (Parameters->>'DCESession' = '01') "
           << "  AND (Parameters->>'StimulationOccurred' = 'No') "
           //<< "  AND ((Parameters->>'ROIScale') ISNULL) "
           << "  AND ((Parameters->>'ROIScale') NOTNULL) "
           //<< "  AND ((Parameters->>'RowShift') NOTNULL) "
           //<< "  AND ((Parameters->>'ColumnShift') NOTNULL) "
           << "  AND (Parameters->>'RowShift' = '0') "
           << "  AND (Parameters->>'ColumnShift' = '0') "
           << "  AND (Parameters->>'MovingVarianceTwoSidedWidth' = '5') ";
           //<< "LIMIT 50 ";
        FUNCINFO("Executing query:\n\t" << ss.str() << "\n");

        pqxx::result res = txn.exec(ss.str());
        if(res.empty()) FUNCERR("Database query resulted in no records. Cannot continue");

        for(pqxx::result::size_type rownum = 0; rownum != res.size(); ++rownum){
            pqxx::result::tuple row = res[rownum];
            records.emplace_back();
            records.back().parameters = json_to_stdmap( row["Parameters"].as<std::string>() );
            std::stringstream(row["samples_1D"].as<std::string>()) >> records.back().samples;
        }

        //Commit transaction.
        txn.commit();
    }catch(const std::exception &e){
        FUNCWARN("Unable to select contours: exception caught: " << e.what());
    }
    FUNCINFO("Found " << records.size() << " records");

    //Figure out which tags the results differ in.
    if(false && (records.size() > 1)){
        std::cout << "Key-values where records differ: " << std::endl;
        std::cout << Parameter_Diff(records) << std::endl;
    }

    //Generate a descriptive title for each record. Store it in the metadata.
    for(auto & record : records){
        std::stringstream ss;
        ss << record.parameters["ROIName"]
           << " " << record.parameters["Description"]
           << " Vol" << record.parameters["Volunteer"]
           << " DCE" << record.parameters["DCESession"]
           << " Stim" << record.parameters["StimulationOccurred"]
           << " dR" << record.parameters["RowShift"]
           << " dC" << record.parameters["ColumnShift"]
           << " Sx" << record.parameters["ROIScale"];
        const auto title = ReplaceAllInstances( Detox_String( ss.str() ), "[_]", " " );
        record.parameters["Title"] = title;
    }

    //Spit out a plot with all records.
    if(false){
        Plotter2 p;
        for(auto & record : records){
            auto A = record.samples;
            //A = A.Select_Those_Within_Inc(120.0, std::numeric_limits<double>::max());
            A = A.Select_Those_Within_Inc(120.0, 450.0);
            auto AA = A.Moving_Average_Two_Sided_Gaussian_Weighting(2.5);
            p.Insert_samples_1D(A,  record.parameters["Title"], "linespoints");
            p.Insert_samples_1D(AA, "", "lines");
        }
        p.Plot();
        //p.Plot_as_PDF("/tmp/analyze_samples_1d_bigart.pdf");
        return 0;
    }


    //Perform some computations over pairs of records from the same study.
    if(true){  
      long int WPValueSignificant(0), WTotalCount(0);
      long int TPValueSignificant(0), TTotalCount(0);

      //Segregate the records using some criteria.
      std::map<std::string,std::vector<struct record>> seg_records;
      for(auto & record : records){
          const auto seg_key = "StudyInstanceUID "_s + record.parameters["StudyInstanceUID"];
          seg_records[ seg_key ].push_back( record );
      }

      //Now perform the computation over each study group.
      for(auto & seg_records_pair : seg_records){
          auto criteria = seg_records_pair.first;
          auto records  = seg_records_pair.second;

          Plotter2 p, q;
          p.Set_Global_Title(criteria);
          q.Set_Global_Title(criteria + " Histograms of diff(A,B) pre/post stimulation");
          for(size_t i = 1; i < records.size(); ++i){
              for(size_t j = 0; j < i; ++j){

      //            if(records[i].parameters["StimulationLeadTime"] != records[j].parameters["StimulationLeadTime"]) continue;
                  const auto stimleadtimeA = records[i].parameters["StimulationLeadTime"];
                  const auto stimleadtimeB = records[j].parameters["StimulationLeadTime"];
                  std::string stimleadtime_str;
                  if(stimleadtimeA.empty() && stimleadtimeB.empty()){
                      stimleadtime_str = "+300sec";
                  }else{
                      stimleadtime_str = (stimleadtimeA.empty()) ? stimleadtimeB : stimleadtimeA;
                  }
                  const auto stimleadtime = stringtoX<double>(PurgeCharsFromString(stimleadtime_str, "+sec"));
                  FUNCINFO("Stimulation lead time = " << stimleadtime);

                  const auto ROINameA = records[i].parameters["VolunteerName"] + " "_s + records[i].parameters["ROIName"] 
                                                                               + " ROIScale"_s + records[i].parameters["ROIScale"];
                  const auto ROINameB = records[j].parameters["VolunteerName"] + " "_s + records[j].parameters["ROIName"] 
                                                                               + " ROIScale"_s + records[j].parameters["ROIScale"];

                  std::cout << "Key-values where records differ: " << std::endl;
                  std::cout << Parameter_Diff({ records[i], records[j] }) << std::endl;

                  //-------------------------------------------------------------
                  samples_1D<double> A(records[i].samples);
                  auto A_title = records[i].parameters["Title"];
                  samples_1D<double> B(records[j].samples);
                  auto B_title = records[j].parameters["Title"];


                  //Trim the initial peak and trim to the shortest time (450 sec).
                  A = A.Select_Those_Within_Inc(125.0, 450.0);
                  B = B.Select_Those_Within_Inc(125.0, 450.0);

                  //Normalize so the sections before the stimulation point (when the curves should be and are very similar) match.
                  double A_factor, B_factor;
                  {
                    auto A_dummy = A.Select_Those_Within_Inc(std::numeric_limits<double>::min(), stimleadtime);
                    auto B_dummy = B.Select_Those_Within_Inc(std::numeric_limits<double>::min(), stimleadtime);
                    A_factor = A_dummy.Integrate_Over_Kernel_unit(std::numeric_limits<double>::min(),
                                                                  std::numeric_limits<double>::max())[0];
                    B_factor = B_dummy.Integrate_Over_Kernel_unit(std::numeric_limits<double>::min(),
                                                                  std::numeric_limits<double>::max())[0];
                  }
                  A = A.Multiply_With(1.0/A_factor);
                  B = B.Multiply_With(1.0/B_factor);

                  p.Insert_samples_1D(A, A_title, "linespoints");
                  p.Insert_samples_1D(B, B_title, "linespoints");
                  p.Insert_samples_1D(A.Multiply_With(0.0), "", "lines");

                  //Smooth the data for comparison.
                  auto AA = A.Moving_Average_Two_Sided_Gaussian_Weighting(2.5);
                  auto BB = B.Moving_Average_Two_Sided_Gaussian_Weighting(2.5);

                  p.Insert_samples_1D(AA, "", "lines");
                  p.Insert_samples_1D(BB, "", "lines");

                  //Generate the pairs of data for a Wilicoxon sign-rank test.
                  {
                      std::vector<std::array<double,2>> paired_datum;
                      for(auto i = 0; i < A.samples.size(); ++i){
                          auto datumA = A.samples[i];
                          auto datumB = B.samples[i];

                          if(datumA[0] > stimleadtime){
                              paired_datum.push_back( { datumA[2], datumB[2] } );
                          }
                      }

                      const auto Pval_Wilcoxon = Stats::P_From_Paired_Wilcoxon_Signed_Rank_Test_2Tail(paired_datum);
                      std::cout << "\t'" << ROINameA << "' vs. '" << ROINameB << "' : "
                                << "Wilcoxon sign-rank test p-value = " << Pval_Wilcoxon<< std::endl;

                      ++WTotalCount;
                      if(Pval_Wilcoxon < 0.05) ++WPValueSignificant;
                  }

                  //Subtract the curves.
                  auto Diff  = A.Subtract(B); //.Apply_Abs();
                  auto DiffS = Diff.Moving_Average_Two_Sided_Gaussian_Weighting(2.5);

                  p.Insert_samples_1D(Diff, "Difference", "lines");
                  p.Insert_samples_1D(DiffS, "", "lines");

                  //Find the mean and variance before and after the stimulation point.
                  {
                      std::vector<double> Diff_y_pre, Diff_y_post;
                      double num_pre(0.0), num_post(0.0);
                      for(auto & datum : Diff.samples){
                          if(datum[0] < stimleadtime){
                              Diff_y_pre .push_back(datum[2]);
                              num_pre += 1.0;
                          }else{
                              Diff_y_post.push_back(datum[2]);
                              num_post += 1.0;
                          }
                      }
                      const auto pre_mean = Stats::Mean(Diff_y_pre);
                      const auto pre_var  = Stats::Unbiased_Var_Est(Diff_y_pre);
                      const auto post_mean = Stats::Mean(Diff_y_post);
                      const auto post_var  = Stats::Unbiased_Var_Est(Diff_y_post);
                      std::cout << "Pre -stimulation: mean +- sigma_of_mean = " << pre_mean  << " +- " << std::sqrt(pre_var ) << std::endl;
                      std::cout << "Post-stimulation: mean +- sigma_of_mean = " << post_mean << " +- " << std::sqrt(post_var) << std::endl;

                      const auto Pvalue = Stats::P_From_StudT_Diff_Means_From_Uneq_Vars(pre_mean , pre_var , num_pre ,
                                                                                        post_mean, post_var, num_post);
                      std::cout << "\t'" << ROINameA << "' vs. '" << ROINameB << "' : "
                                << "\tTwo-tailed t-test p-value = " << Pvalue << std::endl; 
                      std::cout << std::endl;

                      ++TTotalCount;
                      if(Pvalue < 0.05) ++TPValueSignificant;


                      auto pre_hist  = Bag_of_numbers_to_N_equal_bin_samples_1D_histogram(Diff_y_pre,  10, true);
                      auto post_hist = Bag_of_numbers_to_N_equal_bin_samples_1D_histogram(Diff_y_post, 10, true);
                      q.Insert_samples_1D(pre_hist,  "pre-stim",  "filledcurves");
                      q.Insert_samples_1D(post_hist, "post-stim", "filledcurves");
                  }

                  //-------------------------------------------------------------
              }
          }
          //std::stringstream CustomHeader("set pointsize 0.2");
          //p.Append_Header(&CustomHeader);
          //p.Plot();
          //q.Plot();
          //p.Plot_as_PDF(Detox_String(Title) + ".pdf");
          //std::cout << p.Dump_as_String() << std::endl << std::endl;
      }
      FUNCINFO("Wilcoxon :          " << WPValueSignificant << " of " << WTotalCount << " p-values were significant");
      FUNCINFO("Two-tailed t-tests: " << TPValueSignificant << " of " << TTotalCount << " p-values were significant");

    }

    //Perform some computation over individual records.
    if(false){  
      long int PValueSignificant(0), TotalCount(0);
      //Segregate the records using some criteria.
      std::map<std::string,std::vector<struct record>> seg_records;
      for(auto & record : records){
          const auto seg_key = "StudyInstanceUID "_s + record.parameters["StudyInstanceUID"];
          seg_records[ seg_key ].push_back( record );
      }

      //Now perform the computation over each study group.
      for(auto & seg_records_pair : seg_records){
          auto criteria = seg_records_pair.first;
          auto records  = seg_records_pair.second;

          Plotter2 p, q;
          p.Set_Global_Title(criteria);
          q.Set_Global_Title(criteria + " Histograms of values pre/post stimulation");
          for(size_t i = 0; i < records.size(); ++i){
                 
                  const auto stimleadtimeA = records[i].parameters["StimulationLeadTime"];
                  const auto stimleadtime_str = (stimleadtimeA.empty()) ? "+300sec" : stimleadtimeA;
                  const auto stimleadtime = stringtoX<double>(PurgeCharsFromString(stimleadtime_str, "+sec"));
                  FUNCINFO("Stimulation lead time = " << stimleadtime);

                  const auto ROINameA = records[i].parameters["VolunteerName"] + " "_s + records[i].parameters["ROIName"];

                  //-------------------------------------------------------------
                  samples_1D<double> A(records[i].samples);
                  auto A_title = records[i].parameters["Title"];

                  //Trim the initial peak and trim to the shortest time (450 sec).
                  A = A.Select_Those_Within_Inc(stimleadtime - 100.0, stimleadtime + 100.0);

                  p.Insert_samples_1D(A, A_title, "linespoints");
                  p.Insert_samples_1D(A.Multiply_With(0.0), "", "lines");

                  //Smooth the data for comparison.
                  auto AA = A.Moving_Average_Two_Sided_Gaussian_Weighting(2.5);
                  p.Insert_samples_1D(AA, "", "lines");

                  //Find the mean and variance before and after the stimulation point.
                  {
                      std::vector<double> A_y_pre, A_y_post;
                      double num_pre(0.0), num_post(0.0);
                      for(auto & datum : A.samples){
                          if(datum[0] < stimleadtime){
                              A_y_pre .push_back(datum[2]);
                              num_pre += 1.0;
                          }else{
                              A_y_post.push_back(datum[2]);
                              num_post += 1.0;
                          }
                      }
                      const auto pre_mean = Stats::Mean(A_y_pre);
                      const auto pre_var  = Stats::Unbiased_Var_Est(A_y_pre);
                      const auto post_mean = Stats::Mean(A_y_post);
                      const auto post_var  = Stats::Unbiased_Var_Est(A_y_post);
                      std::cout << "Pre -stimulation: mean +- sigma_of_mean = " << pre_mean  << " +- " << std::sqrt(pre_var ) << std::endl;
                      std::cout << "Post-stimulation: mean +- sigma_of_mean = " << post_mean << " +- " << std::sqrt(post_var) << std::endl;

                      const auto Pvalue = Stats::P_From_StudT_Diff_Means_From_Uneq_Vars(pre_mean , pre_var , num_pre ,
                                                                                        post_mean, post_var, num_post);
                      std::cout << "\t'" << ROINameA << "' : Two-tailed t-test p-value = " << Pvalue << std::endl; 
                      std::cout << std::endl;

                      ++TotalCount;
                      if(Pvalue < 0.05) ++PValueSignificant;

                      auto pre_hist  = Bag_of_numbers_to_N_equal_bin_samples_1D_histogram(A_y_pre,  10, true);
                      auto post_hist = Bag_of_numbers_to_N_equal_bin_samples_1D_histogram(A_y_post, 10, true);
                      q.Insert_samples_1D(pre_hist,  "pre-stim",  "filledcurves");
                      q.Insert_samples_1D(post_hist, "post-stim", "filledcurves");
                  }
                  //-------------------------------------------------------------
              
          }
          //std::stringstream CustomHeader("set pointsize 0.2");
          //p.Append_Header(&CustomHeader);
          //p.Plot();
          //q.Plot();
          //p.Plot_as_PDF(Detox_String(Title) + ".pdf");
          //std::cout << p.Dump_as_String() << std::endl << std::endl;
      }

      FUNCINFO("Two-tailed t-tests: " << PValueSignificant << " of " << TotalCount << " p-values were significant");
    }



    return 0;
}
