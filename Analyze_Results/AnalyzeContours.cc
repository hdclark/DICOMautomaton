//AnalyzeContours.cc -- Routines for analyzing records stored in the database. 

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
//#include "YgorMath_Samples.h"
#include "YgorStats.h"
#include "YgorPlot.h"
#include "YgorString.h"
#include "YgorAlgorithms.h"


struct record {
    std::map<std::string,std::string> parameters;
    contour_collection<double> contours;
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
        ss << "SELECT * FROM contours "
           << "WHERE "
           //<< "      (ROIName ~* 'Parotid') "
           << "      (ROIName = 'Left_Parotid') OR (ROIName = 'Right_Parotid') "
           //<< "      (ROIName = 'Left_Parotid') "
           //<< "      (ROIName = 'Right_Parotid') "
           << "LIMIT 50 ";
        FUNCINFO("Executing query:\n\t" << ss.str() << "\n");

        pqxx::result res = txn.exec(ss.str());
        if(res.empty()) FUNCERR("Database query resulted in no records. Cannot continue");

        for(pqxx::result::size_type rownum = 0; rownum != res.size(); ++rownum){
            pqxx::result::tuple row = res[rownum];
            records.emplace_back();

            records.back().parameters[ "ROIName" ] = row["ROIName"].as<std::string>();
            records.back().parameters[ "StudyInstanceUID" ] = row["StudyInstanceUID"].as<std::string>();
            records.back().parameters[ "FrameofReferenceUID" ] = row["FrameofReferenceUID"].as<std::string>();

            if(!records.back().contours.load_from_string( row["ContourCollectionString"].as<std::string>() )){
                FUNCERR("Unable to load contour collection from string");
            }
        }

        //Commit transaction.
        txn.commit();
    }catch(const std::exception &e){
        FUNCWARN("Unable to select contours: exception caught: " << e.what());
    }
    FUNCINFO("Found " << records.size() << " records");

    //Figure out which tags the results differ in.
    if(true && (records.size() > 1)){
        std::cout << "Key-values where records differ: " << std::endl;
        std::cout << Parameter_Diff(records) << std::endl;
    }


    //Compute the mean and spread in contour volumes.
    std::vector<double> ROI_volumes;
    for(auto & record : records){
        //This assumes there is one contour on each slice. 
        //This assumes there are more than three vertices in each contour.
        //This assumes the contours are all of the same orientation.
        //This approach is BRITTLE and can be improved!
 
        const auto A = *std::next(record.contours.contours.front().points.begin(),0); //Three generic points. Hopefully not in a straight line...
        const auto B = *std::next(record.contours.contours.front().points.begin(),1);
        const auto C = *std::next(record.contours.contours.front().points.begin(),2);
        const auto N = (C-B).Cross(A-B).unit();
        std::vector<double> heights;
        for(auto & contour : record.contours.contours){
            auto Centre = contour.Average_Point();
            auto height = Centre.Dot(N);
            heights.push_back(height);
        }
 
        std::sort(heights.begin(), heights.end());
       
        const auto avg_thickness = std::abs((heights.front() - heights.back())/(1.0*(heights.size()-1)));

        double volume = 0.0;
        for(auto & contour : record.contours.contours){
            const auto Centre = contour.Average_Point();
            auto itA = contour.points.begin();
            auto itB = std::next(itA);
            while(itB != contour.points.end()){
                contour_of_points<double> shtl;
                shtl.closed = true;
                shtl.points = { *itB, *itA, Centre };
                const auto darea = std::abs( shtl.Get_Signed_Area() );
                const auto dvolume = darea * avg_thickness;
                volume += dvolume;
                itA = itB;
                std::advance(itB,1);
            }
        }
        ROI_volumes.push_back(volume);
    }


    for(auto & vol : ROI_volumes){
        FUNCINFO("ROI Volume = " << vol);
    }

    FUNCINFO("Mean ROI volume = " << Stats::Mean(ROI_volumes));
    FUNCINFO("Std. Dev. of the mean = " << std::sqrt(Stats::Unbiased_Var_Est(ROI_volumes)));
    FUNCINFO("Median ROI volume = " << Stats::Median(ROI_volumes));


    //Compute the centroid for the contour collection.
    const bool AssumePlanarContours = true;
    for(auto & record : records){
        FUNCINFO("Centroid = " << record.contours.Centroid(AssumePlanarContours));
    }
    //auto cc = contour_collection_sample_ICCR2013();
    //FUNCINFO("CC test Centroid via A = " << cc.Centroid(AssumePlanarContours));
    //FUNCINFO("CC test Centroid via B = " << cc.Centroid());


/*
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
        p.Plot_as_PDF("/tmp/analyze_samples_1d_bigart.pdf");
        return 0;
    }


    //Perform some computations over pairs of records from the same study.
    {  
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
                    auto A_dummy = A.Select_Those_Within_Inc(std::numeric_limits<double>::min(), 300.0);
                    auto B_dummy = B.Select_Those_Within_Inc(std::numeric_limits<double>::min(), 300.0);
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
                          if(datum[0] < 300.0){
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
                      std::cout << "\tP-value = " << Pvalue << std::endl; 
                      std::cout << std::endl;


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
          p.Plot();
          q.Plot();
          //p.Plot_as_PDF(Detox_String(Title) + ".pdf");
          //std::cout << p.Dump_as_String() << std::endl << std::endl;
      }

    }
*/

    return 0;
}
