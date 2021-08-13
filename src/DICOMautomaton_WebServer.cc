//DICOMautomaton_Webserver.cc - A part of DICOMautomaton 2017, 2018. Written by hal clark.
//
// This program provides a standard entry-point into some DICOMautomaton analysis routines.
//

#include <Wt/Http/Request.h>
#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WBreak.h>
#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WFileResource.h>
#include <Wt/WFileUpload.h>
#include <Wt/WGlobal.h>
#include <Wt/WGroupBox.h>
#include <Wt/WImage.h>
#include <Wt/WJavaScript.h>
#include <Wt/WLength.h>
#include <Wt/WLineEdit.h>
#include <Wt/WLink.h>
#include <Wt/WProgressBar.h>
#include <Wt/WPushButton.h>
#include <Wt/WSelectionBox.h>
#include <Wt/WSignal.h>
#include <Wt/WString.h>
#include <Wt/WTable.h>
#include <Wt/WTableCell.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>
#include <Wt/WWidget.h>
#include <Wt/WAnimation.h>
#include <Wt/WComboBox.h>

#include <filesystem>

#include <algorithm>
//#include <cfenv>              //Needed for std::feclearexcept(FE_ALL_EXCEPT).
#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <type_traits>
#include <utility>            //Needed for std::pair.
#include <vector>

#include "Boost_Serialization_File_Loader.h"
#include "DICOM_File_Loader.h"
#include "FITS_File_Loader.h"
#include "XYZ_File_Loader.h"
#include "Lexicon_Loader.h"
#include "Operation_Dispatcher.h"
#include "Structs.h"
#include "Regex_Selectors.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

namespace Wt {
class WEnvironment;
}  // namespace Wt

static
std::string CreateUniqueDirectoryTimestamped(const std::string& prefix, const std::string& postfix){
    std::string out;

    // Create a private working directory somewhere.
    long int i = 0;
    while(out.empty()){
        if(i++ > 5000) throw std::runtime_error("Unable to create unique directory. Do you have adequate permissions?");

        const auto t_now = std::chrono::system_clock::now();
        auto t_now_coarse = std::chrono::system_clock::to_time_t( t_now );

        char t_now_str[100];
        tm working; // Working space for thread-safe localtime_r() variant of std::localtime().
#if !defined(_WIN32) && !defined(_WIN64)
        if(!std::strftime(t_now_str, sizeof(t_now_str), "%Y%m%d-%H%M%S", ::localtime_r(&t_now_coarse, &working))){
#else
        if( (::localtime_s(&working, &t_now_coarse) != 0)
        ||  (!std::strftime(t_now_str, sizeof(t_now_str), "%Y%m%d-%H%M%S", &working)) ){
#endif
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

static
std::string CamelToHuman(std::string in){
    // This routine endeavours to convert as follows:
    //   ROILabelRegex              --> ROI Label Selector
    //   MinimumJunctionSeparation  --> Minimum Junction Separation
    //   MLCModel                   --> MLC Model
    //   CSVFileName                --> CSV Output File
    //   ROILabelRegexA             --> ROI Label Selector A
    //   EarlyROILabelRegex         --> Early ROI Label Selector
    //

    // We have to preserve case sensitivity for filename-related parameters because they are mapped by name.
    // TODO: support case insensitivity for filename parameters?
    std::regex FName = Compile_Regex("FileName");
    in = std::regex_replace(in, FName, "OutputFile");

    // Ensure there are no existing occurences of 'Selector', which will confound this scheme (i.e., make it
    // non-bidirectional).
    {
        std::regex rgx = std::regex("Selector", std::regex::optimize | std::regex::extended);
        if( std::regex_match(in, rgx) ){
            throw std::logic_error("Term 'Selector' present in input.");
        }
    }

    std::regex rgx = std::regex("Regex", std::regex::optimize | std::regex::extended);
    in = std::regex_replace(in, rgx, R"***(Selector)***");

    std::regex dblcaps = std::regex("([A-Z])([A-Z][a-z])", std::regex::optimize | std::regex::extended);
    in = std::regex_replace(in, dblcaps, R"***($1 $2)***");

    std::regex acap = std::regex("([^A-Z ])([A-Z][a-z])", std::regex::optimize | std::regex::extended);
    in = std::regex_replace(in, acap, R"***($1 $2)***");

    std::regex abut = std::regex("([a-z])([A-Z])", std::regex::optimize | std::regex::extended);
    in = std::regex_replace(in, abut, R"***($1 $2)***");

    std::regex lastcap = std::regex("([^A-Z ])([A-Z])$", std::regex::optimize | std::regex::extended);
    in = std::regex_replace(in, lastcap, R"***($1 $2)***");

    std::regex of = std::regex("(Of)([A-Z])", std::regex::optimize | std::regex::extended);
    in = std::regex_replace(in, of, R"***($1 $2)***");
    return in;
}

static
std::string HumanToCamel(std::string in){
    // This routine is the opposite of CamelToHuman(), and should round-trip endeavours to convert as follows:
    //   ROILabelRegex              <-- ROI Label Selector
    //   MinimumJunctionSeparation  <-- Minimum Junction Separation
    //   MLCModel                   <-- MLC Model
    //   CSVFileName                <-- CSV Output File
    //   ROILabelRegexA             <-- ROI Label Selector A
    //   EarlyROILabelRegex         <-- Early ROI Label Selector
    std::regex rgx = std::regex("Selector", std::regex::optimize | std::regex::extended);
    in = std::regex_replace(in, rgx, R"***(Regex)***");

    std::regex spaces = std::regex("[ ]*", std::regex::optimize | std::regex::extended);
    in = std::regex_replace(in, spaces, "");

    std::regex FName = std::regex("OutputFile", std::regex::icase | std::regex::optimize | std::regex::extended);
    in = std::regex_replace(in, FName, "FileName");

    return in;
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
    std::regex trueregex    = Compile_Regex("^tr?u?e?$");
    std::regex falseregex   = Compile_Regex("^fa?l?s?e?$");
    std::regex fnameregex   = Compile_Regex(".*filename.*");
    std::regex roiregex     = Compile_Regex(".*roi.*label.*regex.*");
    std::regex normroiregex = Compile_Regex(".*normalized.*roi.*label.*regex.*");
    
    // --------------------- Web widget shared functors ---------------------

    //void createInvocationMetadataGB(void);
    void createFileUploadGB();
    void filesUploaded(); //Post file upload event.
    void createOperationSelectorGB();
    void createOperationParamSelectorGB();
    void appendOperationParamsColumn();
    void createComputeGB();

};


BaseWebServerApplication::BaseWebServerApplication(const Wt::WEnvironment &env) : Wt::WApplication(env){

    // Create a private working directory somewhere.
    this->InstancePrivateDirectory = CreateUniqueDirectoryTimestamped("/home/hal/DICOMautomaton_Webserver_Artifacts/", // timestamp goes here
                                                                      "/");
    FUNCINFO("The unique directory for this session is '" << this->InstancePrivateDirectory << "'");

    //Try find a lexicon file if none were provided.
    if(FilenameLex.empty()){
        FilenameLex = Locate_Lexicon_File();
        if(FilenameLex.empty()){
            FUNCINFO("No lexicon was explicitly provided. Using located file '" << FilenameLex << "' as lexicon");
        }
    }
    if(FilenameLex.empty()){
        FUNCINFO("No lexicon provided or located. Attempting to write a default lexicon");
        FilenameLex = Create_Default_Lexicon_File();
        FUNCINFO("Using file '" << FilenameLex << "' as lexicon");
    }

    // -----------------------------------------------------------------------------------
    // Static widgets and whole-page styling.
    // -----------------------------------------------------------------------------------
    this->useStyleSheet("webserver_styles/Forms.css");
    setTitle("DICOMautomaton Web Services");
    {
        auto headercont = root()->addWidget(std::make_unique<Wt::WContainerWidget>());
        headercont->addStyleClass("HeaderCont");

        auto title = headercont->addWidget(std::make_unique<Wt::WText>("DICOMautomaton Web Services"));
        title->addStyleClass("Title");

        auto homeurl = Wt::WLink("http://www.halclark.ca/");
        homeurl.setTarget(Wt::LinkTarget::NewWindow);

        auto hpage = headercont->addWidget(std::make_unique<Wt::WAnchor>(homeurl, "halclark.ca"));
        hpage->addStyleClass("Homepage");

        auto brk = root()->addWidget(std::make_unique<Wt::WBreak>());
        brk->addStyleClass("ClearFix");
    }

    this->createFileUploadGB();
}


void BaseWebServerApplication::createFileUploadGB(){
    // This routine creates a file upload box.

    auto gb = root()->addWidget(std::make_unique<Wt::WGroupBox>("File Upload"));
    gb->setObjectName("file_upload_gb");
    gb->addStyleClass("DataEntryGroupBlock");
    gb->hide();

    auto instruct = gb->addWidget(std::make_unique<Wt::WText>("Please upload DICOM files."));
    instruct->addStyleClass("InstructionText");

    (void*) gb->addWidget(std::make_unique<Wt::WBreak>());

    auto fileup = gb->addWidget(std::make_unique<Wt::WFileUpload>());
    fileup->setObjectName("file_upload_gb_file_picker");
    fileup->setFileTextSize(50);
    fileup->setMultiple(true);

    auto pb = gb->addWidget(std::make_unique<Wt::WProgressBar>());
    pb->setWidth(Wt::WLength("100%"));
    pb->hide();
    fileup->setProgressBar(pb);

    auto upbutton = gb->addWidget(std::make_unique<Wt::WPushButton>("Upload"));

    (void*) gb->addWidget(std::make_unique<Wt::WBreak>());

    auto feedback = gb->addWidget(std::make_unique<Wt::WText>());
    feedback->setObjectName("file_upload_gb_feedback");
    feedback->addStyleClass("FeedbackText");

    auto sep_break = root()->addWidget(std::make_unique<Wt::WBreak>());
    sep_break->setCanReceiveFocus(true);

    // -------

    upbutton->clicked().connect(std::bind([=](){
        if(fileup->canUpload()){
            pb->show();
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

    gb->show();
    sep_break->setFocus(true);
    upbutton->setCanReceiveFocus(true);
    upbutton->setFocus(true);
    this->processEvents();
    return;
}


void BaseWebServerApplication::filesUploaded(){
    // This routine gets called after all files have been uploaded.
    //
    // It must corral, validate, load them into the DICOM_data member, and initiate the next interactive widget(s).
    //

    auto fileup = reinterpret_cast<Wt::WFileUpload *>( root()->find("file_upload_gb_file_picker") );
    if(fileup == nullptr) throw std::logic_error("Cannot find file uploader widget in DOM tree. Cannot continue.");

    std::list<std::filesystem::path> UploadedFilesDirsReachable;
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
        (void*) root()->addWidget(std::make_unique<Wt::WBreak>());

        auto gb = root()->addWidget(std::make_unique<Wt::WGroupBox>("File Loading"));
        gb->setObjectName("file_loading_gb");
        gb->addStyleClass("DataEntryGroupBlock");
        gb->hide();
 
        auto feedback = gb->addWidget(std::make_unique<Wt::WText>());
        feedback->setObjectName("file_loading_gb_feedback");
        feedback->addStyleClass("FeedbackText");
        feedback->setText("<p>Loading files now...</p>");

        auto sep_break = root()->addWidget(std::make_unique<Wt::WBreak>());
        sep_break->setCanReceiveFocus(true);

        gb->show();
        sep_break->setFocus(true);
        gb->setCanReceiveFocus(true);
        gb->setFocus(true);
        gb->setFocus(false);
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

        //Uploaded file loading: XYZ files.
        if(!UploadedFilesDirsReachable.empty()
        && !Load_From_XYZ_Files( this->DICOM_data, 
                                 this->InvocationMetadata, 
                                 this->FilenameLex,
                                 UploadedFilesDirsReachable )){
            feedback->setText("<p>Failed to load client-provided XYZ file. Instance terminated.</p>");
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


void BaseWebServerApplication::createOperationSelectorGB(){
    //This routine creates a selector box populated with the available operations.

    (void*) root()->addWidget(std::make_unique<Wt::WBreak>());

    auto gb = root()->addWidget(std::make_unique<Wt::WGroupBox>("Operation Selection"));
    gb->setObjectName("op_select_gb");
    gb->addStyleClass("DataEntryGroupBlock");
    gb->hide();

    auto instruct = gb->addWidget(std::make_unique<Wt::WText>("Please select the operation to perform."));
    instruct->addStyleClass("InstructionText");

    (void*) gb->addWidget(std::make_unique<Wt::WBreak>());

    auto selectcont = gb->addWidget(std::make_unique<Wt::WContainerWidget>());
    selectcont->addStyleClass("SelectorCont");

    auto grouper = selectcont->addWidget(std::make_unique<Wt::WSelectionBox>());
    grouper->setObjectName("op_select_gb_grouper");
    grouper->setSelectionMode(Wt::SelectionMode::Single);
    grouper->addStyleClass("OperationGrouper");
    grouper->setVerticalSize(15);
    grouper->disable();

    (void*) selectcont->addWidget(std::make_unique<Wt::WBreak>());

    auto selector = selectcont->addWidget(std::make_unique<Wt::WSelectionBox>());
    selector->setObjectName("op_select_gb_selector");
    selector->setSelectionMode(Wt::SelectionMode::Single);
    selector->addStyleClass("OperationSelector");
    selector->setVerticalSize(15);
    selector->disable();

    auto desccont = selectcont->addWidget(std::make_unique<Wt::WContainerWidget>());
    desccont->addStyleClass("OperationSelectorDescriptionPanel");

    auto descpanel = desccont->addWidget(std::make_unique<Wt::WText>());
    descpanel->setObjectName("op_select_gb_descpanel");

    {
        auto brk = gb->addWidget(std::make_unique<Wt::WBreak>());
        brk->addStyleClass("ClearFix");
    }

    auto feedback = gb->addWidget(std::make_unique<Wt::WText>());
    feedback->setObjectName("op_select_gb_feedback");
    feedback->addStyleClass("FeedbackText");

    grouper->addItem("QA");
    grouper->addItem("Planning");
    grouper->addItem("Replans");
    grouper->addItem("Modeling");
    grouper->addItem("Research");
    grouper->addItem("Misc");
    //grouper->addItem("All");

    grouper->enable();
    selector->enable();
    descpanel->enable();

    auto gobutton = gb->addWidget(std::make_unique<Wt::WPushButton>("Proceed"));
    gobutton->disable();

    auto sep_break = root()->addWidget(std::make_unique<Wt::WBreak>());
    sep_break->setCanReceiveFocus(true);

    // -------
    auto proceed = [=](){
        //const std::set<int> selected = selector->selectedIndexes();
        //if(selected.empty()) return; // Warn about selecting something?
        if(selector->currentText().empty()) return; // Warn about selecting something?

        grouper->disable();
        selector->disable();
        gobutton->disable();
        descpanel->disable();
        //this->createROISelectorGB();
        this->createOperationParamSelectorGB();
        return;
    };
    gobutton->clicked().connect(std::bind(proceed));
    selector->doubleClicked().connect(std::bind(proceed));

    auto describe = [=](){
        const std::string selected_op = selector->currentText().toUTF8();
        std::stringstream ss;
        auto known_ops = Known_Operations();
        for(auto &anop : known_ops){
            const auto n = anop.first;
            if( n == selected_op ){
                auto op_docs = (anop.second.first)();
                ss << "<p>" << op_docs.desc << "</p>";
                if(!op_docs.notes.empty()){
                    ss << "<p>Notes: <ul>" << std::endl;
                    for(auto &note : op_docs.notes){
                        ss << "<li>"
                           << note
                           << "</li>"
                           << std::endl;
                    }
                    ss << "</ul></p>";
                }
            }
        }
        descpanel->setText(Wt::WString(ss.str()));
        gobutton->enable();
        return;
    };
    selector->activated().connect(std::bind(describe));

    auto populate = [=](){
        selector->disable();
        selector->clear();
        descpanel->setText(Wt::WString());
        gobutton->disable();
        if(grouper == nullptr) throw std::logic_error("Cannot find operation grouper widget in DOM tree. Cannot continue.");
        const std::string selected_group = grouper->currentText().toUTF8(); 

        auto known_ops = Known_Operations();
        for(auto &anop : known_ops){
            const auto n = anop.first;

            if(selected_group == "QA"){
                if( false
                    ||  ( n == "AnalyzeLightRadFieldCoincidence" ) 
                    ||  ( n == "ApplyCalibrationCurve" )
                    ||  ( n == "ConvertDoseToImage" ) 
                    ||  ( n == "ExtractAlphaBeta" )
                    ||  ( n == "ExtractImageHistograms" )
                    ||  ( n == "FVPicketFence" )
                    ||  ( n == "HighlightROIs" )
                    ||  ( n == "PresentationImage" )
                ){
                    selector->addItem(anop.first);
                }
            }

            if(selected_group == "Planning"){
                if( false
                    ||  ( n == "ConvertDoseToImage" ) 
                    ||  ( n == "CropROIDose" )
                    ||  ( n == "DecayDoseOverTimeHalve" ) 
                    ||  ( n == "DecayDoseOverTimeJones2014" ) 
                    ||  ( n == "DICOMExportImagesAsDose" )
                    ||  ( n == "BEDConvert" )
                    ||  ( n == "ExtractImageHistograms" )
                    ||  ( n == "OptimizeStaticBeams" )
                    ||  ( n == "TrimROIDose" )
                ){
                    selector->addItem(anop.first);
                }
            }

            if(selected_group == "Replans"){
                if( false
                    ||  ( n == "CropROIDose" )
                    ||  ( n == "DecayDoseOverTimeHalve" ) 
                    ||  ( n == "DecayDoseOverTimeJones2014" ) 
                    ||  ( n == "DICOMExportImagesAsDose" )
                    ||  ( n == "BEDConvert" )
                    ||  ( n == "TrimROIDose" )
                ){
                    selector->addItem(anop.first);
                }
            }

            if(selected_group == "Modeling"){
                if( false
                    ||  ( n == "CT_Liver_Perfusion" ) 
                    ||  ( n == "CT_Liver_Perfusion_First_Run" ) 
                    ||  ( n == "CT_Liver_Perfusion_Ortho_Views" ) 
                    ||  ( n == "CT_Liver_Perfusion_Pharmaco_1C2I_5Param" ) 
                    ||  ( n == "CT_Liver_Perfusion_Pharmaco_1C2I_Reduced3Param" ) 
                    ||  ( n == "DCEMRI_IAUC" ) 
                    ||  ( n == "DCEMRI_Nonparametric_CE" ) 
                    ||  ( n == "DumpPerROIParams_KineticModel_1C2I_5P" ) 
                    ||  ( n == "UBC3TMRI_DCE" ) 
                    ||  ( n == "UBC3TMRI_DCE_Differences" ) 
                    ||  ( n == "UBC3TMRI_DCE_Experimental" ) 
                    ||  ( n == "UBC3TMRI_IVIM_ADC" ) 
                ){
                    selector->addItem(anop.first);
                }
            }

            if(selected_group == "Research"){
                if( false
                    ||  ( n == "BCCAExtractRadiomicFeatures" )
                    ||  ( n == "ConvertDoseToImage" ) 
                    ||  ( n == "DetectShapes3D" )
                    ||  ( n == "DICOMExportImagesAsDose" )
                    ||  ( n == "EvaluateNTCPModels" ) 
                    ||  ( n == "EvaluateTCPModels" ) 
                    ||  ( n == "ExtractImageHistograms" )
                    ||  ( n == "GrowContours" )
                    ||  ( n == "HighlightROIs" )
                    ||  ( n == "MinkowskiSum3D" )
                    ||  ( n == "OrderImages" )
                    ||  ( n == "PresentationImage" )
                    ||  ( n == "SeamContours" )
                    ||  ( n == "SimplifyContours" )
                    ||  ( n == "SubtractImages" )
                    ||  ( n == "ThresholdImages" )
                ){
                    selector->addItem(anop.first);
                }
            }

            if(selected_group == "Misc"){
                if( false
                    ||  ( n == "AutoCropImages" )
                    ||  ( n == "Average" )
                    ||  ( n == "BoostSerializeDrover" ) 
                    ||  ( n == "ContourBooleanOperations" )
                    ||  ( n == "ContouringAides" ) 
                    ||  ( n == "ContourSimilarity" )
                    ||  ( n == "ContourViaThreshold" )
                    ||  ( n == "ContourVote" )
                    ||  ( n == "ContourWholeImages" )
                    ||  ( n == "ConvertDoseToImage" ) 
                    ||  ( n == "ConvertImageToDose" )
                    ||  ( n == "ConvertNaNsToAir" )
                    ||  ( n == "ConvertNaNsToZeros" )
                    ||  ( n == "CopyImages" )
                    ||  ( n == "CropImageDoseToROIs" )
                    ||  ( n == "CropImages" )
                    ||  ( n == "DecimatePixels" )
                    ||  ( n == "DICOMExportImagesAsDose" )
                    ||  ( n == "DroverDebug" )
                    ||  ( n == "DumpAllOrderedImageMetadataToFile" )
                    ||  ( n == "DumpAnEncompassedPoint" )
                    ||  ( n == "DumpImageMetadataOccurrencesToFile" )
                    ||  ( n == "DumpPixelValuesOverTimeForAnEncompassedPoint" )
                    ||  ( n == "EvaluateDoseVolumeStats" )
                    ||  ( n == "EvaluateHistograms" )
                    ||  ( n == "ExtractImageHistograms" )
                    ||  ( n == "GenerateSurfaceMask" )
                    ||  ( n == "GenerateVirtualDataContourViaThresholdTestV1" )
                    ||  ( n == "GenerateVirtualDataDoseStairsV1" )
                    ||  ( n == "GenerateVirtualDataPerfusionV1" )
                    ||  ( n == "GiveWholeImageArrayABoneWindowLevel" )
                    ||  ( n == "GiveWholeImageArrayAHeadAndNeckWindowLevel" )
                    ||  ( n == "GiveWholeImageArrayAnAbdominalWindowLevel" )
                    ||  ( n == "GiveWholeImageArrayAThoraxWindowLevel" )
                    ||  ( n == "GridBasedRayCastDoseAccumulate" )
                    ||  ( n == "GrowContours" )
                    ||  ( n == "HighlightROIs" )
                    ||  ( n == "LogScale" )
                    ||  ( n == "MeldDose" )
                    ||  ( n == "MinkowskiSum3D" )
                    ||  ( n == "ModifyContourMetadata" )
                    ||  ( n == "ModifyImageMetadata" )
                    ||  ( n == "OptimizeStaticBeams" )
                    ||  ( n == "OrderImages" )
                    ||  ( n == "PresentationImage" )
                    ||  ( n == "PruneEmptyImageDoseArrays" )
                    ||  ( n == "PurgeContours" )
                    ||  ( n == "SeamContours" )
                    ||  ( n == "SelectSlicesIntersectingROI" )
                    ||  ( n == "SimplifyContours" )
                    ||  ( n == "SpatialBlur" )
                    ||  ( n == "SpatialDerivative" )
                    ||  ( n == "SpatialSharpen" )
                    ||  ( n == "SubtractImages" )
                    ||  ( n == "SupersampleImageGrid" )
                    ||  ( n == "ThresholdImages" )
                ){
                    selector->addItem(anop.first);
                }
            }

            //if(selected_group == "All"){
            //    selector->addItem(anop.first);
            //}
        }
        selector->enable();
        return;
    };
    grouper->activated().connect(std::bind(populate));

    gb->show();
    sep_break->setFocus(true);
    gobutton->setCanReceiveFocus(true);
    gobutton->setFocus(true);
    this->processEvents();
    return;
}

void BaseWebServerApplication::appendOperationParamsColumn(){
    //This routine appends a parameter input column to the operation parameter selection table. 
    // The selected operation will be run once for each additional column.
    // 
    // Note: since this operation is always called at least once, it also performs idempotent
    // post-row/column addition tweaks such as adding tool-tips and hiding irrelevant rows.

    //Get the selected operation's name.
    auto selector = reinterpret_cast<Wt::WSelectionBox *>( root()->find("op_select_gb_selector") );
    if(selector == nullptr) throw std::logic_error("Cannot find operation selector widget in DOM tree. Cannot continue.");
    const std::string selected_op = selector->currentText().toUTF8(); 

    //Get a reference to the table.
    auto table = reinterpret_cast<Wt::WTable *>( root()->find("op_paramspec_gb_table") );
    if(table == nullptr) throw std::logic_error("Cannot find operation parameter table widget in DOM tree. Cannot continue.");

    //Determine which ROIs are available, in case they will be needed.
    std::set<std::string> ROI_labels;
    if(this->DICOM_data.contour_data != nullptr){
        for(auto &cc : this->DICOM_data.contour_data->ccs){
            for(auto &c : cc.contours){
                ROI_labels.insert( c.metadata["ROIName"] );
            }
        }
    }

    //Get a list of the known DICOMautomaton operations.
    auto known_ops = Known_Operations();

    //Get the feedback element.
    auto feedback = reinterpret_cast<Wt::WText *>( root()->find("op_paramspec_gb_feedback") );
    if(feedback == nullptr) throw std::logic_error("Cannot find operation feedback widget in DOM tree. Cannot continue.");


    //Begin altering the table.
    const auto first_run = (table->columnCount() == 0);
    if(first_run){
        (void *) table->elementAt(0,0)->addWidget(std::make_unique<Wt::WText>("Parameter"));
    }
    //const auto rows = table->rowCount(); 
    const auto cols = table->columnCount(); 
    table->elementAt(0,cols)->addWidget(std::make_unique<Wt::WText>("Setting"));

    int table_row = 1;
    for(auto &anop : known_ops){
        if(anop.first != selected_op) continue; 

        auto optdocs = anop.second.first();
        if(optdocs.args.empty()){
            feedback->setText("<p>No parameters to adjust...</p>");
            break;
        }

        for(auto &a : optdocs.args){
            // Do not expose argmuents that have been concealed.
            if(a.visibility == OpArgVisibility::Hide){
                continue;
            }

            //Since this is an interactive session, do not expose normalized selections.
            // (This might be useful in some cases though ... change if necessary.)
            if(std::regex_match(a.name,normroiregex)){
                continue;
            }

            auto pn = table->elementAt(table_row,0); // Param name.
            if(first_run){
                // Generate a display name for the parameter.
                //
                // Note: Filenames are not renamed because they refer to specific resources.
                const auto param_name = (a.mimetype.empty()) ? CamelToHuman(a.name)
                                                             : a.name;
                (void *) pn->addWidget(std::make_unique<Wt::WText>( param_name ));
            }

            //ROI selection parameters.
            if(std::regex_match(a.name,roiregex)){
                // Instead of a freeform lineedit widget, provide a spinner.
                auto spinner = table->elementAt(table_row,cols)->addWidget(std::make_unique<Wt::WSelectionBox>());
                spinner->setSelectionMode(Wt::SelectionMode::Extended);
                spinner->setVerticalSize(std::min(15,static_cast<int>(ROI_labels.size())));
                spinner->disable();
                for(const auto &l : ROI_labels) spinner->addItem(l);
                if(!ROI_labels.empty()) spinner->enable();

            //Boolean parameters.
            }else if( (a.examples.size() == 2)
                  && ( ( 
                         std::regex_match(a.examples.front(),trueregex)
                         && std::regex_match(a.examples.back(),falseregex) 
                       ) || (
                         std::regex_match(a.examples.front(),falseregex)
                         && std::regex_match(a.examples.back(),trueregex)
                       ) ) ){
                auto cb = table->elementAt(table_row,cols)->addWidget(std::make_unique<Wt::WCheckBox>(""));
                cb->setChecked( std::regex_match(a.default_val,trueregex) );

            //Parameters with exhaustive examples, which are best shown with combo boxes.
            }else if(a.samples == OpArgSamples::Exhaustive){
                auto combo = table->elementAt(table_row,cols)->addWidget(std::make_unique<Wt::WComboBox>());
                combo->disable();
                for(const auto &e : a.examples){
                    combo->addItem(e);
                    if(e == a.default_val) combo->setCurrentIndex( combo->count() - 1 );
                }
                if(!a.examples.empty()) combo->enable();

            //Filename parameters are not exposed to the user, but we encode them with a non-visible element.
            }else if(std::regex_match(a.name,fnameregex)){
                 //Hide the parameter name from the user.
                 //pn->hide();

                 //Notify that we have to prepare a Wt::WResource for the output.
                 // ...
                 // OK ... Wt appears to be silently discarding WFileResource's added to the table. 
                 // Maybe because it's a non-renderable widget? Who knows. Extremely annoying to debug.
                 // What 'vessel' widget should I stick here instead?   --( Went with a static progress bar ).
                 //
                 //(void *) table->elementAt(table_row,cols)->addWidget(std::make_unique<Wt::WFileResource>("/dev/null"));
                 //table->elementAt(table_row,cols)->addWidget( reinterpret_cast<Wt::WWidget *>(new Wt::WFileResource()) );
                 auto pb = table->elementAt(table_row,cols)->addWidget(std::make_unique<Wt::WProgressBar>()); //Dummy encoding for generated files.
                 pb->hide();

                 auto tr = table->rowAt(table_row);
                 tr->hide();

            //All other parameters get exposed as free-form text entry boxes.
            }else{
                (void *) table->elementAt(table_row,cols)->addWidget(std::make_unique<Wt::WLineEdit>(a.default_val)); // Value to use.

            }

            //Make a tool-tip containing descriptions and examples. Attach it to the parameter name column.
            std::stringstream tooltip_ss;
            tooltip_ss << "<p>" << a.desc << "</p>";
            tooltip_ss << "<p>Examples: <br /><ul>";
            for(auto &e : a.examples) tooltip_ss << "<li>" << e << "</li> "; 
            tooltip_ss << "</ul></p>";

            table->elementAt(table_row, 0  )->setToolTip(Wt::WString::fromUTF8(tooltip_ss.str()), Wt::TextFormat::XHTML);

            ++table_row;
        }
    }
    this->processEvents();
    return;
}


void BaseWebServerApplication::createOperationParamSelectorGB(){
    //This routine creates a manipulation table populated with tweakable parameters from the specified operation.

    (void*) root()->addWidget(std::make_unique<Wt::WBreak>());

    auto gb = root()->addWidget(std::make_unique<Wt::WGroupBox>("Operation Parameter Specification"));
    gb->setObjectName("op_paramspec_gb");
    gb->addStyleClass("DataEntryGroupBlock");
    gb->hide();

    auto instruct = gb->addWidget(std::make_unique<Wt::WText>("Please specify parameters. Hover for descriptions."));
    instruct->addStyleClass("InstructionText");

    auto addbutton = gb->addWidget(std::make_unique<Wt::WPushButton>("Add another pass"));

    (void*) gb->addWidget(std::make_unique<Wt::WBreak>());

    auto table = gb->addWidget(std::make_unique<Wt::WTable>());
    table->setObjectName("op_paramspec_gb_table");
    table->setHeaderCount(1);
    table->setWidth(Wt::WLength("100%"));
    table->disable();

    (void*) gb->addWidget(std::make_unique<Wt::WBreak>());

    auto feedback = gb->addWidget(std::make_unique<Wt::WText>());
    feedback->setObjectName("op_paramspec_gb_feedback");
    feedback->addStyleClass("FeedbackText");

    auto gobutton = gb->addWidget(std::make_unique<Wt::WPushButton>("Proceed"));

    auto sep_break = root()->addWidget(std::make_unique<Wt::WBreak>());
    sep_break->setCanReceiveFocus(true);

    // -------

    this->appendOperationParamsColumn();
    table->enable();

    // -------

    addbutton->clicked().connect(std::bind([=](){
        this->appendOperationParamsColumn();
        return;
    }));

    gobutton->clicked().connect(std::bind([=](){
        addbutton->disable();
        table->disable();
        gobutton->disable();
        this->createComputeGB();
        return;
    }));

    gb->show();
    sep_break->setFocus(true);
    gb->setCanReceiveFocus(true);
    gb->setFocus(true);
    gb->setFocus(false);
    this->processEvents();
    return;
}

void BaseWebServerApplication::createComputeGB(){
    // This routine creates a panel to both launch an operation and pass the output to the client.
    //
    // The actual computation is performed elsewhere -- this routine merely creates the widgets.
    (void*) root()->addWidget(std::make_unique<Wt::WBreak>());

    auto gb = root()->addWidget(std::make_unique<Wt::WGroupBox>("Computation"));
    gb->setObjectName("compute_gb");
    gb->addStyleClass("DataEntryGroupBlock");
    gb->hide();

    auto feedback = gb->addWidget(std::make_unique<Wt::WText>());
    feedback->setObjectName("compute_gb_feedback");
    feedback->addStyleClass("FeedbackText");
    feedback->setText("<p>Computing now...</p>");

    auto sep_break = root()->addWidget(std::make_unique<Wt::WBreak>());
    sep_break->setCanReceiveFocus(true);

    gb->show();
    sep_break->setFocus(true);
    gb->setCanReceiveFocus(true);
    gb->setFocus(true);
    gb->setFocus(false);
    this->processEvents();

    // Gather the operation and parameters specified.
    auto selector = reinterpret_cast<Wt::WSelectionBox *>( root()->find("op_select_gb_selector") );
    if(selector == nullptr) throw std::logic_error("Cannot find operation selector widget in DOM tree. Cannot continue.");
    const std::string selected_op = selector->currentText().toUTF8(); 

    auto table = reinterpret_cast<Wt::WTable *>( root()->find("op_paramspec_gb_table") );
    if(table == nullptr) throw std::logic_error("Cannot find operation parameter table widget in DOM tree. Cannot continue.");

    std::map<std::string,std::shared_ptr<Wt::WFileResource>> OutputFiles;
    std::map<std::string,std::string> OutputFilenames;
    std::map<std::string,std::string> OutputMimetype;
    const auto rows = table->rowCount(); 
    const auto cols = table->columnCount(); 
    bool AllSuccessful = true;
    for(auto col = 1; col < cols; ++col){
        auto op_doc_l = (Known_Operations()[selected_op].first)(); // Documentation parameter list.
        OperationArgPkg op_args(selected_op); // The list of parameters passed to the operation.
        for(int row = 1; row < rows; ++row){
            const auto param_human_name = reinterpret_cast<Wt::WText *>(table->elementAt(row,0)->children().back())->text().toUTF8();
            const auto param_name = HumanToCamel(param_human_name);
    

            //Find documentation for the current parameter.
            OperationArgDoc op_doc;
            for(const auto &o : op_doc_l.args){
                if(o.name == param_name) op_doc = o;
            }

            //Reject this argument if the parameter has been concealed.
            // This will hopefully prevent the user being able to provide parameters via DOM alteration.
            if(op_doc.visibility == OpArgVisibility::Hide){
                op_args.insert(op_doc.name, op_doc.default_val);
                continue;
            }

            std::string param_val;
            auto w = table->elementAt(row,col)->children().back();
            if(w == nullptr) throw std::logic_error("Table element's child widget not found. Cannot continue.");

            if(auto *lineedit = dynamic_cast<Wt::WLineEdit *>(w)){
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
            }else if(auto *combo = dynamic_cast<Wt::WComboBox *>(w)){
                //Validate that the string is one of the possible options.
                const std::string web_str = combo->currentText().toUTF8();
                {
                    bool is_valid = false;
                    for(const auto &e : op_doc.examples) if(e == web_str) is_valid = true;
                    if(!is_valid) throw std::invalid_argument("Combo box received invalid argument");
                }
                param_val = web_str;

            }else if(dynamic_cast<Wt::WProgressBar *>(w)){ //Dummy encoding for generated files.
                OutputMimetype[param_name] = op_doc.mimetype;

                //Create a working file.
                //
                // Note: for multi-run operations, the same output file MUST be used. This is so that the operation
                //       can string the data together into a meaningful way. Otherwise the user will get several 
                //       files back and there is little point in performing a multi-run operation.
                std::string personal_fname;
                if(OutputFilenames.count(param_name) == 0){
                    std::string suffix;
                    if(op_doc.mimetype == "application/dicom"){
                         suffix = ".dcm";
                    }else if(op_doc.mimetype == "text/plain"){
                         suffix = ".txt";
                    }else if(op_doc.mimetype == "text/csv"){
                         suffix = ".csv";
                    }else if(op_doc.mimetype == "application/obj"){
                         suffix = ".obj";
                    }else if(op_doc.mimetype == "application/mtl"){
                         suffix = ".mtl";
                    }else if(op_doc.mimetype == "image/png"){
                         suffix = ".png";
                    }else if(op_doc.mimetype == "image/fits"){
                         suffix = ".fits";
                    }
                    personal_fname = Get_Unique_Filename(this->InstancePrivateDirectory + "generated_file_", 6, suffix);
                    OutputFilenames[param_name] = personal_fname;
                }else{
                    personal_fname = OutputFilenames[param_name];
                }
                param_val += personal_fname;

                //Only create a file resource for the final pass.
                if((col+1) == cols){
                    auto fr = std::make_shared<Wt::WFileResource>(); 
                    fr->setFileName(personal_fname);
                    fr->setMimeType(op_doc.mimetype);

                    if(op_doc.mimetype == "application/dicom"){
                        fr->suggestFileName("output.dcm");
                    }else if(op_doc.mimetype == "text/plain"){
                        fr->suggestFileName("output.txt");
                    }else if(op_doc.mimetype == "text/csv"){
                        fr->suggestFileName("output.csv");
                    }else if(op_doc.mimetype == "application/obj"){
                        fr->suggestFileName("output.obj");
                    }else if(op_doc.mimetype == "application/mtl"){
                        fr->suggestFileName("output.mtl");
                    }else if(op_doc.mimetype == "image/png"){
                        fr->suggestFileName("output.png");
                    }else if(op_doc.mimetype == "image/fits"){
                        fr->suggestFileName("output.fits");
                    }else{
                        fr->suggestFileName("output");
                    }
                    
                    OutputFiles[param_name] = fr;
                }

            }else if(auto *cb = dynamic_cast<Wt::WCheckBox *>(w)){ //Checkbox for Boolean parameters.
                param_val = (cb->isChecked()) ? "true" : "false";

            }else{
                throw std::logic_error("Table element's child widget type cannot be identified. Please propagate changes.");
            }

            // Only add the parameter if it is expected, or if not expected then non-empty.
            if( (op_doc.expected == true)
            ||  ( (op_doc.expected == false) && !param_val.empty() ) ){
                op_args.insert(param_name, param_val);
            }
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
            AllSuccessful = false;
            //return;
        }
    }
    if(AllSuccessful){
        feedback->setText("<p>Operation successful.</p>");
    }

    gb->show();
    sep_break->setFocus(true);
    gb->setCanReceiveFocus(true);
    gb->setFocus(true);
    gb->setFocus(false);
    this->processEvents();

    // ---

    // Corral the output.
    for(auto &apair : OutputFiles){
        const auto param_name = apair.first;
        const auto HumanParamName = CamelToHuman(param_name);

        auto fr = apair.second;
        const auto fname = fr->fileName();

        //Wt will serve an empty file if the backing file does not exist. This approach is more explicit.
        if(! Does_File_Exist_And_Can_Be_Read(fname) ){
            (void *) gb->addWidget(std::make_unique<Wt::WText>("<p>Output file for '"_s + param_name + "' not available.</p>"));
            break;
        }

        (void *) gb->addWidget(std::make_unique<Wt::WBreak>());

        auto pnamecont = gb->addWidget(std::make_unique<Wt::WContainerWidget>());
        pnamecont->addStyleClass("ParameterNameCont");

        auto outparamname = pnamecont->addWidget(std::make_unique<Wt::WText>(HumanParamName));
        outparamname->addStyleClass("OutputParameterName");

        Wt::WLink fr_link = Wt::WLink(fr);
        fr_link.setTarget(Wt::LinkTarget::Self);
        auto dllink = pnamecont->addWidget(std::make_unique<Wt::WAnchor>(fr_link, "download"_s));
        dllink->addStyleClass("DownloadLink");


        // If the file is tabular, create a table to display the info.
        if(OutputMimetype[param_name] == "text/csv"){
            auto table = gb->addWidget(std::make_unique<Wt::WTable>());
            table->setHeaderCount(1);
            table->setWidth(Wt::WLength("100%"));
            table->disable();
            table->hide();

            //Populate the file.
            auto lines = LoadFileToList(fname);
            size_t row = 0;
            for(auto &l : lines){
                auto tokens = SplitStringToVector(l, ",", 'd');
                size_t col = 0;
                for(auto &t : tokens){
                    (void *) table->elementAt(row,col)->addWidget(std::make_unique<Wt::WText>(" "_s + t + " "_s));
                    //(void *) table->elementAt(row,col)->addWidget(std::make_unique<Wt::WLineEdit>(t));
                    ++col;
                }
                ++row;
            }

            table->enable();
            table->show();

            // If the table is large, hide it behind a toggle button.
            if( (table->rowCount() > 10) || (table->columnCount() > 4) ){
                table->hide();
                auto ttbutton = gb->addWidget(std::make_unique<Wt::WPushButton>("Show table"));
                ttbutton->clicked().connect(std::bind([=](){
                    //ttbutton->disable();
                    if( table->isHidden() ){
                        table->show();
                        ttbutton->setText("Hide table");
                    }else{
                        table->hide();
                        ttbutton->setText("Show table");
                    }
                    this->processEvents();
                    return;
                }));
            }
        }

        // If the file is an image, display it.
        if(OutputMimetype[param_name] == "image/png"){
            auto img = gb->addWidget(std::make_unique<Wt::WImage>(Wt::WLink(fr)));
            img->setAlternateText("Generated image.");
            img->hide();
            img->show();

            // This is a CSS sibling class for <image> that obscures the background when hovering over images.
            auto overlay = gb->addWidget(std::make_unique<Wt::WContainerWidget>());
            overlay->addStyleClass("ImageHoverOverlay");
        }
    }
    this->processEvents();

    // ---

    auto gobutton = gb->addWidget(std::make_unique<Wt::WPushButton>("Perform another operation"));

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
                                         "op_select_gb_grouper",
                                         "op_select_gb_selector",
                                         "op_select_gb_descpanel",
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

    gb->show();
    sep_break->setFocus(true);
    gobutton->setCanReceiveFocus(true);
    gobutton->setFocus(true);
    this->processEvents();

    return;
}

std::unique_ptr<Wt::WApplication> createApplication(const Wt::WEnvironment &env){
    return std::make_unique<BaseWebServerApplication>(env);
}
int main(int argc, char **argv){
    return Wt::WRun(argc, argv, &createApplication);
}

