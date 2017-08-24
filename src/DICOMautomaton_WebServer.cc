//DICOMautomaton_Dispatcher.cc - A part of DICOMautomaton 2016. Written by hal clark.
//
// This program provides a standard entry-point into some DICOMautomaton analysis routines.
//

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>    
#include <vector>
#include <set> 
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
//#include <future>             //Needed for std::async(...)
#include <limits>
#include <cmath>
//#include <cfenv>              //Needed for std::feclearexcept(FE_ALL_EXCEPT).
#include <chrono>

#include <getopt.h>           //Needed for 'getopts' argument parsing.
#include <cstdlib>            //Needed for exit() calls.
#include <cstdio>
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>
#include <experimental/filesystem>

#include <boost/algorithm/string.hpp> //For boost:iequals().

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <Wt/WApplication>
#include <Wt/WBreak>
#include <Wt/WContainerWidget>
#include <Wt/WLineEdit>
#include <Wt/WPushButton>
#include <Wt/WText>
#include <Wt/WFileUpload>
#include <Wt/WProgressBar>
#include <Wt/WGroupBox>
#include <Wt/WSelectionBox>
#include <Wt/WFileResource>
#include <Wt/WAnimation>
#include <Wt/WFlags>

#include "Structs.h"
#include "Imebra_Shim.h"      //Wrapper for Imebra library. Black-boxed to speed up compilation.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"   //Needed for bimap class.
#include "YgorPerformance.h"  //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesPlotting.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorDICOMTools.h"   //Needed for Is_File_A_DICOM_File(...);

#include "Boost_Serialization_File_Loader.h"
#include "DICOM_File_Loader.h"
#include "FITS_File_Loader.h"

#include "Operation_Dispatcher.h"


// This class is instanced for each client. It holds all state for a single session.
class BaseWebServerApplication : public Wt::WApplication {
  public:
    BaseWebServerApplication(const Wt::WEnvironment& env);

  private:

    // ------------------ DICOMautomaton members --------------------

    //The main storage place and manager class for loaded image sets, contours, dose matrices, etc..
    Drover DICOM_data;

    //Lexicon filename, for the Explicator class. This is used in select cases for string translation.
    std::string FilenameLex;

    //User-defined tags which are used for helping to keep track of information not present (or easily available) in the
    // loaded DICOM data. Things like volunteer tracking numbers, information from imaging/scanning sessions, etc..
    std::map<std::string,std::string> InvocationMetadata;

    //A explicit declaration that the user will generate data in an operation.
    bool GeneratingVirtualData = false;

    //A working space specific to this instance. Not truly private: can be read by others.
    std::string InstancePrivateDirectory;
    

    // --------------------- Web widget members ---------------------

    void filesUploaded(void); //Post-upload event.

    void createROISelector(void);
    void createOperationSelector(void);


    // RTSTRUCT selection.
    Wt::WGroupBox *rtstructSelectionGroupBox = nullptr;

    Wt::WText *fgSelectionInstructionText = nullptr;
    Wt::WText *fgSelectionFeedbackText = nullptr;
    Wt::WSelectionBox *fgSelector = nullptr;

    // Highlight operation parameter selection.
    Wt::WGroupBox *highlightParameterSelectionGroupBox = nullptr;


    // Output widgets. 
    Wt::WGroupBox *outputGroupBox = nullptr;

    Wt::WText *outputInstructionText = nullptr;
    Wt::WText *outputFeedbackText = nullptr;
    Wt::WPushButton *computeButton = nullptr;
    Wt::WFileResource *outputFile = nullptr;
    Wt::WAnchor *outputAnchor = nullptr;

    void computeButtonClicked(void);

};


BaseWebServerApplication::BaseWebServerApplication(const Wt::WEnvironment &env) : Wt::WApplication(env){

    // Create a private working directory somewhere.
    while(this->InstancePrivateDirectory.empty()){
        const auto t_now = std::chrono::system_clock::now();
        auto t_now_coarse = std::chrono::system_clock::to_time_t( t_now );

        char t_now_str[100];
        if(!std::strftime(t_now_str, sizeof(t_now_str), "%Y%m%d-%H%M%S", std::localtime(&t_now_coarse))){
            throw std::runtime_error("Unable to get current time. Cannot continue.");
        }

        auto since_epoch = t_now.time_since_epoch();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(since_epoch).count();

        std::stringstream ss;
        ss << "/home/hal/temp_dirs/";
        ss << std::string(t_now_str);
        ss << "-";
        ss << std::setfill('0') << std::setw(9) << nanos;
        ss << "_dose_modification_project/";
        this->InstancePrivateDirectory = ss.str();

        if(Does_Dir_Exist_And_Can_Be_Read(this->InstancePrivateDirectory)){
            this->InstancePrivateDirectory.clear();
            continue;
        }
        
        if(!Create_Dir_and_Necessary_Parents(this->InstancePrivateDirectory)){
            this->InstancePrivateDirectory.clear();
            continue;
        }
    }
    //FUNCINFO("Temp directory is  '" << this->InstancePrivateDirectory << "'");


    //Record all metadata provided by the user.
//for(auto i = 0; i < argc; ++i) InvocationMetadata["Invocation"] += std::string(argv[i]) + " ";

    //Try find a lexicon file if none were provided.
    {
        if(this->FilenameLex.empty()){
            std::list<std::string> trial = { 
                    "20150925_SGF_and_SGFQ_tags.lexicon",
                    "Lexicons/20150925_SGF_and_SGFQ_tags.lexicon",
                    "/usr/share/explicator/lexicons/20150925_20150925_SGF_and_SGFQ_tags.lexicon",
                    "/usr/share/explicator/lexicons/20130319_SGF_filter_data_deciphered5.lexicon",
                    "/usr/share/explicator/lexicons/20121030_SGF_filter_data_deciphered4.lexicon" };
            for(const auto & f : trial) if(Does_File_Exist_And_Can_Be_Read(f)){
                this->FilenameLex = f;
                FUNCINFO("No lexicon was explicitly provided. Using file '" << FilenameLex << "' as lexicon");
                break;
            }
        }
        if(this->FilenameLex.empty()) FUNCERR("Lexicon file not found. Please provide one or see program help for more info");
    }


    //============================================================================================ =================================================

/*
    //Operations to perform on the data.
    std::list<OperationArgPkg> Operations;
    Operations.emplace_back("SFML_Viewer");


    if(!Operation_Dispatcher( DICOM_data, InvocationMetadata, FilenameLex,
                              Operations )){
        FUNCERR("Analysis failed. Cannot continue");
    }


////////////////////////////   
//    // -----------------------------------------------------------------------------------
//    // On-the-fly widgets.
//    // -----------------------------------------------------------------------------------
//    GBs.emplace_back( new Wt::WGroupBox(root()) );
//    GBs.back()->setTitle("Operations");
//
//    GBs.back()->addWidget( new Wt::WPushButton("Dummy button") );
//    GBs.back()->children().back()->setObjectName("blah");
//
//    auto w = reinterpret_cast<Wt::WPushButton *>( root()->find("blah") );
//    if(w == nullptr) throw std::logic_error("Cannot find widget in DOM tree. Cannot continue.");
//    //w->link("Howdy");
////////////////////////////   

*/

    //============================================= Populate Web Widgets =============================================

    // -----------------------------------------------------------------------------------
    // Static widgets.
    // -----------------------------------------------------------------------------------
    this->useStyleSheet("webserver_styles/Forms.css");
    setTitle("DICOMautomaton Web Services");
    {
        auto title = new Wt::WText("DICOMautomaton Web Services", root());
        title->addStyleClass("Title");
    }


    // -----------------------------------------------------------------------------------
    // File upload widgets.
    // -----------------------------------------------------------------------------------
    {
        auto gb = new Wt::WGroupBox("File Upload", root());
        gb->setObjectName("file_upload_gb");
        gb->addStyleClass("DataEntryGroupBlock");

        auto instruct = new Wt::WText("Please select the RTSTRUCT and RTDOSE files to upload.", gb);
        instruct->addStyleClass("InstructionText");

        (void*) new Wt::WBreak(gb);

        auto fileup = new Wt::WFileUpload(gb);
        fileup->setObjectName("file_upload_gb_file_picker");
        fileup->setFileTextSize(50);
        fileup->setProgressBar(new Wt::WProgressBar());
        fileup->setMultiple(true);

        (void*) new Wt::WBreak(gb);

        auto upbutton = new Wt::WPushButton("Upload", gb);

        (void*) new Wt::WBreak(gb);

        auto feedback = new Wt::WText(gb);
        feedback->setObjectName("file_upload_gb_feedback");
        feedback->addStyleClass("FeedbackText");

        // -------

        upbutton->clicked().connect(std::bind([=](){
            if(fileup->canUpload()){
                fileup->upload();
                feedback->setText("<p>File is now uploading.</p>");
            }else{
                feedback->setText("<p>File uploading is not supported by your browser. Cannot continue.</p>");
            }
            upbutton->disable();
            return;
        }));

        // -------

        fileup->fileTooLarge().connect(std::bind([=](int64_t approx_size) -> void {
            std::stringstream ss;
            ss << "<p>One of the selected files is larger than the maximum permissible size. " 
               << "(File size: ~" 
               << approx_size / (1000*1024)  //Strangely B --> kB converted differently to kB --> MB. 
               << " MB.)</p>";
            feedback->setText(ss.str());
            return;
        }, std::placeholders::_1 ));

        //Upload eagerly without waiting for button press. Maybe undesirable.
        //fileup->changed().connect(this, std::bind([=](void){
        //    if(fileup->canUpload()){
        //        fileup->upload();
        //        feedback->setText("File is now uploading.");
        //    }else{
        //        feedback->setText("File uploading is not supported by your browser. Cannot continue.");
        //    }
        //    upbutton->disable();
        //    return;
        //}));

        fileup->uploaded().connect(this, &BaseWebServerApplication::filesUploaded);
 
    }



/*


    // -----------------------------------------------------------------------------------
    // Highlight parameter selection.
    // -----------------------------------------------------------------------------------
    root()->addWidget(new Wt::WBreak());
    this->highlightParameterSelectionGroupBox = new Wt::WGroupBox(root());
    this->highlightParameterSelectionGroupBox->setTitle("Highlight Operation Parameter Selection");

    this->highlightParameterSelectionGroupBox->hide();

    // -----------------------------------------------------------------------------------
    // Output widgets.
    // -----------------------------------------------------------------------------------
    root()->addWidget(new Wt::WBreak());
    this->outputGroupBox = new Wt::WGroupBox(root());
    this->outputGroupBox->setTitle("Output");

    this->computeButton = new Wt::WPushButton("Generate Output", this->outputGroupBox);
    this->outputGroupBox->addWidget(new Wt::WBreak());


    this->computeButton->clicked().connect(this, &BaseWebServerApplication::computeButtonClicked);

    this->outputGroupBox->hide();


    // -----------------------------------------------------------------------------------
    // Styling config.
    // -----------------------------------------------------------------------------------


    // RTSTRUCT Selection.
    this->rtstructSelectionGroupBox->addStyleClass("DataEntryGroupBlock");
    this->fgSelectionInstructionText->addStyleClass("InstructionText");
    this->fgSelectionFeedbackText->addStyleClass("FeedbackText");

    // Highlight parameter selection.
    this->highlightParameterSelectionGroupBox->addStyleClass("DataEntryGroupBlock");

    // Output widgets.
    this->outputGroupBox->addStyleClass("DataEntryGroupBlock");
*/

}



void BaseWebServerApplication::filesUploaded(void){
    // This routine gets called after all files have been uploaded.
    //
    // It must corral, validate, load them into the DICOM_data member, and initiate the next interactive widget(s).
    //

    // Assume ownership of the files so they do not disappear when the connection terminates.
    auto fileup = reinterpret_cast<Wt::WFileUpload *>( root()->find("file_upload_gb_file_picker") );
    if(fileup == nullptr) throw std::logic_error("Cannot find file uploader widget in DOM tree. Cannot continue.");

    std::list<boost::filesystem::path> UploadedFilesDirsReachable;
    const auto files_vec = fileup->uploadedFiles(); //const std::vector< Http::UploadedFile > &
    for(const auto &afile : files_vec){
        afile.stealSpoolFile();
        UploadedFilesDirsReachable.emplace_back(afile.spoolFileName());
    }
    fileup->setProgressBar(new Wt::WProgressBar());
    fileup->disable();


    // Feedback for the client.
    auto feedback = reinterpret_cast<Wt::WText *>( root()->find("file_upload_gb_feedback") );
    if(feedback == nullptr) throw std::logic_error("Cannot find file upload feedback text widget in DOM tree. Cannot continue.");

    std::stringstream ss;
    ss << "<p>" << files_vec.size() << " file(s) have been uploaded. </p>";
    {
        int i = 0;
        for(const auto &afile : files_vec){
            ss << "<p> File " << ++i << ": '" << afile.clientFileName() << "'. </p>";
        }
    }
    feedback->setText(ss.str());
    feedback->setToolTip(ss.str());
    this->processEvents();


    // ======================= Load the files ========================
    {
        root()->addWidget(new Wt::WBreak());

        auto gb = new Wt::WGroupBox("File Loading", root());
        gb->setObjectName("file_loading_gb");
        gb->addStyleClass("DataEntryGroupBlock");

        auto feedback = new Wt::WText(gb);
        feedback->setObjectName("file_loading_gb_feedback");
        feedback->addStyleClass("FeedbackText");
        feedback->setText("<p>Loading files now...</p>");
        this->processEvents();

        //Uploaded file loading: Boost.Serialization archives.
        if(!UploadedFilesDirsReachable.empty()
        && !Load_From_Boost_Serialization_Files( this->DICOM_data,
                                                 this->InvocationMetadata,
                                                 this->FilenameLex,
                                                 UploadedFilesDirsReachable )){
            feedback->setText("<p>Failed to load client-provided Boost.Serialization archive. Instance terminated.</p>");
            return;
        }

        //Uploaded file loading: DICOM files.
        if(!UploadedFilesDirsReachable.empty()
        && !Load_From_DICOM_Files( this->DICOM_data,
                                   this->InvocationMetadata,
                                   this->FilenameLex,
                                   UploadedFilesDirsReachable )){
            feedback->setText("<p>Failed to load client-provided DICOM file. Instance terminated.</p>");
            return;
        }

        //Uploaded file loading: FITS files.
        if(!UploadedFilesDirsReachable.empty()
        && !Load_From_FITS_Files( this->DICOM_data, 
                                  this->InvocationMetadata, 
                                  this->FilenameLex,
                                  UploadedFilesDirsReachable )){
            feedback->setText("<p>Failed to load client-provided FITS file. Instance terminated.</p>");
            return;
        }

        //Other loaders.
        // ...


        //If any standalone files remain, they cannot be loaded.
        if(!UploadedFilesDirsReachable.empty()){
            std::stringstream es;
            feedback->setText("<p>Failed to load client-provided file. Instance terminated.</p>");
            return;
        }

        feedback->setText("<p>Loaded all files successfully. </p>");
        this->processEvents();
    }


    //Create the next widgets for the user to interact with.
    this->createOperationSelector();
    this->createROISelector();

    return;
}


void BaseWebServerApplication::createOperationSelector(void){
    //This routine creates a selector box populated with the available operations.

    root()->addWidget(new Wt::WBreak());

    auto gb = new Wt::WGroupBox("Operation Selection", root());
    gb->setObjectName("op_select_gb");
    gb->addStyleClass("DataEntryGroupBlock");

    auto instruct = new Wt::WText("Please select the Operation of interest.", gb);
    instruct->addStyleClass("InstructionText");

    (void*) new Wt::WBreak(gb);

    auto selector = new Wt::WSelectionBox(gb);
    //selector->setSelectionMode(Wt::ExtendedSelection);
    selector->setVerticalSize(15);
    selector->disable();

    (void*) new Wt::WBreak(gb);

    auto feedback = new Wt::WText(gb);
    feedback->setObjectName("op_select_gb_feedback");
    feedback->addStyleClass("FeedbackText");

    auto known_ops = Known_Operations();
    for(auto &anop : known_ops){
        selector->addItem(anop.first);
    }
    selector->enable();

    this->processEvents();
    return;
}


void BaseWebServerApplication::createROISelector(void){
    //This routine creates a selector box populated with the ROI labels found in the loaded ROIs.

    root()->addWidget(new Wt::WBreak());

    auto gb = new Wt::WGroupBox("ROI Selection", root());
    gb->setObjectName("roi_select_gb");
    gb->addStyleClass("DataEntryGroupBlock");

    auto instruct = new Wt::WText("Please select the ROI(s) of interest, if applicable.", gb);
    instruct->addStyleClass("InstructionText");

    (void*) new Wt::WBreak(gb);

    auto selector = new Wt::WSelectionBox(gb);
    selector->setSelectionMode(Wt::ExtendedSelection);
    selector->setVerticalSize(15);
    selector->disable();

    (void*) new Wt::WBreak(gb);

    auto feedback = new Wt::WText(gb);
    feedback->setObjectName("roi_select_gb_feedback");
    feedback->addStyleClass("FeedbackText");

    this->processEvents();

    //Determine which ROIs are available.

    // ...

                //Populate the selection box with entries.
                //
                // $>  dicomautomaton_dispatcher -o DumpROIData RS.1.2.246.352.71.4.811746149197.1310958.20170809164818.dcm | grep '^DumpROIData' | sed -e "s/.*\tROIName='\([^']*\)'.*/\1/g"
                //
                // 


    selector->addItem("A");
    selector->addItem("B");
    selector->addItem("C");
    selector->enable();

    this->processEvents();
    return;
}

void BaseWebServerApplication::computeButtonClicked(void){
    
    // Clear earlier results, if any exist.
    this->outputGroupBox->clear();
//    if(this->outputAnchor != nullptr){
//        this->outputAnchor->disable();
//    }

    // Gather the selected inputs. (Form data and uploaded files.)


    // Perform the computation.


    // Move the outputs to a client-accessible location.


    // Notify the client that the files can be downloaded.
    this->outputFile = new Wt::WFileResource(this->outputGroupBox);   // NOTE: Use Wt::WStreamResource() instead. More versatile.
    this->outputFile->setMimeType("application/octet-stream");
    this->outputFile->setFileName("/opt/files/output_file.dcm");
    this->outputFile->suggestFileName("RD_modified.dcm");

    this->outputGroupBox->addWidget(new Wt::WBreak());
    this->outputAnchor = new Wt::WAnchor(this->outputFile, 
                                         "DICOM data",
                                         this->outputGroupBox);

    return;
}



Wt::WApplication *createApplication(const Wt::WEnvironment &env){
    return new BaseWebServerApplication(env);
}
int main(int argc, char **argv){
    return Wt::WRun(argc, argv, &createApplication);
}

