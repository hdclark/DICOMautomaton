//PlotLineSamples.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <exception>
#include <optional>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include <sstream>
#include <thread>
#include <chrono>

#include "../Insert_Contours.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "PlotLineSamples.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathBSpline.h" //Needed for basis_spline class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)


/*

///////////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Custom plotting apparatus //////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

//This is a simple shuttle class which can be used for a variety of data types. For example, 
// the template can be a samples_1D<T>, a contour_with_points<R>, or some STL container.
template <class C>
struct Shuttle {
    //C const &Data;
    C Data;
    std::string Title;
    std::vector<std::pair<std::string,std::string>> UsingWith;

    //Constructor with some reasonable defaults.
    Shuttle(const C &data, 
            const std::string &title = "", 
            const std::vector<std::pair<std::string,std::string>> &usingwith = 
                 { { "1:2",     "lp"          },
                   { "1:2:3:4", "xyerrorbars" } }
           ) : Data(data), Title(title), UsingWith(usingwith) { };
};



template <typename T> 
inline
void Plot(const std::vector<Shuttle<samples_1D<T>>> &Shuttles,
          const std::string &GlobalTitle = "",
          const std::string &XLabel = "",      // AKA the "abscissa."
          const std::string &YLabel = "",      // AKA the "ordinate."
          const std::vector<std::string> &UserOpts = { } ){ // Generic user options.

    //Gnuplot is very picky about empty data sets and plotting nothing. A window might not be created
    // if such things are passed to Gnuplot. Try to filter them out.
    auto contains_plottable_data = [](const Shuttle<samples_1D<T>> &s) -> bool {
                                      //If there is no data at all.
                                      if(s.Data.size() == 0) return false;

                                      //If there are no finite (plottable) data.
                                      for(const auto &d : s.Data.samples) if(std::isfinite(d[0]) && std::isfinite(d[2])) return true;
                                      return false;
                                  };
    {
        bool PlottableDataPresent = false;
        for(const auto &shtl : Shuttles){
            if(contains_plottable_data(shtl)){
                PlottableDataPresent = true;
                break;
            }
        }
        if(!PlottableDataPresent){
            throw std::invalid_argument("No plottable data. Cannot plot!");
        }
    }

    //FILE *fp = popen("gnuplot --persist ", "w");
    FILE *fp = popen("gnuplot", "w");
    if(fp == nullptr) throw std::runtime_error("Unable to open a pipe to gnuplot");

    fprintf(fp, "reset\n");
    fflush(fp);

    //fprintf(fp, "set size 1.0,1.0\n"); //Width, Height, clamped to [0,1].
    //fprintf(fp, "set autoscale fix\n");
    //fprintf(fp, "set palette defined (0 'black', 1 'white')\n");
    //fprintf(fp, "set palette grey\n");
    //fprintf(fp, "set tics scale 0\n");
    //fprintf(fp, "unset cbtics\n");
    //fprintf(fp, "unset key\n");

    fprintf(fp, "plot sin(x) \n"); // Try to get a window open ASAP.
    fflush(fp);
    std::this_thread::sleep_for(std::chrono::seconds(1));
 
    { 
      const std::string linewidth("1.1"), pointsize("0.4");
      const std::string pt_circ_solid("5"), pt_box_solid("9"), pt_tri_solid("13"), pt_dimnd_solid("11"), pt_invtri_solid("7");
      int N(0);

      fprintf(fp, "set pointsize %s \n", pointsize.c_str());
      fprintf(fp, "set linetype %d lc rgb '#000000' lw %s pt %s \n", ++N, linewidth.c_str(), pt_circ_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#1D4599' lw %s pt %s \n", ++N, linewidth.c_str(), pt_circ_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#11AD34' lw %s pt %s \n", ++N, linewidth.c_str(), pt_circ_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#E62B17' lw %s pt %s \n", ++N, linewidth.c_str(), pt_circ_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#E69F17' lw %s pt %s \n", ++N, linewidth.c_str(), pt_box_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#2F3F60' lw %s pt %s \n", ++N, linewidth.c_str(), pt_box_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#2F6C3D' lw %s pt %s \n", ++N, linewidth.c_str(), pt_box_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#8F463F' lw %s pt %s \n", ++N, linewidth.c_str(), pt_box_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#8F743F' lw %s pt %s \n", ++N, linewidth.c_str(), pt_dimnd_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#031A49' lw %s pt %s \n", ++N, linewidth.c_str(), pt_dimnd_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#025214' lw %s pt %s \n", ++N, linewidth.c_str(), pt_dimnd_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#6D0D03' lw %s pt %s \n", ++N, linewidth.c_str(), pt_dimnd_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#6D4903' lw %s pt %s \n", ++N, linewidth.c_str(), pt_tri_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#224499' lw %s pt %s \n", ++N, linewidth.c_str(), pt_tri_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#00FF00' lw %s pt %s \n", ++N, linewidth.c_str(), pt_tri_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#00BBBB' lw %s pt %s \n", ++N, linewidth.c_str(), pt_invtri_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#000000' lw %s pt %s \n", ++N, linewidth.c_str(), pt_invtri_solid.c_str());
      fprintf(fp, "set linetype %d lc rgb '#000000' lw %s pt %s \n", ++N, linewidth.c_str(), pt_invtri_solid.c_str());
      fprintf(fp, "set linetype cycle %d \n", N); // Now all higher lt := (lt % N).
      fflush(fp);

      //fprintf(fp, "set style increment user \n");  //<-- deprecated!
    }

    fprintf(fp, "set grid lc rgb '#000000' \n");
    fprintf(fp, "set xlabel '%s' \n", XLabel.c_str());
    fprintf(fp, "set ylabel '%s' \n", YLabel.c_str());
    fprintf(fp, "set zero %f \n", std::numeric_limits<float>::denorm_min() );
    //fprintf(fp, "set  \n");
    fflush(fp);

    fprintf(fp, "set title '%s' \n", GlobalTitle.c_str());
    //Could specify the location of the samples_1D we are plotting if wanted:
    // Object with address %s\n', std::tostring(static_cast<intmax_t>((size_t)(std::addressof(*this)))).c_str());
    fflush(fp);

    for(const auto &UserOpt : UserOpts){
        fprintf(fp, "%s \n", UserOpt.c_str());
    }
    fflush(fp);

    //Tell Gnuplot how much to expect and what to do with the coming binary data.
    std::stringstream ss;
    ss << " plot 1/0 title '' ";

    {
        int lt = 0;
        for(const auto &shtl : Shuttles){
            if(!contains_plottable_data(shtl)) continue;

            bool FirstUsingWith = true;
            ++lt;
            const std::string Title = shtl.Title;
            for(const auto &uw : shtl.UsingWith){
                const std::string Using = uw.first;
                const std::string With  = uw.second;
                ss << " , '-' binary record=" << shtl.Data.size() 
                   << R"***( format='%4double' )***"
                   << " using " << Using 
                   << " with " << With
                   << " lt " << lt
                   << " title '" << (FirstUsingWith ? Title : "") << "' ";
                FirstUsingWith = false;
            }
        }
    }
    fprintf(fp, "%s\n", ss.str().c_str()); 
    fflush(fp);


    //Pass Gnuplot the binary data.
    //
    // NOTE: Gnuplot unfortunately requires users to pass binary data across the pipe for each
    //       plot. Sadly, one cannot ask for, say, xyerrorbars and linespoints in the same plot,
    //       so we have to send a fresh copy of the data (in binary) for each specified using-
    //       with pair.
    //
    //       Hopefully someday Gnuplot will honor the [plot '-' ..., '' ...] construct (i.e., 
    //       the "use the same data as the most recently previously-specified source" for the
    //       '-' special file. Currently we are forced to send it again.
    //
    //       Alternatively, hopefully Gnuplot will eventually permit (large, binary) "named 
    //       data" aka "datablocks". Then we could simply do something like:
    //           > $DATA001 binary record=(X,Y) << 
    //           > ... (send the data) ...
    //           > 
    //           > plot $DATA001 using ... with ... , 
    //           >      $DATA001 using ... with ... ,
    //           >      $DATA002 using ... with ... ,
    //           >      ''       using ... with ... 
    //           >
    //           > replot $DATA003 using ... with ... 
    //
    //       which is currently not possible.
    //
    // NOTE: Gnuplot assumes the input data has one of the following layouts:
    //         (x, y, ydelta),
    //         (x, y, ylow, yhigh),
    //         (x, y, xdelta),
    //         (x, y, xlow, xhigh),
    //         (x, y, xdelta, ydelta), or    // <----- This is what we're using.
    //         (x, y, xlow, xhigh, ylow, yhigh).
    //
    //       But samples_1D are laid out like
    //         (x, xdelta, y, ydelta).
    // 
    //       So we need to 'visit' columns in order to emulate the Gnuplot layout.
    //       
    const std::vector<size_t> Element_Order = {0, 2, 1, 3};
    for(const auto &shtl : Shuttles){
        if(!contains_plottable_data(shtl)) continue;

        for(const auto &uw : shtl.UsingWith){
            for(const auto & data_arr : shtl.Data.samples){
                for(const auto & element : Element_Order){
                    auto val = static_cast<double>(data_arr[element]);
                    const size_t res = fwrite(reinterpret_cast<void *>(&val), sizeof(double), 1, fp);
                    if(res != 1) throw std::runtime_error("Unable to write to pipe");
                }
                fflush(fp);
            }
        }
    }
    fflush(fp);

    //Using fork() to wait on the Gnuplot pipe. This requires signal handling, child reaping, 
    // and (more) platform-specific code. Not ideal. On the plus side, it works well and 
    // could be a good fallback.
    //
    //signal(SIGCHLD, SIG_IGN); //Let init reap our children.
    //
    //auto thepid = fork();
    //if((thepid == 0) || (thepid == -1)){
    //    //Close stdin and stdout.
    //    ... iff needed ...
    //    while(true){
    //        std::this_thread::yield();
    //        std::this_thread::sleep_for(std::chrono::seconds(1));        
    //        if(0 > fprintf(fp, "\n")) break; // Keep trying to write until it fails.
    //        fflush(fp);
    //    }
    //    pclose(fp);
    //    exit(0);
    //}


    //Now, in order to keep the Gnuplot window active (responsive), the Gnuplot main process must 
    // continue as long as the user is interested. Instead of blocking our main process, we can 
    // fork and let the child take ownership, or use threads. Going with the latter! 
    //
    // NOTE: Do NOT attempt to access anything other than what you have value captured or thread-
    //       local items. This 'child' thread may run beyond the main thread, so it is possible 
    //       that everything else will be deconstructed while this runs!
    //
    // NOTE: I can't remember right now how to force parallel thread execution. Maybe std::async?
    //       If it is an issue, fix it. It shouldn't really be an issue because the thread just
    //       waits around to close the pipe, but Gnuplot will close the other end at some point.
    //
    // NOTE: I've found that if there are several items in the ThreadWaiter, the main process will
    //       terminate after any one of the threads have joined (actually I think all threads too,
    //       maybe with something like a quick_exit), but the gnuplot process will get reparented
    //       to init. This behaviour is a bit strange, because one would expect all destructors
    //       to be waited on. Well, it seems reasonable and maybe even useful -- if init kills all
    //       children ASAP then closing one window will close all others after the main process
    //       has finished. This would actually be ideal, I think. Still, be careful!
    // 
    auto Wait_and_Close_Pipe = [=](void) -> void {
        while(true){
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::seconds(1));        
            if(0 > fprintf(fp, "\n")) break; // Keep trying to write until it fails.
            fflush(fp);
        }
        pclose(fp);
        return;
    };

    Wait_and_Close_Pipe();
    return;
}
*/



OperationDoc OpArgDocPlotLineSamples(){
    OperationDoc out;
    out.name = "PlotLineSamples";

    out.desc = 
        "This operation plots the selected line samples.";


    out.args.emplace_back();
    out.args.back() = LSWhitelistOpArgDoc();
    out.args.back().name = "LineSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "Title";
    out.args.back().desc = "The title to display in the plot."
                           " Leave empty to disable.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "Line Samples", "Time Series", "DVH for XYZ" };


    out.args.emplace_back();
    out.args.back().name = "AbscissaLabel";
    out.args.back().desc = "The label to attach to the abscissa (i.e., the 'x' or horizontal coordinate)."
                           " Leave empty to disable.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "(arb.)", "Time (s)", "Distance (mm)", "Dose (Gy)" };


    out.args.emplace_back();
    out.args.back().name = "OrdinateLabel";
    out.args.back().desc = "The label to attach to the ordinate (i.e., the 'y' or vertical coordinate)."
                           " Leave empty to disable.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "(arb.)", "Intensity (arb.)", "Volume (mm^3)", "Fraction (arb.)" };


    return out;
}

Drover PlotLineSamples(Drover DICOM_data, const OperationArgPkg& OptArgs, const std::map<std::string,std::string>& /*InvocationMetadata*/, const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto LineSelectionStr = OptArgs.getValueStr("LineSelection").value();
    const auto TitleStr         = OptArgs.getValueStr("Title").value();
    const auto AbscissaLabelStr = OptArgs.getValueStr("AbscissaLabel").value();
    const auto OrdinateLabelStr = OptArgs.getValueStr("OrdinateLabel").value();

    //-----------------------------------------------------------------------------------------------------------------

    auto LSs_all = All_LSs( DICOM_data );
    const auto LSs = Whitelist( LSs_all, LineSelectionStr );
    if(LSs.empty()){
        throw std::invalid_argument("No line samples selected. Cannot continue.");
    }
    FUNCINFO("Attempting to plot " << LSs.size() << " line samples");


    //NOTE: This routine is spotty. It doesn't always work, and seems to have a hard time opening a 
    // display window when a large data set is loaded. Files therefore get written for backup access.
    std::vector<YgorMathPlottingGnuplot::Shuttle<samples_1D<double>>> shuttle;
    //std::vector<Shuttle<samples_1D<double>>> shuttle;
    for(const auto & lsp_it : LSs){
        auto m = (*lsp_it)->line.metadata;
        const auto LineName = (m.count("LineName") == 0) ? "(no name)" : m["LineName"];
        const auto PatientID = (m.count("PatientID") == 0) ? "(no patient ID)" : m["PatientID"];
        const auto ls = (*lsp_it)->line;

        shuttle.emplace_back(ls, PatientID + ": "_s + LineName, 
                             std::vector<std::pair<std::string,std::string>>{{ "1:2", "l"}} );

        const auto fn = Get_Unique_Sequential_Filename("/tmp/dcma_line_sample_",4,".txt");
        ls.Write_To_File(fn);
        AppendStringToFile("# Line sample generated for alternative display: '"_s + LineName + "'.\n", fn);
        FUNCINFO("Line sample course with name '" << LineName << "' written to '" << fn << "'");
    }

    try{
        YgorMathPlottingGnuplot::Plot<double>(shuttle, TitleStr, AbscissaLabelStr, OrdinateLabelStr);
        //Plot<double>(shuttle, "Title here", "X-label here", "Y-label here");
    }catch(const std::exception &e){
        FUNCWARN("Unable to plot line sample: " << e.what());
    }

    return DICOM_data;
}
