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
#include <Wt/WCheckBox>
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
#include <Wt/WTable>
#include <Wt/WTableCell>

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

/*
class fileBackedStreamResource : public Wt::WStreamResource {
  public:
    fileBackedStreamResource(std::string fileName, 
                             std::string suggestedFileName,
                             Wt::WObject *parent = 0) : Wt::WStreamResource(parent),
                                                        fileName_(fileName) {
        this->setMimeType("application/octet-stream");
        this->suggestFileName(suggestedFileName);
    }

    ~fileBackedStreamResource(){
        this->beingDeleted();
    }

    std::string getFileName(void){
        return this->fileName_;
    }

    void handleRequest(const Wt::Http::Request &request,
                       Wt::Http::Response &response){
        std::ifstream r(fileName_.c_str(), std::ios::in | std::ios::binary);
        if(r.ok()){
            this->handleRequestPiecewise(request, response, r);
        }else{
            throw std::runtime_error("File not accessible. Computation failed.");
        }
        return;
    }

  private:
    std::string fileName_;
};
*/

static
std::string CreateUniqueDirectoryTimestamped(std::string prefix, std::string postfix){
    std::string out;

    // Create a private working directory somewhere.
    long int i = 0;
    while(out.empty()){
        if(i++ > 5000) throw std::runtime_error("Unable to create unique directory. Do you have adequate permissions?");

        const auto t_now = std::chrono::system_clock::now();
        auto t_now_coarse = std::chrono::system_clock::to_time_t( t_now );

        char t_now_str[100];
        if(!std::strftime(t_now_str, sizeof(t_now_str), "%Y%m%d-%H%M%S", std::localtime(&t_now_coarse))){
            throw std::runtime_error("Unable to get current time.");
        }

        auto since_epoch = t_now.time_since_epoch();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(since_epoch).count();

        std::stringstream ss;
        ss << prefix;
        ss << std::string(t_now_str);
        ss << "-";
        ss << std::setfill('0') << std::setw(9) << nanos;
        ss << postfix;
        out = ss.str();

        if(Does_Dir_Exist_And_Can_Be_Read(out)
        || !Create_Dir_and_Necessary_Parents(out) ){
            out.clear();
        }
    }
    return out;
}

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
    //bool GeneratingVirtualData = false;

    //A working space specific to this instance. Not truly private: can be read by others.
    std::string InstancePrivateDirectory;
    

    //Regex for operation parameters.
    std::regex trueregex    = std::regex("^tr?u?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    std::regex falseregex   = std::regex("^fa?l?s?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    std::regex fnameregex   = std::regex(".*filename.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    std::regex roiregex     = std::regex(".*roi.*label.*regex.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    std::regex normroiregex = std::regex(".*normalized.*roi.*label.*regex.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    
    // --------------------- Web widget shared functors ---------------------

    //void createInvocationMetadataGB(void);
    void createFileUploadGB(void);
    void filesUploaded(void); //Post file upload event.
    void createOperationSelectorGB(void);
    void createOperationParamSelectorGB(void);
    void createComputeGB(void);

};


BaseWebServerApplication::BaseWebServerApplication(const Wt::WEnvironment &env) : Wt::WApplication(env){

    // Create a private working directory somewhere.
    this->InstancePrivateDirectory = CreateUniqueDirectoryTimestamped("/home/hal/DICOMautomaton_Webserver_Artifacts/", // timestamp goes here
                                                                      "_dose_modification_project/");
    FUNCINFO("The unique directory for this session is '" << this->InstancePrivateDirectory << "'");

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
        if(this->FilenameLex.empty()) throw std::runtime_error("Lexicon file not found. Please provide one or see program help for more info");
    }

    // -----------------------------------------------------------------------------------
    // Static widgets and whole-page styling.
    // -----------------------------------------------------------------------------------
    this->useStyleSheet("webserver_styles/Forms.css");
    setTitle("DICOMautomaton Web Services");
    {
        auto title = new Wt::WText("DICOMautomaton Web Services", root());
        title->addStyleClass("Title");
    }

    this->createFileUploadGB();
}


//void BaseWebServerApplication::createInvocationMetadataGB(void){
//
//    //Create a groupbox widget that allows invocation metadata be entered. 
//    //  This is extra info not contained in the DICOM files. Also useful for non-DICOM files.
//    //  Maybe it should be optional? Not really needed at the moment. Eventually though.
//    
//
//    // NOTE: If you are implementing this, begin by copying the Operation Parameter Specification table.
//    //       Also insert it into the dialog widget sequence (probably immediately after loading completes).
//
//    for( each row in the table ){
//        InvocationMetadata[ parameter name ] += user-supplied value;
//    }
//}

void BaseWebServerApplication::createFileUploadGB(void){
    // This routine creates a file upload box.

    auto gb = new Wt::WGroupBox("File Upload", root());
    gb->setObjectName("file_upload_gb");
    gb->addStyleClass("DataEntryGroupBlock");

    auto instruct = new Wt::WText("Please select the RTSTRUCT and RTDOSE files to upload.", gb);
    instruct->addStyleClass("InstructionText");

    (void*) new Wt::WBreak(gb);

    auto fileup = new Wt::WFileUpload(gb);
    fileup->setObjectName("file_upload_gb_file_picker");
    fileup->setFileTextSize(50);
    fileup->setMultiple(true);

    auto pb = new Wt::WProgressBar();
    pb->setWidth(Wt::WLength("100%"));
    fileup->setProgressBar(pb);

//    (void*) new Wt::WBreak(gb);

    auto upbutton = new Wt::WPushButton("Upload", gb);

    (void*) new Wt::WBreak(gb);

    auto feedback = new Wt::WText(gb);
    feedback->setObjectName("file_upload_gb_feedback");
    feedback->addStyleClass("FeedbackText");

    // -------

    upbutton->clicked().connect(std::bind([=](){
        if(fileup->canUpload()){
            fileup->upload();
            feedback->setText("<p>Upload in progress...</p>");
        }else{
            feedback->setText("<p>File uploads are not supported by your browser. Cannot continue.</p>");
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

    upbutton->setCanReceiveFocus(true);
    upbutton->setFocus(true);
    this->processEvents();
    return;
}


void BaseWebServerApplication::filesUploaded(void){
    // This routine gets called after all files have been uploaded.
    //
    // It must corral, validate, load them into the DICOM_data member, and initiate the next interactive widget(s).
    //

    auto fileup = reinterpret_cast<Wt::WFileUpload *>( root()->find("file_upload_gb_file_picker") );
    if(fileup == nullptr) throw std::logic_error("Cannot find file uploader widget in DOM tree. Cannot continue.");

    std::list<boost::filesystem::path> UploadedFilesDirsReachable;
    const auto files_vec = fileup->uploadedFiles(); //const std::vector< Http::UploadedFile > &
    for(const auto &afile : files_vec){
        // Assume ownership of the files so they do not disappear when the connection terminates.
        //
        // NOTE: You'll have to garbage-collect files that you steal. Perhaps by moving them to some infinite storage
        //       location or consuming when loaded into memory?
        //afile.stealSpoolFile();
        UploadedFilesDirsReachable.emplace_back(afile.spoolFileName());

        //Copy the file to the working directory. (Useful for debugging.)
        if(!CopyFile(afile.spoolFileName(), 
                     this->InstancePrivateDirectory + afile.clientFileName())
        && !CopyFile(afile.spoolFileName(), 
                     this->InstancePrivateDirectory + afile.spoolFileName()) ){
            FUNCWARN("Unable to copy uploaded file '" << afile.clientFileName() << "'"
                     << " aka '" << afile.spoolFileName() << "' to archive directory. Continuing");
        }
    }
    auto pb = new Wt::WProgressBar();
    pb->setWidth(Wt::WLength("100%"));
    fileup->setProgressBar(pb);
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

        gb->setCanReceiveFocus(true);
        gb->setFocus(true);
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
    //this->createInvocationMetadataGB();
    this->createOperationSelectorGB();
    return;
}


void BaseWebServerApplication::createOperationSelectorGB(void){
    //This routine creates a selector box populated with the available operations.

    root()->addWidget(new Wt::WBreak());

    auto gb = new Wt::WGroupBox("Operation Selection", root());
    gb->setObjectName("op_select_gb");
    gb->addStyleClass("DataEntryGroupBlock");

    auto instruct = new Wt::WText("Please select the operation of interest.", gb);
    instruct->addStyleClass("InstructionText");

    (void*) new Wt::WBreak(gb);

    auto selector = new Wt::WSelectionBox(gb);
    selector->setObjectName("op_select_gb_selector");
    //selector->setSelectionMode(Wt::ExtendedSelection);
    selector->setVerticalSize(15);
    selector->disable();

    (void*) new Wt::WBreak(gb);

    auto feedback = new Wt::WText(gb);
    feedback->setObjectName("op_select_gb_feedback");
    feedback->addStyleClass("FeedbackText");

    auto known_ops = Known_Operations();
    for(auto &anop : known_ops){
        const auto n = anop.first;
        if( ( n == "HighlightROIs" )
        ||  ( n == "DICOMExportImagesAsDose" )
        ||  ( n == "ConvertDoseToImage" ) 
        ||  ( n == "DecayDoseOverTimeJones2014" ) 
        ||  ( n == "DecayDoseOverTimeHalve" ) 
        ||  ( n == "EvaluateNTCPModels" ) 
        ||  ( n == "EvaluateTCPModels" ) 
        ||  ( n == "RePlanReIrradiateDoseTrimming" ) ){    //Whitelist ... for now.
            selector->addItem(anop.first);
        }
    }
    selector->enable();

    (void*) new Wt::WBreak(gb);

    auto gobutton = new Wt::WPushButton("Proceed", gb);

    // -------

    gobutton->clicked().connect(std::bind([=](){
        //const std::set<int> selected = selector->selectedIndexes();
        //if(selected.empty()) return; // Warn about selecting something?
        if(selector->currentText().empty()) return; // Warn about selecting something?

        gobutton->disable();
        //this->createROISelectorGB();
        this->createOperationParamSelectorGB();
        return;
    }));

    gobutton->setCanReceiveFocus(true);
    gobutton->setFocus(true);
    this->processEvents();
    return;
}

/*
void BaseWebServerApplication::createROISelectorGB(void){
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

    //Determine which ROIs are available.
    std::set<std::string> ROI_labels;
    if(this->DICOM_data.contour_data != nullptr){
        for(auto &cc : this->DICOM_data.contour_data->ccs){
            for(auto &c : cc.contours){
                ROI_labels.insert( c.metadata["ROIName"] );
            }
        }
    }
    for(const auto &l : ROI_labels){
        selector->addItem(l);
    }
    selector->enable();

    (void*) new Wt::WBreak(gb);

    auto gobutton = new Wt::WPushButton("Proceed", gb);
    gobutton->clicked().connect(std::bind([=](){
        //Possible to proceed without selecting any ROI.
        //const std::set<int> selected = selector->selectedIndexes();
        //if(selected.empty()) return; // Warn about selecting something?

        selector->disable();
        gobutton->disable();
        this->createOperationParamSelectorGB();
        return;
    }));

    this->processEvents();
    return;
}
*/

void BaseWebServerApplication::createOperationParamSelectorGB(void){
    //This routine creates a manipulation table populated with tweakable parameters from the specified operation.

    root()->addWidget(new Wt::WBreak());

    auto gb = new Wt::WGroupBox("Operation Parameter Specification", root());
    gb->setObjectName("op_paramspec_gb");
    gb->addStyleClass("DataEntryGroupBlock");

    auto instruct = new Wt::WText("Please specify operation parameters. Hover over for descriptions.", gb);
    instruct->addStyleClass("InstructionText");

//    auto addbutton = new Wt::WPushButton("Add another pass", gb);

    (void*) new Wt::WBreak(gb);

    auto table = new Wt::WTable(gb);
    table->setObjectName("op_paramspec_gb_table");
    table->setHeaderCount(1);
    table->setWidth(Wt::WLength("100%"));
    table->disable();

    (void*) new Wt::WBreak(gb);

    auto feedback = new Wt::WText(gb);
    feedback->setObjectName("op_paramspec_gb_feedback");
    feedback->addStyleClass("FeedbackText");

//    (void*) new Wt::WBreak(gb);

    auto gobutton = new Wt::WPushButton("Proceed", gb);

    // -------

    //table->elementAt(0,0)->addWidget(new Wt::WText("#"));
    table->elementAt(0,0)->addWidget(new Wt::WText(""));
    table->elementAt(0,1)->addWidget(new Wt::WText("Parameter"));
    table->elementAt(0,2)->addWidget(new Wt::WText("Setting"));

    //Determine which ROIs are available, in case they will be needed.
    std::set<std::string> ROI_labels;
    if(this->DICOM_data.contour_data != nullptr){
        for(auto &cc : this->DICOM_data.contour_data->ccs){
            for(auto &c : cc.contours){
                ROI_labels.insert( c.metadata["ROIName"] );
            }
        }
    }
    //Get the selected operation's name.
    auto selector = reinterpret_cast<Wt::WSelectionBox *>( root()->find("op_select_gb_selector") );
    if(selector == nullptr) throw std::logic_error("Cannot find operation selector widget in DOM tree. Cannot continue.");
    const std::string selected_op = selector->currentText().toUTF8(); 

    auto known_ops = Known_Operations();
    int table_row = 1;
    for(auto &anop : known_ops){
        if(anop.first != selected_op) continue; 

        auto optdocs = anop.second.first();
        if(optdocs.empty()){
            feedback->setText("<p>No adjustable options.</p>");
            break;
        }

        for(auto &a : optdocs){
            //Since this is an interactive session, do not expose normalized selections.
            // (This might be useful in some cases though ... change if necessary.)
            if(std::regex_match(a.name,normroiregex)){
                continue;
            }
            
            int col = 0;
            //(void *) new Wt::WText(std::to_string(table_row),   table->elementAt(table_row,col++)); // Row number (cosmetic only).
            (void *) new Wt::WText("",     table->elementAt(table_row,col++)); // Row number (cosmetic only).
            auto pn = new Wt::WText(a.name, table->elementAt(table_row,col++)); // Param name.

            //ROI selection parameters.
            if(false){ // Parameters to expose.
            }else if(std::regex_match(a.name,roiregex)){
                // Instead of a freeform lineedit widget, provide a spinner.
                auto selector = new Wt::WSelectionBox(table->elementAt(table_row,col++));
                selector->setSelectionMode(Wt::ExtendedSelection);
                selector->setVerticalSize(std::min(15,static_cast<int>(ROI_labels.size())));
                selector->disable();
                for(const auto &l : ROI_labels) selector->addItem(l);
                if(!ROI_labels.empty()) selector->enable();

            //Filename parameters are not exposed to the user, but we encode them with a non-visible element.
            }else if(std::regex_match(a.name,fnameregex)){
                 //Hide the parameter name from the user.
                 pn->hide();

                 //Notify that we have to prepare a Wt::WResource for the output.
                 // ...
                 // OK ... Wt appears to be silently discarding WFileResource's added to the table. 
                 // Maybe because it's a non-renderable widget? Who knows. Extremely annoying to debug.
                 // What 'vessel' widget should I stick here instead?   --( Went with a static progress bar ).
                 //
                 //(void *) new Wt::WFileResource("/dev/null", table->elementAt(table_row,col++));
                 //table->elementAt(table_row,col++)->addWidget( reinterpret_cast<Wt::WWidget *>(new Wt::WFileResource()) );
                 auto pb = new Wt::WProgressBar(table->elementAt(table_row,col++)); //Dummy encoding for generated files.
                 pb->hide();

            //Boolean parameters.
            }else if( (a.examples.size() == 2)
                  && ( ( 
                         std::regex_match(a.examples.front(),trueregex)
                         && std::regex_match(a.examples.back(),falseregex) 
                       ) || (
                         std::regex_match(a.examples.front(),falseregex)
                         && std::regex_match(a.examples.back(),trueregex)
                       ) ) ){
                auto cb = new Wt::WCheckBox("", table->elementAt(table_row,col++));
                cb->setChecked( std::regex_match(a.default_val,trueregex) );

            //All other parameters get exposed as free-form text entry boxes.
            }else{
                (void *) new Wt::WLineEdit(a.default_val, table->elementAt(table_row,col++)); // Value to use.

            }

            //Make a tool-tip containing descriptions and examples.
            std::stringstream tooltip_ss;
            tooltip_ss << "<p>" << a.desc << "</p>";
            tooltip_ss << "<p>Examples: <br /><ul>";
            for(auto &e : a.examples) tooltip_ss << "<li>" << e << "</li> "; 
            tooltip_ss << "</ul></p>";

            table->elementAt(table_row,0)->setToolTip(Wt::WString::fromUTF8(tooltip_ss.str()), Wt::XHTMLText);
            table->elementAt(table_row,1)->setToolTip(Wt::WString::fromUTF8(tooltip_ss.str()), Wt::XHTMLText);
            table->elementAt(table_row,2)->setToolTip(Wt::WString::fromUTF8(tooltip_ss.str()), Wt::XHTMLText);

            table->addStyleClass("table");
            table->toggleStyleClass("table-hover", true);
            table->toggleStyleClass("table-condensed", true);

            ++table_row;
        }
    }
    table->enable();

    // -------

    gobutton->clicked().connect(std::bind([=](){
        selector->disable();
        table->disable();
        gobutton->disable();
        this->createComputeGB();
        return;
    }));

    gb->setCanReceiveFocus(true);
    gb->setFocus(true);
    this->processEvents();
    return;
}

void BaseWebServerApplication::createComputeGB(void){
    // This routine creates a panel to both launch an operation and pass the output to the client.
    //
    // The actual computation is performed elsewhere -- this routine merely creates the widgets.
    root()->addWidget(new Wt::WBreak());

    auto gb = new Wt::WGroupBox("Computation", root());
    gb->setObjectName("compute_gb");
    gb->addStyleClass("DataEntryGroupBlock");

    auto feedback = new Wt::WText(gb);
    feedback->setObjectName("compute_gb_feedback");
    feedback->addStyleClass("FeedbackText");
    feedback->setText("<p>Computing now...</p>");

    gb->setCanReceiveFocus(true);
    gb->setFocus(true);
    this->processEvents();


    // Gather the operation and parameters specified.
    auto selector = reinterpret_cast<Wt::WSelectionBox *>( root()->find("op_select_gb_selector") );
    if(selector == nullptr) throw std::logic_error("Cannot find operation selector widget in DOM tree. Cannot continue.");
    const std::string selected_op = selector->currentText().toUTF8(); 
    OperationArgPkg op_args(selected_op);

    auto table = reinterpret_cast<Wt::WTable *>( root()->find("op_paramspec_gb_table") );
    if(table == nullptr) throw std::logic_error("Cannot find operation parameter table widget in DOM tree. Cannot continue.");

    std::map<std::string,Wt::WFileResource *> OutputFiles;
    const auto rows = table->rowCount(); 
    const auto cols = table->columnCount(); 
    if(cols != 3) throw std::logic_error("Table column count changed. Please propagate changes.");
    for(int row = 1; row < rows; ++row){
        const auto param_name = reinterpret_cast<Wt::WText *>(table->elementAt(row,1)->children().back())->text().toUTF8();

        std::string param_val;
        auto w = table->elementAt(row,2)->children().back();
        if(w == nullptr) throw std::logic_error("Table element's child widget not found. Cannot continue.");

        if(false){
        }else if(auto *lineedit = dynamic_cast<Wt::WLineEdit *>(w)){
            param_val = lineedit->text().toUTF8();
        }else if(auto *selector = dynamic_cast<Wt::WSelectionBox *>(w)){
            //Must convert from selected ROI labels to regex.
            std::set<int> selected = selector->selectedIndexes();
            for(const auto &n : selected){
                const auto raw_val = selector->itemText(n).toUTF8();

                std::string shtl;
                for(const auto &c : raw_val){
                    switch(c){ // The following are special in extended regex: . [ \ ( ) * + ? { | ^ $
                        case '+':
                        case '.':
                        case '(':
                        case ')':
                            shtl += "["_s + c + "]"_s;
                            break;
                        case '[':
                        case '\\':
                        case '*':
                        case '?':
                        case '{':
                        case '|':
                        case '^':
                        case '$':
                            shtl += R"***(\)***" + c;
                            break;
                        default:
                            shtl += c;
                            break;
                    }
                }
                param_val += (param_val.empty()) ? shtl 
                                                 : ("|"_s + shtl);
            }

        }else if(dynamic_cast<Wt::WProgressBar *>(w)){ //Dummy encoding for generated files.
            //Create a working file.
            const std::string personal_fname = Get_Unique_Filename(this->InstancePrivateDirectory + "generated_file_", 6);

            auto fr = new Wt::WFileResource(gb);
            fr->setMimeType("application/octet-stream");
            fr->setFileName(personal_fname);
            fr->suggestFileName("output.dcm");
            
            param_val += personal_fname;
            OutputFiles[param_name] = fr;

        }else if(auto *cb = dynamic_cast<Wt::WCheckBox *>(w)){ //Checkbox for Boolean parameters.
            if(cb->isChecked()){
                param_val = "true";
            }else{
                param_val = "false";
            }

        }else{
            throw std::logic_error("Table element's child widget type cannot be identified. Please propagate changes.");
        }
        op_args.insert(param_name, param_val);
    }

    // ---

    //Perform the operation.
    std::list<OperationArgPkg> PackedOperation = { op_args };
    try{
        if(!Operation_Dispatcher( this->DICOM_data, 
                                  this->InvocationMetadata, 
                                  this->FilenameLex,
                                  PackedOperation )){
            throw std::runtime_error("Return value non-zero (non-descript error condition)");
        }
    }catch(const std::exception &e){
        feedback->setText("<p>Operation failed: "_s + e.what() + ".</p>");
        return;
    }
    feedback->setText("<p>Operation successful. </p>");

    gb->setCanReceiveFocus(true);
    gb->setFocus(true);
    this->processEvents();

    // ---

    // Corral the output.
    for(auto &apair : OutputFiles){
        const auto param_name = apair.first;
        auto *fr = apair.second;

        const auto fname = fr->fileName();
        gb->addWidget(new Wt::WBreak());

        //Wt will serve an empty file if the backing file does not exist. This approach is more explicit.
        if(Does_File_Exist_And_Can_Be_Read(fname)){
            (void *) new Wt::WAnchor(fr, "Output file: "_s + param_name, gb);
        }else{
            (void *) new Wt::WText("<p>Output file: "_s + fname + " not available.</p>", gb);
        }
    }
    this->processEvents();

    // ---

//    (void*) new Wt::WBreak(gb);

    auto gobutton = new Wt::WPushButton("Perform another operation", gb);

    gb->setCanReceiveFocus(true);
    gb->setFocus(true);

    gobutton->clicked().connect(std::bind([=](){
        gobutton->disable();

        //Rename all the named entities so that earlier widgets won't interfere with new ones.
        // We don't care about old widgets interfering with each other though.
        //
        // Note: I was unable to walk the DOM/widget tree without segmentation faults.
        //       Explicitly locating each named widget sucks, but it works. Fix if possible.
        //
        std::list<std::string> named = { "file_upload_gb", 
                                         "file_upload_gb_file_picker",
                                         "file_upload_gb_feedback",

                                         "file_loading_gb",
                                         "file_loading_gb_feedback",

                                         "op_select_gb",
                                         "op_select_gb_selector",
                                         "op_select_gb_feedback",

                                         "roi_select_gb",
                                         "roi_select_gb_feedback",

                                         "op_paramspec_gb",
                                         "op_paramspec_gb_table",
                                         "op_paramspec_gb_feedback",

                                         "compute_gb",
                                         "compute_gb_feedback" };

        for(auto &n : named){
            auto w = root()->find(n);
            if(w == nullptr) continue;
            w->setObjectName( w->objectName() + "_OLD" );
        }

        this->processEvents();
        this->createOperationSelectorGB();
        return;
    }));

    gobutton->setCanReceiveFocus(true);
    gobutton->setFocus(true);
    this->processEvents();
    return;
}

Wt::WApplication *createApplication(const Wt::WEnvironment &env){
    return new BaseWebServerApplication(env);
}
int main(int argc, char **argv){
    return Wt::WRun(argc, argv, &createApplication);
}

