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

    Wt::WText *titleText = nullptr;


    std::list<Wt::WGroupBox *> GroupBoxes;


    // File upload widgets.
    Wt::WGroupBox *fileUploadGroupBox = nullptr;

    Wt::WText *fileUploadInstructionText = nullptr;
    Wt::WText *fileUploadFeedbackText = nullptr;
    Wt::WFileUpload *fileUploader = nullptr;
    Wt::WPushButton *fileUploadButton = nullptr;

    void fileTooLarge(int64_t);
    void fileChanged(void);
    void filesUploaded(void);

    void fileUploadButtonClicked(void);

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
FUNCINFO("Temp directory is  '" << this->InstancePrivateDirectory << "'");


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
*/




    //============================================= Populate Web Widgets =============================================

    // Document title.
    setTitle("DICOMautomaton Web Services");

    // Create and register HTML elements.
    this->titleText = new Wt::WText(root());
    this->titleText->setText("DICOMautomaton Web Services");


    // -----------------------------------------------------------------------------------
    // File upload widgets 
    // -----------------------------------------------------------------------------------
    this->fileUploadGroupBox = new Wt::WGroupBox(root());
    this->fileUploadGroupBox->setTitle("File Upload");

    this->fileUploadInstructionText = new Wt::WText(this->fileUploadGroupBox);
    this->fileUploadInstructionText->setText("Please select the RTSTRUCT and RTDOSE files to upload. ");
    this->fileUploadGroupBox->addWidget(new Wt::WBreak());
    this->fileUploader = new Wt::WFileUpload(this->fileUploadGroupBox);
    this->fileUploadGroupBox->addWidget(new Wt::WBreak());
    this->fileUploadButton = new Wt::WPushButton("Upload", this->fileUploadGroupBox);
    this->fileUploadGroupBox->addWidget(new Wt::WBreak());
    this->fileUploadFeedbackText = new Wt::WText(this->fileUploadGroupBox);

    // File uploader config.
    this->fileUploader->setFileTextSize(50); // Set the width of the widget to 50 characters
    this->fileUploader->setProgressBar(new Wt::WProgressBar());
    //this->fileUploader->setMargin(10, Wt::Right);
    this->fileUploader->setMultiple(true);


    // File upload callback registrations.
    this->fileUploadButton->clicked().connect(this, &BaseWebServerApplication::fileUploadButtonClicked);
    this->fileUploader->fileTooLarge().connect(this, &BaseWebServerApplication::fileTooLarge);
    this->fileUploader->changed().connect(this, &BaseWebServerApplication::fileChanged); //Upload eagerly without waiting for button press. Maybe undesirable.
    this->fileUploader->uploaded().connect(this, &BaseWebServerApplication::filesUploaded);


    // -----------------------------------------------------------------------------------
    // RTSTRUCT selection 
    // -----------------------------------------------------------------------------------
    root()->addWidget(new Wt::WBreak());
    this->rtstructSelectionGroupBox = new Wt::WGroupBox(root());
    this->rtstructSelectionGroupBox->setTitle("ROI Selection");

    this->fgSelectionInstructionText = new Wt::WText(this->rtstructSelectionGroupBox);
    this->fgSelectionInstructionText->setText("Please select the ROI(s) you want to isolate. ");
    this->rtstructSelectionGroupBox->addWidget(new Wt::WBreak());
    this->fgSelector = new Wt::WSelectionBox(this->rtstructSelectionGroupBox);
    this->rtstructSelectionGroupBox->addWidget(new Wt::WBreak());
    this->fgSelectionFeedbackText = new Wt::WText(this->rtstructSelectionGroupBox);

    // RTSTRUCT selector config.
    this->fgSelector->setSelectionMode(Wt::ExtendedSelection);
    this->fgSelector->setVerticalSize(15);
    this->fgSelector->disable();

    this->rtstructSelectionGroupBox->hide();


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
    // On-the-fly widgets.
    // -----------------------------------------------------------------------------------
    GroupBoxes.emplace_back( new Wt::WGroupBox(root()) );
    GroupBoxes.back()->setTitle("Operations");

    GroupBoxes.back()->addWidget( new Wt::WPushButton("Dummy button") );
    GroupBoxes.back()->children().back().setObjectName("blah");

    auto w = reinterpret_cast<Wt::WPushButton>( GroupBoxes.find("blah") );
    if(w == nullptr) throw std::logic_error("Cannot find widget in DOM tree. Cannot continue.");
    w->link("Howdy");
    

    // -----------------------------------------------------------------------------------
    // Styling config.
    // -----------------------------------------------------------------------------------

    this->useStyleSheet("Forms.css");
    this->titleText->addStyleClass("Title");

    // File upload widgets.
    this->fileUploadGroupBox->addStyleClass("DataEntryGroupBlock");
    this->fileUploadInstructionText->addStyleClass("InstructionText");

    this->fileUploadFeedbackText->addStyleClass("FeedbackText");
    //this->fileUploader->addStyleClass("centered-example");
    //this->fileUploadButton->addStyleClass("centered-example");

    // RTSTRUCT Selection.
    this->rtstructSelectionGroupBox->addStyleClass("DataEntryGroupBlock");
    this->fgSelectionInstructionText->addStyleClass("InstructionText");
    this->fgSelectionFeedbackText->addStyleClass("FeedbackText");

    // Highlight parameter selection.
    this->highlightParameterSelectionGroupBox->addStyleClass("DataEntryGroupBlock");

    // Output widgets.
    this->outputGroupBox->addStyleClass("DataEntryGroupBlock");

}

void BaseWebServerApplication::fileTooLarge(int64_t approx_size){
    std::stringstream ss;
    ss << "One of the selected files is larger than the maximum permissible size. " 
       << "(File size: ~" 
       << approx_size / (1000*1024)  //Strangely B --> kB converted differently to kB --> MB. 
       << " MB.) ";
    this->fileUploadFeedbackText->setText(ss.str());
    return;
}

void BaseWebServerApplication::fileChanged(void){
    if(this->fileUploader->canUpload()){
        this->fileUploader->upload();
        this->fileUploadFeedbackText->setText("File upload has changed.");
    }else{
        this->fileUploadFeedbackText->setText("File uploading is not supported by your browser. Cannot continue!");
    }
    this->fileUploadButton->disable();
    return;
}

void BaseWebServerApplication::filesUploaded(void){
    //This routine gets called when all files have been successfully uploaded.
    std::stringstream ss;


    //List of filenames or directories to parse and load.
    std::list<boost::filesystem::path> UploadedFilesDirsReachable;

    const auto files_vec = this->fileUploader->uploadedFiles(); //const std::vector< Http::UploadedFile > &
    ss << "<p>" << files_vec.size() << " file(s) have been uploaded. </p>";

    //Assume ownership of the files so they do not disappear when the connection terminates.
    for(const auto &afile : files_vec){
        afile.stealSpoolFile();
        UploadedFilesDirsReachable.emplace_back(afile.spoolFileName());
    }

    // Debugging information.
    {
        int i = 0;
        for(const auto &afile : files_vec){
            ss << "<p> File " << ++i << ": '" << afile.clientFileName() << "'. </p>";
        }
    }

    this->fileUploadFeedbackText->setText(ss.str());
    this->fileUploadFeedbackText->setToolTip(ss.str());
    this->fileUploader->setProgressBar(new Wt::WProgressBar());
    this->fileUploader->disable();  // ?


    // ======================= Load the files ========================

    //Uploaded file loading: Boost.Serialization archives.
    if(!UploadedFilesDirsReachable.empty()
    && !Load_From_Boost_Serialization_Files( this->DICOM_data,
                                             this->InvocationMetadata,
                                             this->FilenameLex,
                                             UploadedFilesDirsReachable )){
        FUNCERR("Failed to load Boost.Serialization archive");
    }

    //Uploaded file loading: DICOM files.
    if(!UploadedFilesDirsReachable.empty()
    && !Load_From_DICOM_Files( this->DICOM_data,
                               this->InvocationMetadata,
                               this->FilenameLex,
                               UploadedFilesDirsReachable )){
        FUNCERR("Failed to load DICOM file");
    }

    //Uploaded file loading: FITS files.
    if(!UploadedFilesDirsReachable.empty()
    && !Load_From_FITS_Files( this->DICOM_data, 
                              this->InvocationMetadata, 
                              this->FilenameLex,
                              UploadedFilesDirsReachable )){
        FUNCERR("Failed to load FITS file");
    }

    //Other loaders ...


    //If any standalone files remain, they cannot be loaded.
    if(!UploadedFilesDirsReachable.empty()){
        FUNCERR("Unable to load file " << UploadedFilesDirsReachable.front() << ". Refusing to continue");
    }


    // =================== Populate operations selector =================










    //Populate the selection box with entries.
    //
    // $>  dicomautomaton_dispatcher -o DumpROIData RS.1.2.246.352.71.4.811746149197.1310958.20170809164818.dcm | grep '^DumpROIData' | sed -e "s/.*\tROIName='\([^']*\)'.*/\1/g"
    //
    // 

    this->fgSelector->addItem("A");
    this->fgSelector->addItem("B");
    this->fgSelector->addItem("C");
    this->fgSelector->enable();

    return;
}

void BaseWebServerApplication::fileUploadButtonClicked(void){
    if(this->fileUploader->canUpload()){
        this->fileUploader->upload();
        this->fileUploadFeedbackText->setText("File is now uploading.");
    }else{
        this->fileUploadFeedbackText->setText("File uploading is not supported by your browser. Cannot continue!");
    }
    this->fileUploadButton->disable();
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

