//Documentation.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <YgorMisc.h>
#include <YgorString.h>
#include <boost/algorithm/string/predicate.hpp>
#include <exception>
#include <functional>
#include <list>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>    
#include <type_traits>
#include <utility>

#include "Operation_Dispatcher.h"
#include "Structs.h"

static
void reflow_and_emit_paragraph(std::ostream &os, 
                               long int width, 
                               std::string prefix_first_line,      // for lists: "  - ".
                               std::string prefix_remaining_lines, // for lists: "    ".
                               std::string first_line,             // break the line early after this text.
                               std::string text){                  // for everything.
    if(prefix_first_line.size() != prefix_remaining_lines.size()){
        throw std::logic_error("Prefixes differ in size. Refusing to continue.");
    }
    std::string pref = prefix_first_line;
    const long int reflow_width = (width - prefix_first_line.size());

    if(!first_line.empty()){
        auto lines = Reflow_Line_to_Fit_Width_Left_Just(first_line, reflow_width - 2); // Leave room for line break.

        long int N_lines = lines.size();
        long int N_line = 0;
        for(auto & aline : lines){
            os << pref << aline;
            if(++N_line == N_lines){
                // Pandoc markdown enforces a line break mid-paragraph when the
                // line ending is preceeded by 2 spaces.
                os << "  " << std::endl;
            }else{
                os << std::endl;
            }
            pref = prefix_remaining_lines;
        }
    }

    for(auto & aline : Reflow_Line_to_Fit_Width_Left_Just(text, reflow_width)){
        os << pref << aline << std::endl;
        pref = prefix_remaining_lines;
    }
    os << std::endl;
    return;
}


void Emit_Documentation(std::ostream &os){
    // This routine contains documentation about DICOMautomaton tools that can be emitted at runtime.
    //
    // Currently, pandoc-style markdown is produced.

    const std::string bulleta("- ");
    const std::string bulletb("  ");
    const std::string nobullet;
    const std::string nolinebreak;
    const long int max_width = 120;

    os << "---" << std::endl;
    os << "title: DICOMautomaton Reference Manual" << std::endl;
    os << "---" << std::endl;
    os << std::endl;

    // -----------------------------------------------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "# Overview"
    );

    //----------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "## About"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "DICOMautomaton is a collection of software tools for processing and analyzing medical images."
        " Once a workflow has been developed, the aim of DICOMautomaton is to require minimal interaction"
        " to perform the workflow in an automated way."
        " However, some interactive tools are also included for workflow development, exploratory analysis,"
        " and contouring."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "DICOMautomaton is meant to be flexible enough to adapt to a wide variety of situations and has"
        " been incorporated into projects to provide: a local PACs, image analysis for various types of QA,"
        " kinetic modeling of perfusion images, automated fuzzy mapping of ROI names to a standard lexicon,"
        " dosimetric analysis, TCP and NTCP modeling, ROI contour/volume manipulation, estimation of surface"
        " dose, ray casting through patient and phantom geometry, rudimentary linac beam optimization,"
        " radiomics, and has been used in various ways to explore the relationship between toxicity and"
        " dose in sub-organ compartments."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "Note: DICOMautomaton should **NOT** be used for clinical purposes."
        " It is experimental software."
        " It is suitable for research or support tool purposes only."
        " It comes with no warranty or guarantee of any kind, either explicit or implied."
        " Users of DICOMautomaton do so fully at their own risk."
    );

    //----------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "## Project Home"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "This project's homepage can be found at <http://www.halclark.ca/>."
        " The source code is available at either"
        " <https://gitlab.com/hdeanclark/DICOMautomaton/> or" 
        " <https://github.com/hdclark/DICOMautomaton/>."
    );

    //----------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "## Download"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "DICOMautomaton relies only on open source software and is itself open source software."
        " Source code is available at <https://github.com/hdclark/DICOMautomaton>."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "Currently, binaries are not provided. Only linux is supported and a recent C++ compiler is needed."
        " A ```PKGBUILD``` file is provided for Arch Linux and derivatives, and CMake can be used to"
        " generate deb files for Debian derivatives. A docker container is available for easy portability"
        " to other systems. DICOMautomaton has successfully run on x86, x86_64, and most ARM systems."
        " To maintain flexibility, DICOMautomaton is generally not ABI or API stable."
    );

    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "## License and Copying"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "All materials herein which may be copywrited, where applicable, are."
        " Copyright 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019"
        " hal clark. See the ```LICENSE``` file for details about the license."
        " Informally, DICOMautomaton is available under a GPLv3+ license."
        " The Imebra library is bundled for convenience and was not written"
        " by hal clark; consult its license file in ```src/imebra/license.txt```."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "All liability is herefore disclaimed. The person(s) who use this source"
        " and/or software do so strictly under their own volition. They assume all"
        " associated liability for use and misuse, including but not limited to"
        " damages, harm, injury, and death which may result, including but not limited"
        " to that arising from unforeseen and unanticipated implementation defects."
    );

    //----------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "## Dependencies"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "Dependencies are listed in the ```PKGBUILD``` file (using Arch Linux package"
        " naming conventions) and in the ```CMakeLists.txt``` file (Debian package naming"
        " conventions) bundled with the source code. See"
        " <https://github.com/hdclark/DICOMautomaton>."
        " Broadly, DICOMautomaton depends on Boost, CGAL, SFML, Eigen, Asio, Wt,"
        " NLopt, and PostgreSQL. Disabling some functionality at compile time can"
        " eliminate some dependencies. This instance has been compiled with the following"
        " functionality."
    );

    os << std::endl
       << "  Dependency       Functionality Enabled?"                       << std::endl
       << "  ---------------  ---------------------------------------"      << std::endl
       << "  Ygor             true (required)"                              << std::endl
       << "  YgorClustering   true (required; header-only)"                 << std::endl
       << "  Explicator       true (required)"                              << std::endl
       << "  Imebra           true (required; bundled)"                     << std::endl
       << "  Boost            true (required)"                              << std::endl
       << "  asio             true (required)"                              << std::endl
       << "  zlib             true (required)"                              << std::endl
       << "  MPFR             true (required)"                              << std::endl
       << "  GNU GMP          true (required)"                              << std::endl
#ifdef DCMA_USE_EIGEN
       << "  Eigen            true"                                         << std::endl
#else       
       << "  Eigen            false"                                        << std::endl
#endif       
#ifdef DCMA_USE_CGAL
       << "  CGAL             true"                                         << std::endl
#else       
       << "  CGAL             false"                                        << std::endl
#endif       
#ifdef DCMA_USE_NLOPT
       << "  NLOpt            true"                                         << std::endl
#else       
       << "  NLOpt            false"                                        << std::endl
#endif       
#ifdef DCMA_USE_SFML
       << "  SFML             true"                                         << std::endl
#else       
       << "  SFML             false"                                        << std::endl
#endif       
#ifdef DCMA_USE_WT
       << "  Wt               true"                                         << std::endl
#else       
       << "  Wt               false"                                        << std::endl
#endif       
#ifdef DCMA_USE_GNU_GSL
       << "  GNU GSL          true"                                         << std::endl
#else       
       << "  GNU GSL          false"                                        << std::endl
#endif       
#ifdef DCMA_USE_POSTGRES
       << "  pqxx             true"                                         << std::endl
#else       
       << "  pqxx             false"                                        << std::endl
#endif       
#ifdef DCMA_USE_JANSSON
       << "  Jansson          true"                                         << std::endl
#else       
       << "  Jansson          false"                                        << std::endl
#endif       
       << std::endl
       << "  Table: Dependencies enabled for this instance." << std::endl
       << std::endl;

    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "Notably, DICOMautomaton depends on the author's 'Ygor,' 'Explicator,'" 
        " and 'YgorClustering' projects. See"
        " <https://gitlab.com/hdeanclark/Ygor>"
        " (mirrored at <https://github.com/hdclark/Ygor>),"
        " <https://gitlab.com/hdeanclark/Explicator>"
        " (mirrored at <https://github.com/hdclark/Explicator>), and"
        " (only for compilation) <https://gitlab.com/hdeanclark/YgorClustering>"
        " (mirrored at <https://github.com/hdclark/YgorClustering>)."
    );
  
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "## Feedback"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "All feedback, questions, comments, and pull requests are welcomed."
    );

    //----------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "## FAQs"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet,
        "**Q.** What is the best way to use DICOMautomaton?"
    ,
        "**A.** DICOMautomaton provides a command-line interface, SFML-based image viewer, and limited"
        " web interface. The command-line interface is most conducive to automation, the viewer works"
        " best for interactive tasks, and the web interface works well for specific"
        " installations."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet,
        "**Q.** How do I contribute, report bugs, or contact the author?"
    ,
        "**A.** All feedback, questions, comments, and pull requests are welcomed."
        " Please find contact information at <https://github.com/hdclark/DICOMautomaton>."
    );

    //----------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "## Citing"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "Several publications and presentations refer to DICOMautomaton or describe some aspect of it."
        " Here are a few:"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb, nolinebreak,
        "H. Clark, J. Beaudry, J. Wu, and S. Thomas."
        " Making use of virtual dimensions for visualization and contouring."
        " Poster presentation at the International Conference on the use of Computers in Radiation"
        " Therapy, London, UK. June 27-30, 2016."
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb, nolinebreak,
        "H. Clark, S. Thomas, V. Moiseenko, R. Lee, B. Gill, C. Duzenli, and J. Wu."
        " Automated segmentation and dose-volume analysis with DICOMautomaton."
        " In the Journal of Physics: Conference Series, vol. 489, no. 1, p. 012009."
        " IOP Publishing, 2014."
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb, nolinebreak,
        "H. Clark, J. Wu, V. Moiseenko, R. Lee, B. Gill, C. Duzenli, and S. Thomas."
        " Semi-automated contour recognition using DICOMautomaton."
        " In the Journal of Physics: Conference Series, vol. 489, no. 1, p. 012088."
        " IOP Publishing, 2014."
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb, nolinebreak,
        "H. Clark, J. Wu, V. Moiseenko, and S. Thomas."
        " Distributed, asynchronous, reactive dosimetric and outcomes analysis using DICOMautomaton."
        " Poster presentation at the COMP Annual Scientific Meeting, Banff, Canada."
        " July 9--12, 2014."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "If you use DICOMautomaton in an academic work, we ask that you please cite the"
        " most relevant publication for that work, if possible."
    );
 
    //----------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "## Components"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "### dicomautomaton_dispatcher"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "#### Description"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "The core command-line interface to DICOMautomaton is the `dicomautomaton_dispatcher` program."
        " It is presents an interface based on chaining of discrete operations on collections of images,"
        " DICOM images, DICOM radiotherapy files (RTSTRUCTS and RTDOSE), and various other types of files."
        " `dicomautomaton_dispatcher` has access to all defined operations described"
        " in [Operations](#operations). It can be used to launch both interactive and non-interactive"
        " tasks. Data can be sourced from a database or files in a variety of formats."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "Name/label selectors in dicomautomaton_dispatcher generally support fuzzy matching via"
        " [libexplicator](https://gitlab.com/hdeanclark/Explicator) or regular expressions."
        " The operations and parameters that provide these"
        " options are documented in [Operations](#operations)."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "Filetype support differs in some cases. A custom FITS file reader and writer are supported,"
        " and DICOM files are generally supported. There is currently no support for RTPLANs, though"
        " DICOM image, RTSTRUCT, and RTDOSE files are well supported. There is limited support for"
        " writing files -- currently JPEG, PNG, and FITS images; RTDOSE files; and Boost.Serialize"
        " archive writing are supported."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "#### Usage Examples"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_dispatcher --help```"
        ,
        "*Print a listing of all available options.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_dispatcher CT*dcm```"
        ,
        "*Launch the default interactive viewer to inspect a collection of computed tomography images.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_dispatcher MR*dcm```"
        ,
        "*Launch the default interactive viewer to inspect a collection of magnetic resonance images.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_dispatcher -o SFML_Viewer MR*dcm```"
        ,
        "*Launch the default interactive viewer to inspect a collection of magnetic resonance images."
        " Note that files specified on the command line are always loaded **prior** to running any operations."
        " Injecting files midway through the operation chain must make use of an operation designed to do so.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_dispatcher CT*dcm RTSTRUCT*dcm RTDOSE*dcm -o Average -o SFML_Viewer```"
        ,
        "*Load DICOM files, perform an [averaging](#average) operation, and then launch"
        " the SFML viewer to inspect the output.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_dispatcher ./RTIMAGE.dcm -o AnalyzePicketFence:ImageSelection='last':InteractivePlots='false'```"
        ,
        "*Perform a [picket fence analysis](#analyzepicketfence) of an RTIMAGE file.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_dispatcher -f create_temp_view.sql -f select_records_from_temp_view.sql -o ComputeSomething```"
        ,
        "*Load a SQL common file that creates a SQL view, issue a query involving the view which"
        " returns some DICOM file(s). Perform analysis 'ComputeSomething' with the files.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_dispatcher -f common.sql -f seriesA.sql -n -f seriesB.sql -o SFML_Viewer```"
        ,
        "*Load two distinct groups of data. The second group does not 'see' the file 'common.sql'"
        " side effects -- the queries are totally separate.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_dispatcher fileA fileB -s fileC adir/ -m PatientID=XYZ003 -o ComputeXYZ -o SFML_Viewer```"
        ,
        "*Load standalone files and all files in specified directory."
        " Inform the analysis 'ComputeXYZ' of the patient's ID, launch the analysis,"
        " and then interactively view.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_dispatcher CT*dcm -o ModifyingOperation -o BoostSerializeDrover```"
        ,
        "*Launch the default interactive viewer to inspect a collection of computed tomography images,"
        " perform an operation that modifies them, and serialize the internal state for later"
        " using the [BoostSerializeDrover](#boostserializedrover) operation.*"
    );

    //----------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "### dicomautomaton_webserver"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "#### Description"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "This web server presents most operations in an interactive web page."
        " Some operations are disabled by default (e.g., [BuildLexiconInteractively](#buildlexiconinteractively)"
        " because they are not designed to be operated via remote procedure calls."
        " This routine should be run within a capability-limiting environment, but access to an X server is required."
        " A Docker script is bundled with DICOMautomaton sources which includes everything needed to function properly."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "#### Usage Examples"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_webserver --help```"
        ,
        "*Print a listing of all available options. Note that most configuration"
        " is done via editing configuration files. See ```/etc/DICOMautomaton/```.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_webserver --config /etc/DICOMautomaton/webserver.conf"
        " --http-address 0.0.0.0 --http-port 8080 --docroot='/etc/DICOMautomaton/'```"
        ,
        "*Launch the webserver on any interface and port 8080.*"
    );

    //----------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "### dicomautomaton_bsarchive_convert"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "#### Description"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "A program for converting Boost.Serialization archives types which DICOMautomaton can read."
        " These archives need to be created by the [BoostSerializeDrover](#boostserializedrover) operation."
        " Some archive types are concise and not portable (i.e., binary archives), or verbose"
        " (and thus slow to read and write) and portable (i.e., XML, plain text)."
        " To combat verbosity, on-the-fly gzip compression and decompression is supported."
        " This program can be used to convert archive types."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "#### Usage Examples"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_bsarchive_convert --help```"
        ,
        "*Print a listing of all available options.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_bsarchive_convert -i file.binary -o file.xml -t 'XML'```"
        ,
        "*Convert a binary archive to a portable XML archive.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_bsarchive_convert -i file.binary.gz -o file.xml.gz -t 'gzip-xml'```"
        ,
        "*Convert a binary archive to a gzipped portable XML archive.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_bsarchive_convert -i file.binary.gz -o file.xml -t 'XML'```"
        ,
        "*Convert a gzipped binary archive to a non-gzipped portable XML archive.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_bsarchive_convert -i file.xml.gz -o file.txt -t 'txt'```"
        ,
        "*Convert a gzipped binary archive to a non-gzipped, portable, and inspectable text archive.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_bsarchive_convert -i file.txt -o file.txt.gz -t 'gzip-txt'```"
        ,
        "*Convert an uncompressed text archive to a compressed text archive."
        " Note that this conversion is effectively the same as simply ```gzip file.txt```.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_bsarchive_convert -i file.xml.gz -o file.bin -t 'binary'```"
        ,
        "*Convert a compressed archive to a binary file.*"
        " Note that binary archives should only expect to be readable on the same hardware"
        " with the same versions and are therefore best for checkpointing calculations that"
        " can fail or may need to be tweaked later.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_bsarchive_convert -i file.xml.gz -o file.bin.gz -t 'gzip-binary'```"
        ,
        "*Convert a compressed archive to a compressed binary file.*"
    );

    //----------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "### dicomautomaton_dump"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "#### Description"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "This program is extremely simplistic. Given a single DICOM file, it prints to stdout the value"
        " of one DICOM tag. This program is best used in scripts, for example to check the modality"
        " or a file."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "#### Usage Examples"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```dicomautomaton_dump afile.dcm 0x0008 0x0060```"
        ,
        "*Print the value of the DICOM tag (0x0008,0x0060) aka (0008,0060).*"
    );

    //----------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "### pacs_ingress"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "#### Description"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "Given a DICOM file and some additional metadata, insert the data into a PACs system database."
        " The file itself will be copied into the database and various bits of data will be deciphered."
        " Note that at the moment a 'gdcmdump' file must be provided and is stored alongside the DICOM file"
        " in the database filestore. This sidecar file is meant to support ad-hoc DICOM queries without"
        " having to index the entire file."
        " Also note that imports into the database are minimal, leaving files with multiple NULL values."
        " This is done to improve ingress times. A separate database refresh"
        " ([pacs_refresh](#pacs_refresh)) must be performed to replace NULL values."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "#### Usage Examples"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```pacs_ingress --help```"
        ,
        "*Print a listing of all available options.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```pacs_ingress -f '/tmp/a.dcm' -g '/tmp/a.gdcmdump' -p 'XYZ Study 2019'"
        " -c 'Study concerning XYZ.'```"
        ,
        "*Insert the file '/tmp/a.dcm' into the database.*"
    );

    //----------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "### pacs_refresh"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "#### Description"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "A program for trying to replace database NULLs, if possible, using stored files."
        " This program is complementary to [pacs_ingress](#pacs_ingress)."
        " Note that the ```--days-back/-d``` parameter should always be specified."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "#### Usage Examples"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```pacs_refresh --help```"
        ,
        "*Print a listing of all available options.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```pacs_refresh -d 7```"
        ,
        "*Perform a refresh of the database, restricting to files imported"
        " within the previous 7 days.*"
    );

    //----------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "### pacs_duplicate_cleaner"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "#### Description"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "Given a DICOM file, check if it is in the PACS DB. If so, delete the file."
        " Note that a full, byte-by-byte comparison is NOT performed -- rather only the"
        " top-level DICOM unique identifiers are (currently) compared."
        " No other metadata is considered. So this program is not suitable if DICOM files"
        " have been modified without re-assigning unique identifiers!"
        " (Which is non-standard behaviour.)"
        " Note that if an *exact* comparison is desired, using a traditional file de-duplicator will work."
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "#### Usage Examples"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```pacs_duplicate_cleaner --help```"
        ,
        "*Print a listing of all available options.*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```pacs_duplicate_cleaner -f '/path/to/a/dicom/file.dcm'```"
        ,
        "*Check if 'file.dcm' is already in the PACS DB. If so, delete it ('file.dcm').*"
    );
    reflow_and_emit_paragraph(os, max_width, bulleta, bulletb,
        "```pacs_duplicate_cleaner -f '/path/to/a/dicom/file.dcm' -n```"
        ,
        "*Check if 'file.dcm' is already in the PACS DB, but do not delete anything.*"
    );

    // -----------------------------------------------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "# List of Available Operations"
    );

    // Print an index of links to each operation.
    auto known_ops = Known_Operations();
    {
        for(auto &anop : known_ops){
            const auto name = anop.first;
            //os << bulleta << "[" << name << "](#" << name << ")" << std::endl;
            os << bulleta << name << std::endl;
        }
        os << std::endl;
    }

    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "# Operations Reference"
    );
    for(auto &anop : known_ops){
        const auto name = anop.first;

        reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
            "## "_s + name
        );
        auto optdocs = anop.second.first();
        reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
            "### Description"
        );
        reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
            optdocs.desc
        );

        if(!optdocs.notes.empty()){
            reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
                "### Notes"
            );
            for(auto &n : optdocs.notes){
                reflow_and_emit_paragraph(os, max_width, bulleta, bulletb, nolinebreak,
                    n
                );
            }
        }

        reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
            "### Parameters"
        );
        if(optdocs.args.empty()){
            reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
                "No registered options."
            );
        }else{
            for(auto &a : optdocs.args){
                os << bulleta << a.name << std::endl;
            }
            os << std::endl;
            for(auto &a : optdocs.args){
                reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
                    "#### "_s + a.name
                );

                reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
                    "##### Description"
                );
                reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
                    a.desc
                );

                reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
                    "##### Default"
                );
                os << bulleta << "```\"" << a.default_val << "\"```" << std::endl;
                os << std::endl;

                if(!a.examples.empty()){
                    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
                        "##### Examples"
                    );
                    for(auto &e : a.examples){
                        os << bulleta << "```\"" << e << "\"```" << std::endl;
                    }
                    os << std::endl;
                }
            }
            os << std::endl;
        }
    }

    // -----------------------------------------------------
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "# Known Issues and Limitations"
    );

    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "## Hanging on Debian"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "The SFML_Viewer operation hangs on some systems after viewing a plot with"
        " Gnuplot. This stems from a known issue in Ygor."
    );

    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "## Build Requirements"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "DICOMautomaton depends on several heavily templated libraries and"
        " external projects. It requires a considerable amount of memory to build."
    );

    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "## DICOM-RT Support Incomplete"
    );
    reflow_and_emit_paragraph(os, max_width, nobullet, nobullet, nolinebreak,
        "Support for the DICOM Radiotherapy extensions are limited."
        " In particular, only RTDOSE files can currently be exported,"
        " and RTPLAN files are not supported at all. Read support for"
        " DICOM image modalities and RTSTRUCTS are generally supported well."
        " Broader DICOM support is planned for a future release."
    );

    return;
}

