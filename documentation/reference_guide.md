---
title: DICOMautomaton Reference Manual
---

# Overview

## About

DICOMautomaton is a collection of software tools for processing and analyzing
medical images. Once a workflow has been developed, the aim of DICOMautomaton is
to require minimal interaction to perform the workflow in an automated way.
However, some interactive tools are also included for workflow development,
exploratory analysis, and contouring.

DICOMautomaton is meant to be flexible enough to adapt to a wide variety of
situations and has been incorporated into projects to provide: a local PACs,
image analysis for various types of QA, kinetic modeling of perfusion images,
automated fuzzy mapping of ROI names to a standard lexicon, dosimetric analysis,
TCP and NTCP modeling, ROI contour/volume manipulation, estimation of surface
dose, ray casting through patient and phantom geometry, rudimentary linac beam
optimization, radiomics, and has been used in various ways to explore the
relationship between toxicity and dose in sub-organ compartments.

Note: DICOMautomaton should **NOT** be used for clinical purposes. It is
experimental software. It is suitable for research or support tool purposes
only. It comes with no warranty or guarantee of any kind, either explicit or
implied. Users of DICOMautomaton do so fully at their own risk.

## Project Home

This project's homepage can be found at <http://www.halclark.ca/>. The source
code is available at either <https://gitlab.com/hdeanclark/DICOMautomaton/> or
<https://github.com/hdclark/DICOMautomaton/>.

## Download

DICOMautomaton relies only on open source software and is itself open source
software. Source code is available at
<https://github.com/hdclark/DICOMautomaton>.

Currently, binaries are not provided. Only linux is supported and a recent C++
compiler is needed. A ```PKGBUILD``` file is provided for Arch Linux and
derivatives, and CMake can be used to generate deb files for Debian derivatives.
A docker container is available for easy portability to other systems.
DICOMautomaton has successfully run on x86, x86_64, and most ARM systems. To
maintain flexibility, DICOMautomaton is generally not ABI or API stable.

## License and Copying

All materials herein which may be copywrited, where applicable, are. Copyright
2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019 hal clark. See the
```LICENSE``` file for details about the license. Informally, DICOMautomaton is
available under a GPLv3+ license. The Imebra library is bundled for convenience
and was not written by hal clark; consult its license file in
```src/imebra/license.txt```.

All liability is herefore disclaimed. The person(s) who use this source and/or
software do so strictly under their own volition. They assume all associated
liability for use and misuse, including but not limited to damages, harm,
injury, and death which may result, including but not limited to that arising
from unforeseen and unanticipated implementation defects.

## Dependencies

Dependencies are listed in the ```PKGBUILD``` file (using Arch Linux package
naming conventions) and in the ```CMakeLists.txt``` file (Debian package naming
conventions) bundled with the source code. See
<https://github.com/hdclark/DICOMautomaton>. Broadly, DICOMautomaton depends on
Boost, CGAL, SFML, Eigen, Asio, Wt, and PostgreSQL.

Notably, DICOMautomaton depends on the author's 'Ygor,' 'Explicator,' and
'YgorClustering' projects. See <https://gitlab.com/hdeanclark/Ygor> (mirrored at
<https://github.com/hdclark/Ygor>), <https://gitlab.com/hdeanclark/Explicator>
(mirrored at <https://github.com/hdclark/Explicator>), and (only for
compilation) <https://gitlab.com/hdeanclark/YgorClustering> (mirrored at
<https://github.com/hdclark/YgorClustering>).

## Feedback

All feedback, questions, comments, and pull requests are welcomed.

## FAQs

**Q.** What is the best way to use DICOMautomaton?  
**A.** DICOMautomaton provides a command-line interface, SFML-based image
viewer, and limited web interface. The command-line interface is most conducive
to automation, the viewer works best for interactive tasks, and the web
interface works well for specific installations.

**Q.** How do I contribute, report bugs, or contact the author?  
**A.** All feedback, questions, comments, and pull requests are welcomed. Please
find contact information at <https://github.com/hdclark/DICOMautomaton>.

## Citing

Several publications and presentations refer to DICOMautomaton or describe some
aspect of it. Here are a few:

- H. Clark, J. Beaudry, J. Wu, and S. Thomas. Making use of virtual dimensions
  for visualization and contouring. Poster presentation at the International
  Conference on the use of Computers in Radiation Therapy, London, UK. June
  27-30, 2016.

- H. Clark, S. Thomas, V. Moiseenko, R. Lee, B. Gill, C. Duzenli, and J. Wu.
  Automated segmentation and dose-volume analysis with DICOMautomaton. In the
  Journal of Physics: Conference Series, vol. 489, no. 1, p. 012009. IOP
  Publishing, 2014.

- H. Clark, J. Wu, V. Moiseenko, R. Lee, B. Gill, C. Duzenli, and S. Thomas.
  Semi-automated contour recognition using DICOMautomaton. In the Journal of
  Physics: Conference Series, vol. 489, no. 1, p. 012088. IOP Publishing, 2014.

- H. Clark, J. Wu, V. Moiseenko, and S. Thomas. Distributed, asynchronous,
  reactive dosimetric and outcomes analysis using DICOMautomaton. Poster
  presentation at the COMP Annual Scientific Meeting, Banff, Canada. July 9--12,
  2014.

If you use DICOMautomaton in an academic work, we ask that you please cite the
most relevant publication for that work, if possible.

## Components

### dicomautomaton_dispatcher

#### Description

The core command-line interface to DICOMautomaton is the
`dicomautomaton_dispatcher` program. It is presents an interface based on
chaining of discrete operations on collections of images, DICOM images, DICOM
radiotherapy files (RTSTRUCTS and RTDOSE), and various other types of files.
`dicomautomaton_dispatcher` has access to all defined operations described in
[Operations](#operations). It can be used to launch both interactive and
non-interactive tasks. Data can be sourced from a database or files in a variety
of formats.

Name/label selectors in dicomautomaton_dispatcher generally support fuzzy
matching via [libexplicator](https://gitlab.com/hdeanclark/Explicator) or
regular expressions. The operations and parameters that provide these options
are documented in [Operations](#operations).

Filetype support differs in some cases. A custom FITS file reader and writer are
supported, and DICOM files are generally supported. There is currently no
support for RTPLANs, though DICOM image, RTSTRUCT, and RTDOSE files are well
supported. There is limited support for writing files -- currently JPEG, PNG,
and FITS images; RTDOSE files; and Boost.Serialize archive writing are
supported.

#### Usage Examples

- ```dicomautomaton_dispatcher --help```  
  *Print a listing of all available options.*

- ```dicomautomaton_dispatcher CT*dcm```  
  *Launch the default interactive viewer to inspect a collection of computed
  tomography images.*

- ```dicomautomaton_dispatcher MR*dcm```  
  *Launch the default interactive viewer to inspect a collection of magnetic
  resonance images.*

- ```dicomautomaton_dispatcher -o SFML_Viewer MR*dcm```  
  *Launch the default interactive viewer to inspect a collection of magnetic
  resonance images. Note that files specified on the command line are always
  loaded **prior** to running any operations. Injecting files midway through the
  operation chain must make use of an operation designed to do so.*

- ```dicomautomaton_dispatcher CT*dcm RTSTRUCT*dcm RTDOSE*dcm -o Average -o
  SFML_Viewer```  
  *Load DICOM files, perform an [averaging](#average) operation, and then launch
  the SFML viewer to inspect the output.*

- ```dicomautomaton_dispatcher ./RTIMAGE.dcm -o
  AnalyzePicketFence:ImageSelection='last':InteractivePlots='false'```  
  *Perform a [picket fence analysis](#analyzepicketfence) of an RTIMAGE file.*

- ```dicomautomaton_dispatcher -f create_temp_view.sql -f
  select_records_from_temp_view.sql -o ComputeSomething```  
  *Load a SQL common file that creates a SQL view, issue a query involving the
  view which returns some DICOM file(s). Perform analysis 'ComputeSomething'
  with the files.*

- ```dicomautomaton_dispatcher -f common.sql -f seriesA.sql -n -f seriesB.sql
  -o SFML_Viewer```  
  *Load two distinct groups of data. The second group does not 'see' the file
  'common.sql' side effects -- the queries are totally separate.*

- ```dicomautomaton_dispatcher fileA fileB -s fileC adir/ -m PatientID=XYZ003
  -o ComputeXYZ -o SFML_Viewer```  
  *Load standalone files and all files in specified directory. Inform the
  analysis 'ComputeXYZ' of the patient's ID, launch the analysis, and then
  interactively view.*

- ```dicomautomaton_dispatcher CT*dcm -o ModifyingOperation -o
  BoostSerializeDrover```  
  *Launch the default interactive viewer to inspect a collection of computed
  tomography images, perform an operation that modifies them, and serialize the
  internal state for later using the
  [BoostSerializeDrover](#boostserializedrover) operation.*

### dicomautomaton_webserver

#### Description

This web server presents most operations in an interactive web page. Some
operations are disabled by default (e.g.,
[BuildLexiconInteractively](#buildlexiconinteractively) because they are not
designed to be operated via remote procedure calls. This routine should be run
within a capability-limiting environment, but access to an X server is required.
A Docker script is bundled with DICOMautomaton sources which includes everything
needed to function properly.

#### Usage Examples

- ```dicomautomaton_webserver --help```  
  *Print a listing of all available options. Note that most configuration is
  done via editing configuration files. See ```/etc/DICOMautomaton/```.*

- ```dicomautomaton_webserver --config /etc/DICOMautomaton/webserver.conf
  --http-address 0.0.0.0 --http-port 8080 --docroot='/etc/DICOMautomaton/'```  
  *Launch the webserver on any interface and port 8080.*

### dicomautomaton_bsarchive_convert

#### Description

A program for converting Boost.Serialization archives types which DICOMautomaton
can read. These archives need to be created by the
[BoostSerializeDrover](#boostserializedrover) operation. Some archive types are
concise and not portable (i.e., binary archives), or verbose (and thus slow to
read and write) and portable (i.e., XML, plain text). To combat verbosity,
on-the-fly gzip compression and decompression is supported. This program can be
used to convert archive types.

#### Usage Examples

- ```dicomautomaton_bsarchive_convert --help```  
  *Print a listing of all available options.*

- ```dicomautomaton_bsarchive_convert -i file.binary -o file.xml -t 'XML'```  
  *Convert a binary archive to a portable XML archive.*

- ```dicomautomaton_bsarchive_convert -i file.binary.gz -o file.xml.gz -t
  'gzip-xml'```  
  *Convert a binary archive to a gzipped portable XML archive.*

- ```dicomautomaton_bsarchive_convert -i file.binary.gz -o file.xml -t
  'XML'```  
  *Convert a gzipped binary archive to a non-gzipped portable XML archive.*

- ```dicomautomaton_bsarchive_convert -i file.xml.gz -o file.txt -t 'txt'```  
  *Convert a gzipped binary archive to a non-gzipped, portable, and inspectable
  text archive.*

- ```dicomautomaton_bsarchive_convert -i file.txt -o file.txt.gz -t
  'gzip-txt'```  
  *Convert an uncompressed text archive to a compressed text archive. Note that
  this conversion is effectively the same as simply ```gzip file.txt```.*

- ```dicomautomaton_bsarchive_convert -i file.xml.gz -o file.bin -t
  'binary'```  
  *Convert a compressed archive to a binary file.* Note that binary archives
  should only expect to be readable on the same hardware with the same versions
  and are therefore best for checkpointing calculations that can fail or may
  need to be tweaked later.*

- ```dicomautomaton_bsarchive_convert -i file.xml.gz -o file.bin.gz -t
  'gzip-binary'```  
  *Convert a compressed archive to a compressed binary file..*

### dicomautomaton_dump

#### Description

This program is extremely simplistic. Given a single DICOM file, it prints to
stdout the value of one DICOM tag. This program is best used in scripts, for
example to check the modality or a file.

#### Usage Examples

- ```dicomautomaton_dump afile.dcm 0x0008 0x0060```  
  *Print the value of the DICOM tag (0x0008,0x0060) aka (0008,0060).*

### pacs_ingress

#### Description

Given a DICOM file and some additional metadata, insert the data into a PACs
system database. The file itself will be copied into the database and various
bits of data will be deciphered. Note that at the moment a 'gdcmdump' file must
be provided and is stored alongside the DICOM file in the database filestore.
This sidecar file is meant to support ad-hoc DICOM queries without having to
index the entire file. Also note that imports into the database are minimal,
leaving files with multiple NULL values. This is done to improve ingress times.
A separate database refresh ([pacs_refresh](#pacs_refresh)) must be performed to
replace NULL values.

#### Usage Examples

- ```pacs_ingress --help```  
  *Print a listing of all available options.*

- ```pacs_ingress -f '/tmp/a.dcm' -g '/tmp/a.gdcmdump' -p 'XYZ Study 2019' -c
  'Study concerning XYZ.'```  
  *Insert the file '/tmp/a.dcm' into the database.*

### pacs_refresh

#### Description

A program for trying to replace database NULLs, if possible, using stored files.
This program is complementary to [pacs_ingress](#pacs_ingress). Note that the
```--days-back/-d``` parameter should always be specified.

#### Usage Examples

- ```pacs_refresh --help```  
  *Print a listing of all available options.*

- ```pacs_refresh -d 7```  
  *Perform a refresh of the database, restricting to files imported within the
  previous 7 days.*

### pacs_duplicate_cleaner

#### Description

Given a DICOM file, check if it is in the PACS DB. If so, delete the file. Note
that a full, byte-by-byte comparison is NOT performed -- rather only the
top-level DICOM unique identifiers are (currently) compared. No other metadata
is considered. So this program is not suitable if DICOM files have been modified
without re-assigning unique identifiers! (Which is non-standard behaviour.) Note
that if an *exact* comparison is desired, using a traditional file de-duplicator
will work.

#### Usage Examples

- ```pacs_duplicate_cleaner --help```  
  *Print a listing of all available options.*

- ```pacs_duplicate_cleaner -f '/path/to/a/dicom/file.dcm'```  
  *Check if 'file.dcm' is already in the PACS DB. If so, delete it
  ('file.dcm').*

- ```pacs_duplicate_cleaner -f '/path/to/a/dicom/file.dcm' -n```  
  *Check if 'file.dcm' is already in the PACS DB, but do not delete anything.*

### automaton

#### Description

An earlier prototype of [dicomautomaton_dispatcher](#dicomautomaton_dispatcher)
that will likely be deprecated soon. Use is discouraged. The means of switching
operations was via source editing and recompilation.

This program allows for performing rapid, no-nonsense, no-GUI computations using
DICOM files. As much as possible, emphasis is placed on having the program 'do
the right thing', which refers to the attempts to deal with incomplete
information (such as missing files, non-matching DICOM data sets, and the
careful treatment of existing data.)

This program is designed to accept an input structure name(s) (pre-sanitized or
not) and some DICOM data, and then produce output for the structure(s). An
example might be computation of a DVH for the left parotid.

In some ways this program is very forgiving of user behaviour, but in general it
has very strictly-defined behaviour. For example, input files can be either
directories or files, and non-DICOM files will be automatically weeded-out.
However, it is intentionally difficult to accidentally overwrite existing data:
if an output file already exists, will usually refuse to overwrite it. To be
user-friendly, though, a non-existing filename will be chosen and the the user
will be warned. This might occasionally be frustrating for the user, but is the
'safe' thing to do in most cases.

#### Usage Examples

- ```automaton --help```  
  *Print a listing of all available options.*

### overlaydosedata

#### Description

Like [automaton](#automaton), this program is an earlier prototype of
[dicomautomaton_dispatcher](#dicomautomaton_dispatcher) that will likely be
deprecated soon. Use is discouraged. The aim of overlaydosedata was to provide a
semi-interactive viewer capable of simultaneous display of medical images, ROI
structures, and dose information.

#### Usage Examples

- ```overlaydosedata --help```  
  *Print a listing of all available options.*

# Operations

## AccumulateRowsColumns

### Description

This operation generates row- and column-profiles of images in which the entire
row or column has been summed together. It is useful primarily for detection of
axes-aligned edges or ridges.

### Notes

- It is often useful to pre-process inputs by computing an in-image-plane
  derivative, gradient magnitude, or similar (i.e., something to emphasize
  edges) before calling this routine. It is not necessary, however.

### Parameters

- ImageSelection

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```


## AnalyzeLightRadFieldCoincidence

### Description

This operation analyzes the selected images to compare light and radiation field
coincidence for fixed, symmetric field sizes. Coincidences are extracted
automatically by fitting Gaussians to the peak nearest to one of the specified
field boundaries and comparing offset from one another. So, for example, a
10x10cm MLC-defined field would be compared to a 15x15cm field if there are
sharp edges (say, metal rulers) that define a 10x10cm field (i.e., considered to
represent the light field). Horizontal and vertical directions (both positive
and negative) are all analyzed separately.

### Notes

- This routine assumes both fields are squarely aligned with the image axes.
  Alignment need not be perfect, but the Gaussians may be significantly
  broadened if there is misalignment. This should be fixed in a future revision.

- It is often useful to pre-process inputs by computing an in-image-plane
  derivative, gradient magnitude, or similar (i.e., something to emphasize
  edges) before calling this routine. It may not be necessary, however.

### Parameters

- ImageSelection
- ToleranceLevel
- EdgeLengths
- SearchDistance
- PeakSimilarityThreshold
- UserComment
- OutputFileName
- InteractivePlots

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### ToleranceLevel

##### Description

Controls detected edge visualization for easy identification of edges out of
tolerance. Note: this value refers to edge-to-edge separation, not
edge-to-nominal distances. This value is in DICOM units.

##### Default

- ```"1.0"```

##### Examples

- ```"0.5"```
- ```"1.0"```
- ```"2.0"```
- ```"inf"```

#### EdgeLengths

##### Description

Comma-separated list of (symmetric) edge lengths fields should be analyzed at.
For example, if 50x50, 100x100, 150x150, and 200x200 (all in mm) fields are to
be analyzed, this argument would be '50,100,150,200' and it will be assumed that
the field centre is at DICOM position (0,0,0). All values are in DICOM units.

##### Default

- ```"100"```

##### Examples

- ```"100.0"```
- ```"50,100,150,200,300"```
- ```"10.273,20.2456"```

#### SearchDistance

##### Description

The distance around the anticipated field edges to search for edges (actually
sharp peaks arising from edges). If an edge is further away than this value from
the anticipated field edge, then the coincidence will be ignored altogether. The
value should be greater than the largest action/tolerance threshold with some
additional margin (so gross errors can be observed), but small enough that
spurious edges (i.e., unintended features in the image, such as metal fasteners,
or artifacts near the field edge) do not replace the true field edges. The
'sharpness' of the field edge (resulting from the density of the material used
to demarcate the edge) can impact this value; if the edge is not sharp, then the
peak will be shallow, noisy, and may therefore travel around depending on how
the image is pre-processed. Note that both radiation field and light field edges
may differ from the 'nominal' anticipated edges, so this wobble factor should be
incorporated in the search distance. This quantity must be in DICOM units.

##### Default

- ```"3.0"```

##### Examples

- ```"2.5"```
- ```"3.0"```
- ```"5.0"```

#### PeakSimilarityThreshold

##### Description

Images can be taken such that duplicate peaks will occur, such as when field
sizes are re-used. Peaks are therefore de-duplicated. This value (as a %,
ranging from [0,100]) specifies the threshold of disimilarity below which peaks
are considered duplicates. A low value will make duplicates confuse the
analysis, but a high value may cause legitimate peaks to be discarded depending
on the attenuation cababilties of the field edge markers.

##### Default

- ```"25"```

##### Examples

- ```"5"```
- ```"10"```
- ```"15"```
- ```"50"```

#### UserComment

##### Description

A string that will be inserted into the output file which will simplify merging
output with differing parameters, from different sources, or using
sub-selections of the data. If left empty, the column will be omitted from the
output.

##### Default

- ```""```

##### Examples

- ```""```
- ```"6MV"```
- ```"Using XYZ"```
- ```"Test with thick metal edges"```

#### OutputFileName

##### Description

A filename (or full path) in which to append field edge coincidence data
generated by this routine. The format is CSV. Leave empty to dump to generate a
unique temporary file.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"derivative_data.csv"```

#### InteractivePlots

##### Description

Whether to interactively show plots showing detected edges.

##### Default

- ```"false"```

##### Examples

- ```"true"```
- ```"false"```


## AnalyzePicketFence

### Description

This operation extracts MLC positions from a picket fence image.

### Notes

- This routine requires data to be pre-processed. The gross picket area should
  be isolated and the leaf junction areas contoured (one contour per junction).
  Both can be accomplished via thresholding.

- This routine analyzes the picket fences on the plane in which they are
  specified within the DICOM file, which often coincides with the image receptor
  ('RTImageSID'). Tolerances are evaluated on the isoplane, so the image is
  projected before measuring distances, but the image itself is not altered; a
  uniform magnification factor of SAD/SID is applied to all distances.

### Parameters

- ImageSelection
- MLCModel
- MLCROILabel
- JunctionROILabel
- PeakROILabel
- MinimumJunctionSeparation
- ThresholdDistance
- LeafGapsFileName
- ResultsSummaryFileName
- UserComment
- InteractivePlots

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### MLCModel

##### Description

The MLC design geometry to use. 'VarianMillenniumMLC80' has 80 leafs in each
bank; leaves are 10mm wide at isocentre; and the maximum static field size is
40cm x 40cm. 'VarianMillenniumMLC120' has 120 leafs in each bank; the 40 central
leaves are 5mm wide at isocentre; the 20 peripheral leaves are 10mm wide; and
the maximum static field size is 40cm x 40cm. 'VarianHD120' has 120 leafs in
each bank; the 32 central leaves are 2.5mm wide at isocentre; the 28 peripheral
leaves are 5mm wide; and the maximum static field size is 40cm x 22cm.

##### Default

- ```"VarianMillenniumMLC120"```

##### Examples

- ```"VarianMillenniumMLC80"```
- ```"VarianMillenniumMLC120"```
- ```"VarianHD120"```

#### MLCROILabel

##### Description

An ROI imitating the MLC axes of leaf pairs is created. This is the label to
apply to it. Note that the leaves are modeled with thin contour rectangles of
virtually zero area. Also note that the outline colour is significant and
denotes leaf pair pass/fail.

##### Default

- ```"Leaves"```

##### Examples

- ```"MLC_leaves"```
- ```"MLC"```
- ```"approx_leaf_axes"```

#### JunctionROILabel

##### Description

An ROI imitating the junction is created. This is the label to apply to it. Note
that the junctions are modeled with thin contour rectangles of virtually zero
area.

##### Default

- ```"Junction"```

##### Examples

- ```"Junction"```
- ```"Picket_Fence_Junction"```

#### PeakROILabel

##### Description

ROIs encircling the leaf profile peaks are created. This is the label to apply
to it. Note that the peaks are modeled with small squares.

##### Default

- ```"Peak"```

##### Examples

- ```"Peak"```
- ```"Picket_Fence_Peak"```

#### MinimumJunctionSeparation

##### Description

The minimum distance between junctions on the SAD isoplane in DICOM units (mm).
This number is used to de-duplicate automatically detected junctions. Analysis
results should not be sensitive to the specific value.

##### Default

- ```"10.0"```

##### Examples

- ```"5.0"```
- ```"10.0"```
- ```"15.0"```
- ```"25.0"```

#### ThresholdDistance

##### Description

The threshold distance in DICOM units (mm) above which MLC separations are
considered to 'fail'. Each leaf pair is evaluated separately. Pass/fail status
is also indicated by setting the leaf axis contour colour (blue for pass, red
for fail).

##### Default

- ```"1.0"```

##### Examples

- ```"0.5"```
- ```"1.0"```
- ```"2.0"```

#### LeafGapsFileName

##### Description

This file will contain gap and nominal-vs-actual offset distances for each leaf
pair. The format is CSV. Leave empty to dump to generate a unique temporary
file. If an existing file is present, rows will be appended without writing a
header.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"derivative_data.csv"```

#### ResultsSummaryFileName

##### Description

This file will contain a brief summary of the results. The format is CSV. Leave
empty to dump to generate a unique temporary file. If an existing file is
present, rows will be appended without writing a header.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"derivative_data.csv"```

#### UserComment

##### Description

A string that will be inserted into the output file which will simplify merging
output with differing parameters, from different sources, or using
sub-selections of the data.

##### Default

- ```""```

##### Examples

- ```""```
- ```"Using XYZ"```
- ```"Patient treatment plan C"```

#### InteractivePlots

##### Description

Whether to interactively show plots showing detected edges.

##### Default

- ```"false"```

##### Examples

- ```"true"```
- ```"false"```


## ApplyCalibrationCurve

### Description

This operation applies a given calibration curve to voxel data inside the
specified ROI(s). It is designed to apply calibration curves, but is useful for
transforming voxel intensities using any supplied 1D curve.

### Notes

- This routine can handle overlapping or duplicate contours.

### Parameters

- Channel
- ImageSelection
- ContourOverlap
- Inclusivity
- CalibCurveFileName
- NormalizedROILabelRegex
- ROILabelRegex

#### Channel

##### Description

The image channel to use. Zero-based. Use '-1' to operate on all available
channels.

##### Default

- ```"-1"```

##### Examples

- ```"-1"```
- ```"0"```
- ```"1"```
- ```"2"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### ContourOverlap

##### Description

Controls overlapping contours are treated. The default 'ignore' treats
overlapping contours as a single contour, regardless of contour orientation. The
option 'honour_opposite_orientations' makes overlapping contours with opposite
orientation cancel. Otherwise, orientation is ignored. The latter is useful for
Boolean structures where contour orientation is significant for interior
contours (holes). The option 'overlapping_contours_cancel' ignores orientation
and cancels all contour overlap.

##### Default

- ```"ignore"```

##### Examples

- ```"ignore"```
- ```"honour_opposite_orientations"```
- ```"overlapping_contours_cancel"```
- ```"honour_opps"```
- ```"overlap_cancel"```

#### Inclusivity

##### Description

Controls how voxels are deemed to be 'within' the interior of the selected
ROI(s). The default 'center' considers only the central-most point of each
voxel. There are two corner options that correspond to a 2D projection of the
voxel onto the image plane. The first, 'planar_corner_inclusive', considers a
voxel interior if ANY corner is interior. The second, 'planar_corner_exclusive',
considers a voxel interior if ALL (four) corners are interior.

##### Default

- ```"center"```

##### Examples

- ```"center"```
- ```"centre"```
- ```"planar_corner_inclusive"```
- ```"planar_inc"```
- ```"planar_corner_exclusive"```
- ```"planar_exc"```

#### CalibCurveFileName

##### Description

The file from which a calibration curve should be read from. The format should
be line-based with either 2 or 4 numbers per line. For 2 numbers: (current pixel
value) (new pixel value) and for 4 numbers: (current pixel value) (uncertainty)
(new pixel value) (uncertainty). Uncertainties refer to the prior number and may
be uniformly zero if unknown. Lines beginning with '#' are treated as comments
and ignored. The curve is linearly interpolated, and must span the full range of
pixel values. This is done to avoid extrapolation within the operation since the
correct behaviour will differ depending on the specifics of the calibration.

##### Default

- ```""```

##### Examples

- ```"/tmp/calib.dat"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```


## AutoCropImages

### Description

This operation crops image slices using image-specific metadata embedded within
the image.

### Parameters

- ImageSelection
- DICOMMargin
- RTIMAGE

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"all"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### DICOMMargin

##### Description

The amount of margin (in the DICOM coordinate system) to spare from cropping.

##### Default

- ```"0.0"```

##### Examples

- ```"0.1"```
- ```"2.0"```
- ```"-0.5"```
- ```"20.0"```

#### RTIMAGE

##### Description

If true, attempt to crop the image using information embedded in an RTIMAGE.
This option cannot be used with the other options.

##### Default

- ```"true"```

##### Examples

- ```"true"```
- ```"false"```


## Average

### Description

This operation averages image or dose volumes. It can average over spatial or
temporal dimensions. However, rather than relying specifically on time for
temporal averaging, any images that have overlapping voxels can be averaged.

### Notes

- This operation is typically used to create an aggregate view of a large volume
  of data. It may also increase SNR and can be used for contouring purposes.

### Parameters

- DoseImageSelection
- ImageSelection
- AveragingMethod

#### DoseImageSelection

##### Description

Dose images to operate on. Either 'none', 'last', or 'all'.

##### Default

- ```"none"```

##### Examples

- ```"none"```
- ```"last"```
- ```"all"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### AveragingMethod

##### Description

The averaging method to use. Valid methods are 'overlapping-spatially' and
'overlapping-temporally'.

##### Default

- ```""```

##### Examples

- ```"overlapping-spatially"```
- ```"overlapping-temporally"```


## BCCAExtractRadiomicFeatures

### Description

This operation extracts radiomic features from an image and one or more ROIs.

### Notes

- This is a 'simplified' version of the full radiomics extract routine that uses
  defaults that are expected to be reasonable across a wide range of scenarios.

### Parameters

- NormalizedROILabelRegex
- ROILabelRegex
- FractionalAreaTolerance
- SimplificationMethod
- UserComment
- FeaturesFileName
- ImageSelection
- NormalizedROILabelRegex
- ROILabelRegex
- ScaleFactor
- ImageFileName

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### FractionalAreaTolerance

##### Description

The fraction of area each contour will tolerate during simplified. This is a
measure of how much the contour area can change due to simplification.

##### Default

- ```"0.05"```

##### Examples

- ```"0.001"```
- ```"0.01"```
- ```"0.02"```
- ```"0.05"```
- ```"0.10"```

#### SimplificationMethod

##### Description

The specific algorithm used to perform contour simplification. 'Vertex removal'
is a simple algorithm that removes vertices one-by-one without replacement. It
iteratively ranks vertices and removes the single vertex that has the least
impact on contour area. It is best suited to removing redundant vertices or
whenever new vertices should not be added. 'Vertex collapse' combines two
adjacent vertices into a single vertex at their midpoint. It iteratively ranks
vertex pairs and removes the single vertex that has the least total impact on
contour area. Note that small sharp features that alternate inward and outward
will have a small total area cost, so will be pruned early. Thus this technique
acts as a low-pass filter and will defer simplification of high-curvature
regions until necessary. It is more economical compared to vertex removal in
that it will usually simplify contours more for a given tolerance (or,
equivalently, can retain contour fidelity better than vertex removal for the
same number of vertices). However, vertex collapse performs an averaging that
may result in numerical imprecision.

##### Default

- ```"vert-rem"```

##### Examples

- ```"vertex-collapse"```
- ```"vertex-removal"```

#### UserComment

##### Description

A string that will be inserted into the output file which will simplify merging
output with differing parameters, from different sources, or using
sub-selections of the data.

##### Default

- ```""```

##### Examples

- ```""```
- ```"Using XYZ"```
- ```"Patient treatment plan C"```

#### FeaturesFileName

##### Description

Features will be appended to this file. The format is CSV. Leave empty to dump
to generate a unique temporary file. If an existing file is present, rows will
be appended without writing a header.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"derivative_data.csv"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### ScaleFactor

##### Description

This factor is applied to the image width and height to magnify (larger than 1)
or shrink (less than 1) the image. This factor only affects the output image
size. Note that aspect ratio is retained, but rounding for non-integer factors
may lead to small (1-2 pixel) discrepancies.

##### Default

- ```"1.5"```

##### Examples

- ```"0.5"```
- ```"1.0"```
- ```"2.0"```
- ```"5.23"```

#### ImageFileName

##### Description

The file name to use for the image. If blank, a filename will be generated
sequentially.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/an_image.png"```
- ```"afile.png"```


## BoostSerializeDrover

### Description

This operation exports all loaded state to a serialized format that can be
loaded again later. Is is especially useful for suspending long-running
operations with intermittant interactive sub-operations.

### Parameters

- Filename

#### Filename

##### Description

The filename (or full path name) to which the serialized data should be written.
The file format is gzipped XML, which should be portable across most CPUs.

##### Default

- ```"/tmp/boost_serialized_drover.xml.gz"```

##### Examples

- ```"/tmp/out.xml.gz"```
- ```"./out.xml.gz"```
- ```"out.xml.gz"```


## BuildLexiconInteractively

### Description

This operation interactively builds a lexicon using the currently loaded contour
labels. It is useful for constructing a domain-specific lexicon from a set of
representative data.

### Parameters

- CleanLabels
- JunkLabel
- OmitROILabelRegex
- LexiconSeedFile

#### CleanLabels

##### Description

A listing of the labels of interest. These will be (some of) the 'clean' entries
in the finished lexicon. You should only name ROIs you specifically care about
and which have a single, unambiguous occurence in the data set (e.g.,
'Left_Parotid' is good, but 'JUNK' and 'Parotids' are bad -- you won't be able
to select the single 'JUNK' label if all you care about are parotids.

##### Default

- ```"Body,Brainstem,Chiasm,Cord,Larynx Pharynx,Left Eye,Left Optic Nerve,Left Parotid,Left Submand,Left Temp Lobe,Oral Cavity,Right Eye,Right Optic Nerve,Right Parotid,Right Submand,Right Temp Lobe"```

##### Examples

- ```"Left Parotid,Right Parotid,Left Submand,Right Submand"```
- ```"Left Submand,Right Submand"```

#### JunkLabel

##### Description

A label to apply to the un-matched labels. This helps prevent false positives by
excluding names which are close to a desired clean label. For example, if you
are looking for 'Left_Parotid' you will want to mark 'left-parotid_opti' and
'OLDLeftParotid' as junk. Passing an empty string disables junk labeling.

##### Default

- ```"JUNK"```

##### Examples

- ```""```
- ```"Junk"```
- ```"Irrelevant"```
- ```"NA_Organ"```

#### OmitROILabelRegex

##### Description

A regex matching ROI labels/names to prune. Only matching ROIs will be pruned.
The default will match no ROIs. Be aware that input spaces are trimmed to a
single space. If your ROI name has more than two sequential spaces, use regex to
avoid them. All ROIs have to match the single regex, so use the 'or' token if
needed. Regex is case insensitive and uses extended POSIX syntax. (Note: an
exclusive approach is taken rather than an inclusive approach because regex
negations are not easily supported in the POSIX syntax.)

##### Default

- ```""```

##### Examples

- ```".*left.*|.*right.*|.*eyes.*"```
- ```".*PTV.*|.*CTV.*|.*GTV.*"```

#### LexiconSeedFile

##### Description

A file containing a 'seed' lexicon to use and add to. This is the lexicon that
is being built. It will be modified.

##### Default

- ```""```

##### Examples

- ```"./some_lexicon"```
- ```"/tmp/temp_lexicon"```


## CT_Liver_Perfusion

### Description

This operation performed dynamic contrast-enhanced CT perfusion image modeling
on a time series image volume.

### Notes

- This routine is used for research purposes only.

### Parameters

No registered options.

## CT_Liver_Perfusion_First_Run

### Description

This operation performed dynamic contrast-enhanced CT perfusion image modeling
on a time series image volume.

### Notes

- Use this mode when peeking at the data for the first time. It avoids computing
  much, just lets you *look* at the data, find t_0, etc..

### Parameters

No registered options.

## CT_Liver_Perfusion_Ortho_Views

### Description

This operation performed dynamic contrast-enhanced CT perfusion image modeling
on a time series image volume.

### Notes

- Use this mode when you are only interested in oblique/orthogonal views. The
  point of this operation is to keep memory low so image sets can be compared.

### Parameters

No registered options.

## CT_Liver_Perfusion_Pharmaco_1C2I_5Param

### Description

This operation performed dynamic contrast-enhanced CT perfusion image modeling
on a time series image volume.

### Parameters

- AIFROINameRegex
- ExponentialKernelCoeffTruncation
- FastChebyshevMultiplication
- PlotAIFVIF
- PlotPixelModel
- PreDecimateOutSizeR
- PreDecimateOutSizeC
- TargetROINameRegex
- UseBasisSplineInterpolation
- BasisSplineCoefficients
- BasisSplineOrder
- UseChebyshevPolyMethod
- ChebyshevPolyCoefficients
- VIFROINameRegex

#### AIFROINameRegex

##### Description

Regex for the name of the ROI to use as the AIF. It should generally be a major
artery near the trunk or near the tissue of interest.

##### Default

- ```"Abdominal_Aorta"```

##### Examples

- ```"Abdominal_Aorta"```
- ```".*Aorta.*"```
- ```"Major_Artery"```

#### ExponentialKernelCoeffTruncation

##### Description

Control the number of Chebyshev coefficients used to approximate the exponential
kernel. Usually ~10 will suffice. ~20 is probably overkill, and ~5 is probably
too few. It is probably better to err on the side of caution and enlarge this
number if you're worried about loss of precision -- this will slow the
computation somewhat. (You might be able to offset by retaining fewer
coefficients in Chebyshev multiplication; see 'FastChebyshevMultiplication'
parameter.)

##### Default

- ```"10"```

##### Examples

- ```"20"```
- ```"15"```
- ```"10"```
- ```"5"```

#### FastChebyshevMultiplication

##### Description

Control coefficient truncation/pruning to speed up Chebyshev polynomial
multiplication. (This setting does nothing if the Chebyshev method is not being
used.) The choice of this number depends on how much precision you are willing
to forgo. It also strongly depends on the number of datum in the AIF, VIF, and
the number of coefficients used to approximate the exponential kernel (usually
~10 suffices). Numbers are specified relative to max(N,M), where N and M are the
number of coefficients in the two Chebyshev expansions taking part in the
multiplication. If too many coefficients are requested (i.e., more than (N+M-2))
then the full non-approximate multiplication is carried out.

##### Default

- ```"*10000000.0"```

##### Examples

- ```"*2.0"```
- ```"*1.5"```
- ```"*1.0"```
- ```"*0.5"```
- ```"*0.3"```

#### PlotAIFVIF

##### Description

Control whether the AIF and VIF should be shown prior to modeling.

##### Default

- ```"false"```

##### Examples

- ```"true"```
- ```"false"```

#### PlotPixelModel

##### Description

Show a plot of the fitted model for a specified pixel. Plotting happens
immediately after the pixel is processed. You can supply arbitrary metadata, but
must also supply Row and Column numbers. Note that numerical comparisons are
performed lexically, so you have to be exact. Also note the sub-separation token
is a semi-colon, not a colon.

##### Default

- ```""```

##### Examples

- ```"Row@12;Column@4;Description@.*k1A.*"```
- ```"Row@256;Column@500;SliceLocation@23;SliceThickness@0.5"```
- ```"Row@256;Column@500;Some@thing#Row@256;Column@501;Another@thing"```
- ```"Row@0;Column@5#Row@4;Column@5#Row@8;Column@5#Row@12;Column@5"```

#### PreDecimateOutSizeR

##### Description

The number of pixels along the row unit vector to group into an outgoing pixel.
This optional step can reduce computation effort by downsampling (decimating)
images before computing fitted parameter maps (but *after* computing AIF and VIF
time courses). Must be a multiplicative factor of the incoming image's row
count. No decimation occurs if either this or 'PreDecimateOutSizeC' is zero or
negative.

##### Default

- ```"8"```

##### Examples

- ```"0"```
- ```"2"```
- ```"4"```
- ```"8"```
- ```"16"```
- ```"32"```
- ```"64"```
- ```"128"```
- ```"256"```
- ```"512"```

#### PreDecimateOutSizeC

##### Description

The number of pixels along the column unit vector to group into an outgoing
pixel. This optional step can reduce computation effort by downsampling
(decimating) images before computing fitted parameter maps (but *after*
computing AIF and VIF time courses). Must be a multiplicative factor of the
incoming image's column count. No decimation occurs if either this or
'PreDecimateOutSizeR' is zero or negative.

##### Default

- ```"8"```

##### Examples

- ```"0"```
- ```"2"```
- ```"4"```
- ```"8"```
- ```"16"```
- ```"32"```
- ```"64"```
- ```"128"```
- ```"256"```
- ```"512"```

#### TargetROINameRegex

##### Description

Regex for the name of the ROI to perform modeling within. The largest contour is
usually what you want, but you can also be more focused.

##### Default

- ```".*Body.*"```

##### Examples

- ```"Liver_Patches_For_Testing_Smaller"```
- ```"Liver_Patches_For_Testing"```
- ```"Suspected_Liver_Rough"```
- ```"Rough_Body"```
- ```".*body.*"```
- ```".*something.*\|.*another.*thing.*"```

#### UseBasisSplineInterpolation

##### Description

Control whether the AIF and VIF should use basis spline interpolation in
conjunction with the Chebyshev polynomial method. If this option is not set,
linear interpolation is used instead. Linear interpolation may result in a
less-smooth AIF and VIF (and therefore possibly slower optimizer convergence),
but is safer if you cannot verify the AIF and VIF plots are reasonable. This
option currently produces an effect only if the Chebyshev polynomial method is
being used.

##### Default

- ```"false"```

##### Examples

- ```"true"```
- ```"false"```

#### BasisSplineCoefficients

##### Description

Control the number of basis spline coefficients to use, if applicable. (This
setting does nothing when basis splines are not being used.) Valid options for
this setting depend on the amount of data and b-spline order. This number
controls the number of coefficients that are fitted (via least-squares). You
must verify that overfitting is not happening. If in doubt, use fewer
coefficients. There are two ways to specify the number: relative and absolute.
Relative means relative to the number of datum. For example, if the AIF and VIF
have ~40 datum then generally '*0.5' is safe. ('*0.5' means there are half the
number of coefficients as datum.) Inspect for overfitting and poor fit. Because
this routine happens once and is fast, do not tweak to optimize for speed; the
aim of this method is to produce a smooth and accurate AIF and VIF. Because an
integer number of coefficients are needed, so rounding is used. You can also
specify the absolute number of coefficients to use like '20'. It often makes
more sense to use relative specification. Be aware that not all inputs can be
honoured due to limits on b-spline knots and breaks, and may cause unpredictable
behaviour or internal failure.

##### Default

- ```"*0.5"```

##### Examples

- ```"*0.8"```
- ```"*0.5"```
- ```"*0.3"```
- ```"20.0"```
- ```"10.0"```

#### BasisSplineOrder

##### Description

Control the polynomial order of basis spline interpolation to use, if
applicable. (This setting does nothing when basis splines are not being used.)
This parameter controls the order of polynomial used for b-spline interpolation,
and therefore has ramifications for the computability and numerical stability of
AIF and VIF derivatives. Stick with '4' or '5' if you're unsure.

##### Default

- ```"4"```

##### Examples

- ```"1"```
- ```"2"```
- ```"3"```
- ```"4"```
- ```"5"```
- ```"6"```
- ```"7"```
- ```"8"```
- ```"9"```
- ```"10"```

#### UseChebyshevPolyMethod

##### Description

Control whether the AIF and VIF should be approximated by Chebyshev polynomials.
If this option is not set, a inear interpolation approach is used instead.

##### Default

- ```"true"```

##### Examples

- ```"true"```
- ```"false"```

#### ChebyshevPolyCoefficients

##### Description

Control the number of Chebyshev polynomial coefficients to use, if applicable.
(This setting does nothing when the Chebyshev polynomial method is not being
used.) This number controls the number of coefficients that are computed. There
are two ways to specify the number: relative and absolute. Relative means
relative to the number of datum. For example, if the AIF and VIF have ~40 datum
then generally '*2' is safe. ('*2' means there are 2x the number of coefficients
as datum; usually overkill.) A good middle-ground is '*1' which is faster but
should produce similar results. For speed '/2' is even faster, but can produce
bad results in some cases. Because an integer number of coefficients are needed,
rounding is used. You can also specify the absolute number of coefficients to
use like '20'. It often makes more sense to use relative specification. Be aware
that not all inputs can be honoured (i.e., too large, too small, or negative),
and may cause unpredictable behaviour or internal failure.

##### Default

- ```"*2.0"```

##### Examples

- ```"*10.0"```
- ```"*5.0"```
- ```"*2.0"```
- ```"*1.23"```
- ```"*1.0"```
- ```"/1.0"```
- ```"/2.0"```
- ```"/3.0"```
- ```"/5.0"```
- ```"100.0"```
- ```"50.0"```
- ```"20"```
- ```"10.01"```

#### VIFROINameRegex

##### Description

Regex for the name of the ROI to use as the VIF. It should generally be a major
vein near the trunk or near the tissue of interest.

##### Default

- ```"Hepatic_Portal_Vein"```

##### Examples

- ```"Hepatic_Portal_Vein"```
- ```".*Portal.*Vein.*"```
- ```"Major_Vein"```


## CT_Liver_Perfusion_Pharmaco_1C2I_Reduced3Param

### Description

This operation performed dynamic contrast-enhanced CT perfusion image modeling
on a time series image volume.

### Parameters

- AIFROINameRegex
- ExponentialKernelCoeffTruncation
- FastChebyshevMultiplication
- PlotAIFVIF
- PlotPixelModel
- PreDecimateOutSizeR
- PreDecimateOutSizeC
- TargetROINameRegex
- UseBasisSplineInterpolation
- BasisSplineCoefficients
- BasisSplineOrder
- ChebyshevPolyCoefficients
- VIFROINameRegex

#### AIFROINameRegex

##### Description

Regex for the name of the ROI to use as the AIF. It should generally be a major
artery near the trunk or near the tissue of interest.

##### Default

- ```"Abdominal_Aorta"```

##### Examples

- ```"Abdominal_Aorta"```
- ```".*Aorta.*"```
- ```"Major_Artery"```

#### ExponentialKernelCoeffTruncation

##### Description

Control the number of Chebyshev coefficients used to approximate the exponential
kernel. Usually ~10 will suffice. ~20 is probably overkill, and ~5 is probably
too few. It is probably better to err on the side of caution and enlarge this
number if you're worried about loss of precision -- this will slow the
computation somewhat. (You might be able to offset by retaining fewer
coefficients in Chebyshev multiplication; see 'FastChebyshevMultiplication'
parameter.)

##### Default

- ```"10"```

##### Examples

- ```"20"```
- ```"15"```
- ```"10"```
- ```"5"```

#### FastChebyshevMultiplication

##### Description

Control coefficient truncation/pruning to speed up Chebyshev polynomial
multiplication. (This setting does nothing if the Chebyshev method is not being
used.) The choice of this number depends on how much precision you are willing
to forgo. It also strongly depends on the number of datum in the AIF, VIF, and
the number of coefficients used to approximate the exponential kernel (usually
~10 suffices). Numbers are specified relative to max(N,M), where N and M are the
number of coefficients in the two Chebyshev expansions taking part in the
multiplication. If too many coefficients are requested (i.e., more than (N+M-2))
then the full non-approximate multiplication is carried out.

##### Default

- ```"*10000000.0"```

##### Examples

- ```"*2.0"```
- ```"*1.5"```
- ```"*1.0"```
- ```"*0.5"```
- ```"*0.3"```

#### PlotAIFVIF

##### Description

Control whether the AIF and VIF should be shown prior to modeling.

##### Default

- ```"false"```

##### Examples

- ```"true"```
- ```"false"```

#### PlotPixelModel

##### Description

Show a plot of the fitted model for a specified pixel. Plotting happens
immediately after the pixel is processed. You can supply arbitrary metadata, but
must also supply Row and Column numbers. Note that numerical comparisons are
performed lexically, so you have to be exact. Also note the sub-separation token
is a semi-colon, not a colon.

##### Default

- ```""```

##### Examples

- ```"Row@12;Column@4;Description@.*k1A.*"```
- ```"Row@256;Column@500;SliceLocation@23;SliceThickness@0.5"```
- ```"Row@256;Column@500;Some@thing#Row@256;Column@501;Another@thing"```
- ```"Row@0;Column@5#Row@4;Column@5#Row@8;Column@5#Row@12;Column@5"```

#### PreDecimateOutSizeR

##### Description

The number of pixels along the row unit vector to group into an outgoing pixel.
This optional step can reduce computation effort by downsampling (decimating)
images before computing fitted parameter maps (but *after* computing AIF and VIF
time courses). Must be a multiplicative factor of the incoming image's row
count. No decimation occurs if either this or 'PreDecimateOutSizeC' is zero or
negative.

##### Default

- ```"8"```

##### Examples

- ```"0"```
- ```"2"```
- ```"4"```
- ```"8"```
- ```"16"```
- ```"32"```
- ```"64"```
- ```"128"```
- ```"256"```
- ```"512"```

#### PreDecimateOutSizeC

##### Description

The number of pixels along the column unit vector to group into an outgoing
pixel. This optional step can reduce computation effort by downsampling
(decimating) images before computing fitted parameter maps (but *after*
computing AIF and VIF time courses). Must be a multiplicative factor of the
incoming image's column count. No decimation occurs if either this or
'PreDecimateOutSizeR' is zero or negative.

##### Default

- ```"8"```

##### Examples

- ```"0"```
- ```"2"```
- ```"4"```
- ```"8"```
- ```"16"```
- ```"32"```
- ```"64"```
- ```"128"```
- ```"256"```
- ```"512"```

#### TargetROINameRegex

##### Description

Regex for the name of the ROI to perform modeling within. The largest contour is
usually what you want, but you can also be more focused.

##### Default

- ```".*Body.*"```

##### Examples

- ```"Liver_Patches_For_Testing_Smaller"```
- ```"Liver_Patches_For_Testing"```
- ```"Suspected_Liver_Rough"```
- ```"Rough_Body"```
- ```".*body.*"```
- ```".*something.*\|.*another.*thing.*"```

#### UseBasisSplineInterpolation

##### Description

Control whether the AIF and VIF should use basis spline interpolation in
conjunction with the Chebyshev polynomial method. If this option is not set,
linear interpolation is used instead. Linear interpolation may result in a
less-smooth AIF and VIF (and therefore possibly slower optimizer convergence),
but is safer if you cannot verify the AIF and VIF plots are reasonable. This
option currently produces an effect only if the Chebyshev polynomial method is
being used.

##### Default

- ```"false"```

##### Examples

- ```"true"```
- ```"false"```

#### BasisSplineCoefficients

##### Description

Control the number of basis spline coefficients to use, if applicable. (This
setting does nothing when basis splines are not being used.) Valid options for
this setting depend on the amount of data and b-spline order. This number
controls the number of coefficients that are fitted (via least-squares). You
must verify that overfitting is not happening. If in doubt, use fewer
coefficients. There are two ways to specify the number: relative and absolute.
Relative means relative to the number of datum. For example, if the AIF and VIF
have ~40 datum then generally '*0.5' is safe. ('*0.5' means there are half the
number of coefficients as datum.) Inspect for overfitting and poor fit. Because
this routine happens once and is fast, do not tweak to optimize for speed; the
aim of this method is to produce a smooth and accurate AIF and VIF. Because an
integer number of coefficients are needed, so rounding is used. You can also
specify the absolute number of coefficients to use like '20'. It often makes
more sense to use relative specification. Be aware that not all inputs can be
honoured due to limits on b-spline knots and breaks, and may cause unpredictable
behaviour or internal failure.

##### Default

- ```"*0.5"```

##### Examples

- ```"*0.8"```
- ```"*0.5"```
- ```"*0.3"```
- ```"20.0"```
- ```"10.0"```

#### BasisSplineOrder

##### Description

Control the polynomial order of basis spline interpolation to use, if
applicable. (This setting does nothing when basis splines are not being used.)
This parameter controls the order of polynomial used for b-spline interpolation,
and therefore has ramifications for the computability and numerical stability of
AIF and VIF derivatives. Stick with '4' or '5' if you're unsure.

##### Default

- ```"4"```

##### Examples

- ```"1"```
- ```"2"```
- ```"3"```
- ```"4"```
- ```"5"```
- ```"6"```
- ```"7"```
- ```"8"```
- ```"9"```
- ```"10"```

#### ChebyshevPolyCoefficients

##### Description

Control the number of Chebyshev polynomial coefficients to use, if applicable.
(This setting does nothing when the Chebyshev polynomial method is not being
used.) This number controls the number of coefficients that are computed. There
are two ways to specify the number: relative and absolute. Relative means
relative to the number of datum. For example, if the AIF and VIF have ~40 datum
then generally '*2' is safe. ('*2' means there are 2x the number of coefficients
as datum; usually overkill.) A good middle-ground is '*1' which is faster but
should produce similar results. For speed '/2' is even faster, but can produce
bad results in some cases. Because an integer number of coefficients are needed,
rounding is used. You can also specify the absolute number of coefficients to
use like '20'. It often makes more sense to use relative specification. Be aware
that not all inputs can be honoured (i.e., too large, too small, or negative),
and may cause unpredictable behaviour or internal failure.

##### Default

- ```"*2.0"```

##### Examples

- ```"*10.0"```
- ```"*5.0"```
- ```"*2.0"```
- ```"*1.23"```
- ```"*1.0"```
- ```"/1.0"```
- ```"/2.0"```
- ```"/3.0"```
- ```"/5.0"```
- ```"100.0"```
- ```"50.0"```
- ```"20"```
- ```"10.01"```

#### VIFROINameRegex

##### Description

Regex for the name of the ROI to use as the VIF. It should generally be a major
vein near the trunk or near the tissue of interest.

##### Default

- ```"Hepatic_Portal_Vein"```

##### Examples

- ```"Hepatic_Portal_Vein"```
- ```".*Portal.*Vein.*"```
- ```"Major_Vein"```


## ComparePixels

### Description

This operation compares images on a per-voxel/per-pixel basis.

### Notes

- Images are overwritten, but ReferenceImages are not. Multiple Images may be
  specified, but only one ReferenceImages may be specified.

### Parameters

- ImageSelection
- ReferenceImageSelection
- NormalizedROILabelRegex
- ROILabelRegex

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"all"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### ReferenceImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"all"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```


## ContourBasedRayCastDoseAccumulate

### Description

This operation performs ray-casting to estimate the dose of a surface. The
surface is represented as a set of contours (i.e., an ROI).

### Parameters

- DoseLengthMapFileName
- LengthMapFileName
- NormalizedROILabelRegex
- ROILabelRegex
- CylinderRadius
- RaydL
- Rows
- Columns

#### DoseLengthMapFileName

##### Description

A filename (or full path) for the (dose)*(length traveled through the ROI peel)
image map. The format is TBD. Leave empty to dump to generate a unique temporary
file.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.img"```
- ```"derivative_data.img"```

#### LengthMapFileName

##### Description

A filename (or full path) for the (length traveled through the ROI peel) image
map. The format is TBD. Leave empty to dump to generate a unique temporary file.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.img"```
- ```"derivative_data.img"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### CylinderRadius

##### Description

The radius of the cylinder surrounding contour line segments that defines the
'surface'. Quantity is in the DICOM coordinate system.

##### Default

- ```"3.0"```

##### Examples

- ```"1.0"```
- ```"2.0"```
- ```"0.5"```
- ```"5.0"```

#### RaydL

##### Description

The distance to move a ray each iteration. Should be << img_thickness and <<
cylinder_radius. Making too large will invalidate results, causing rays to pass
through the surface without registering any dose accumulation. Making too small
will cause the run-time to grow and may eventually lead to truncation or
round-off errors. Quantity is in the DICOM coordinate system.

##### Default

- ```"0.1"```

##### Examples

- ```"0.1"```
- ```"0.05"```
- ```"0.01"```
- ```"0.005"```

#### Rows

##### Description

The number of rows in the resulting images.

##### Default

- ```"256"```

##### Examples

- ```"10"```
- ```"50"```
- ```"128"```
- ```"1024"```

#### Columns

##### Description

The number of columns in the resulting images.

##### Default

- ```"256"```

##### Examples

- ```"10"```
- ```"50"```
- ```"128"```
- ```"1024"```


## ContourBooleanOperations

### Description

This routine performs 2D Boolean operations on user-provided sets of ROIs. The
ROIs themselves are planar contours embedded in R^3, but the Boolean operation
is performed once for each 2D plane where the selected ROIs reside. This routine
can only perform Boolean operations on co-planar contours. This routine can
operate on single contours (rather than ROIs composed of several contours) by
simply presenting this routine with a single contour to select.

### Notes

- This routine DOES support disconnected ROIs, such as left- and right-parotid
  contours that have been joined into a single 'parotids' ROI.

- Many Boolean operations can produce contours with holes. This operation
  currently connects the interior and exterior with a seam so that holes can be
  represented by a single polygon (rather than a separate hole polygon). It *is*
  possible to export holes as contours with a negative orientation, but this was
  not needed when writing.

- Only the common metadata between contours is propagated to the product
  contours.

### Parameters

- ROILabelRegexA
- ROILabelRegexB
- NormalizedROILabelRegexA
- NormalizedROILabelRegexB
- Operation
- OutputROILabel

#### ROILabelRegexA

##### Description

A regex matching ROI labels/names that comprise the set of contour polygons 'A'
as in f(A,B) where f is some Boolean operation. The default with match all
available ROIs, which is probably not what you want.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*[pP]rostate.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### ROILabelRegexB

##### Description

A regex matching ROI labels/names that comprise the set of contour polygons 'B'
as in f(A,B) where f is some Boolean operation. The default with match all
available ROIs, which is probably not what you want.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### NormalizedROILabelRegexA

##### Description

A regex matching ROI labels/names that comprise the set of contour polygons 'A'
as in f(A,B) where f is some Boolean operation. The regex is applied to
normalized ROI labels/names, which are translated using a user-provided lexicon
(i.e., a dictionary that supports fuzzy matching). The default with match all
available ROIs, which is probably not what you want.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### NormalizedROILabelRegexB

##### Description

A regex matching ROI labels/names that comprise the set of contour polygons 'B'
as in f(A,B) where f is some Boolean operation. The regex is applied to
normalized ROI labels/names, which are translated using a user-provided lexicon
(i.e., a dictionary that supports fuzzy matching). The default with match all
available ROIs, which is probably not what you want.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### Operation

##### Description

The Boolean operation (e.g., the function 'f') to perform on the sets of contour
polygons 'A' and 'B'. 'Symmetric difference' is also known as 'XOR'.

##### Default

- ```"join"```

##### Examples

- ```"intersection"```
- ```"join"```
- ```"difference"```
- ```"symmetric_difference"```

#### OutputROILabel

##### Description

The label to attach to the ROI contour product of f(A,B).

##### Default

- ```"Boolean_result"```

##### Examples

- ```"A+B"```
- ```"A-B"```
- ```"AuB"```
- ```"AnB"```
- ```"AxB"```
- ```"A^B"```
- ```"union"```
- ```"xor"```
- ```"combined"```
- ```"body_without_spinal_cord"```


## ContourSimilarity

### Description

This operation estimates the similarity or overlap between two sets of contours.
The comparison is based on point samples. It is useful for comparing contouring
styles.

### Notes

- This routine requires an image grid, which is used to control the sampling
  rate.

### Parameters

No registered options.

## ContourViaThreshold

### Description

This operation constructs ROI contours using images and pixel/voxel value
thresholds. The output is 'ephemeral' and is not commited to any database.

### Notes

- This routine expects images to be non-overlapping. In other words, if images
  overlap then the contours generated may also overlap. This is probably not
  what you want (but there is nothing intrinsically wrong with presenting this
  routine with multiple images if you intentionally want overlapping contours).

- Existing contours are ignored and unaltered.

- Contour orientation is (likely) not properly handled in this routine, so
  'pinches' and holes will produce contours with inconsistent or invalid
  topology. If in doubt, disable merge simplifications and live with the
  computational penalty.

### Parameters

- ROILabel
- Lower
- Upper
- Channel
- ImageSelection
- SimplifyMergeAdjacent

#### ROILabel

##### Description

A label to attach to the ROI contours.

##### Default

- ```"unspecified"```

##### Examples

- ```"unspecified"```
- ```"body"```
- ```"air"```
- ```"bone"```
- ```"invalid"```
- ```"above_zero"```
- ```"below_5.3"```

#### Lower

##### Description

The lower bound (inclusive). Pixels with values < this number are excluded from
the ROI. If the number is followed by a '%', the bound will be scaled between
the min and max pixel values [0-100%]. If the number is followed by 'tile', the
bound will be replaced with the corresponding percentile [0-100tile]. Note that
upper and lower bounds can be specified separately (e.g., lower bound is a
percentage, but upper bound is a percentile).

##### Default

- ```"-inf"```

##### Examples

- ```"0.0"```
- ```"-1E-99"```
- ```"1.23"```
- ```"0.2%"```
- ```"23tile"```
- ```"23.123 tile"```

#### Upper

##### Description

The upper bound (inclusive). Pixels with values > this number are excluded from
the ROI. If the number is followed by a '%', the bound will be scaled between
the min and max pixel values [0-100%]. If the number is followed by 'tile', the
bound will be replaced with the corresponding percentile [0-100tile]. Note that
upper and lower bounds can be specified separately (e.g., lower bound is a
percentage, but upper bound is a percentile).

##### Default

- ```"inf"```

##### Examples

- ```"1.0"```
- ```"1E-99"```
- ```"2.34"```
- ```"98.12%"```
- ```"94tile"```
- ```"94.123 tile"```

#### Channel

##### Description

The image channel to use. Zero-based.

##### Default

- ```"0"```

##### Examples

- ```"0"```
- ```"1"```
- ```"2"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### SimplifyMergeAdjacent

##### Description

Simplify contours by merging adjacent contours. This reduces the number of
contours dramatically, but will cause issues if there are holes (two contours
are generated if there is a single hole, but most DICOMautomaton code disregards
orientation -- so the pixels within the hole will be considered part of the ROI,
possibly even doubly so depending on the algorithm). Disabling merges is always
safe (and is therefore the default) but can be extremely costly for large
images. Furthermore, if you know the ROI does not have holes (or if you don't
care) then it is safe to enable merges.

##### Default

- ```"false"```

##### Examples

- ```"true"```
- ```"false"```


## ContourVote

### Description

This routine pits contours against one another using various criteria. A number
of 'closest' or 'best' or 'winning' contours are copied into a new contour
collection with the specified ROILabel. The original ROIs are not altered, even
the winning ROIs.

### Notes

- This operation considers individual contours only at the moment. It could be
  extended to operate on whole ROIs (i.e., contour_collections), or to perform a
  separate vote within each ROI. The individual contour approach was taken for
  relevance in 2D image (e.g., RTIMAGE) analysis.

- This operation currently cannot perform voting on multiple criteria. Several
  criteria could be specified, but an awkward weighting system would also be
  needed.

### Parameters

- WinnerROILabel
- ROILabelRegex
- NormalizedROILabelRegex
- Area
- Perimeter
- CentroidX
- CentroidY
- CentroidZ
- WinnerCount

#### WinnerROILabel

##### Description

The ROI label to attach to the winning contour(s). All other metadata remains
the same.

##### Default

- ```"unspecified"```

##### Examples

- ```"closest"```
- ```"best"```
- ```"winners"```
- ```"best-matches"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### Area

##### Description

If this option is provided with a valid positive number, the contour(s) with an
area closest to the specified value is/are retained. Note that the DICOM
coordinate space is used. (Supplying the default, NaN, will disable this
option.) Note: if several criteria are specified, it is not specified in which
order they are considered.

##### Default

- ```"nan"```

##### Examples

- ```"nan"```
- ```"100.0"```
- ```"1000"```
- ```"10.23E8"```

#### Perimeter

##### Description

If this option is provided with a valid positive number, the contour(s) with a
perimeter closest to the specified value is/are retained. Note that the DICOM
coordinate space is used. (Supplying the default, NaN, will disable this
option.) Note: if several criteria are specified, it is not specified in which
order they are considered.

##### Default

- ```"nan"```

##### Examples

- ```"nan"```
- ```"0.0"```
- ```"123.456"```
- ```"1E6"```

#### CentroidX

##### Description

If this option is provided with a valid positive number, the contour(s) with a
centroid closest to the specified value is/are retained. Note that the DICOM
coordinate space is used. (Supplying the default, NaN, will disable this
option.) Note: if several criteria are specified, it is not specified in which
order they are considered.

##### Default

- ```"nan"```

##### Examples

- ```"nan"```
- ```"0.0"```
- ```"123.456"```
- ```"-1E6"```

#### CentroidY

##### Description

If this option is provided with a valid positive number, the contour(s) with a
centroid closest to the specified value is/are retained. Note that the DICOM
coordinate space is used. (Supplying the default, NaN, will disable this
option.) Note: if several criteria are specified, it is not specified in which
order they are considered.

##### Default

- ```"nan"```

##### Examples

- ```"nan"```
- ```"0.0"```
- ```"123.456"```
- ```"-1E6"```

#### CentroidZ

##### Description

If this option is provided with a valid positive number, the contour(s) with a
centroid closest to the specified value is/are retained. Note that the DICOM
coordinate space is used. (Supplying the default, NaN, will disable this
option.) Note: if several criteria are specified, it is not specified in which
order they are considered.

##### Default

- ```"nan"```

##### Examples

- ```"nan"```
- ```"0.0"```
- ```"123.456"```
- ```"-1E6"```

#### WinnerCount

##### Description

Retain this number of 'best' or 'winning' contours.

##### Default

- ```"1"```

##### Examples

- ```"0"```
- ```"1"```
- ```"3"```
- ```"10000"```


## ContourWholeImages

### Description

This operation constructs contours for an ROI that encompasses the whole of all
specified images. It is useful for operations that operate on ROIs whenever you
want to compute something over the whole image. This routine avoids having to
manually contour anything. The output is 'ephemeral' and is not commited to any
database.

### Notes

- This routine will attempt to avoid repeat contours. Generated contours are
  tested for intersection with an image before the image is processed.

- Existing contours are ignored and unaltered.

- Contours are set slightly inside the outer boundary so they can be easily
  visualized by overlaying on the image. All voxel centres will be within the
  bounds.

### Parameters

- ROILabel
- ImageSelection

#### ROILabel

##### Description

A label to attach to the ROI contours.

##### Default

- ```"everything"```

##### Examples

- ```"everything"```
- ```"whole_images"```
- ```"unspecified"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```


## ContouringAides

### Description

This operation attempts to prepare an image for easier contouring.

### Notes

- At the moment, only logarithmic scaling is applied.

### Parameters

No registered options.

## ConvertDoseToImage

### Description

This operation converts all loaded Dose_Arrays to Image_Arrays. Neither image
contents nor metadata should change, but the intent to treat as an image or dose
matrix will of course change. A deep copy may be performed.

### Parameters

No registered options.

## ConvertImageToDose

### Description

This operation converts all loaded Image_Arrays to Dose_Arrays. Neither image
contents nor metadata should change, but the intent to treat as an image or dose
matrix will of course change. A deep copy may be performed.

### Parameters

No registered options.

## ConvertNaNsToAir

### Description

This operation runs the data through a per-pixel filter, converting NaN's to air
in Hounsfield units (-1024).

### Parameters

No registered options.

## ConvertNaNsToZeros

### Description

This operation runs the data through a per-pixel filter, converting NaN's to
zeros.

### Parameters

No registered options.

## CopyImages

### Description

This operation deep-copies the selected image arrays.

### Parameters

- ImageSelection

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```


## CropImageDoseToROIs

### Description

This operation crops image and/or dose array slices to the specified ROI(s),
with an additional margin.

### Parameters

- DICOMMargin
- DoseImageSelection
- ImageSelection
- NormalizedROILabelRegex
- ROILabelRegex

#### DICOMMargin

##### Description

The amount of margin (in the DICOM coordinate system) to surround the ROI(s).

##### Default

- ```"0.5"```

##### Examples

- ```"0.1"```
- ```"2.0"```
- ```"-0.5"```
- ```"20.0"```

#### DoseImageSelection

##### Description

Dose images to operate on. Either 'none', 'last', or 'all'.

##### Default

- ```"none"```

##### Examples

- ```"none"```
- ```"last"```
- ```"all"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```


## CropImages

### Description

This operation crops image slices in either pixel or DICOM coordinate spaces.

### Parameters

- ImageSelection
- RowsL
- RowsH
- ColumnsL
- ColumnsH
- DICOMMargin

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"all"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### RowsL

##### Description

The number of rows to remove, starting with the first row. Can be absolute (px),
percentage (%), or distance in terms of the DICOM coordinate system. Note the
DICOM coordinate system can be flipped, so the first row can be either on the
top or bottom of the image.

##### Default

- ```"0px"```

##### Examples

- ```"0px"```
- ```"10px"```
- ```"100px"```
- ```"15%"```
- ```"15.75%"```
- ```"123.45"```

#### RowsH

##### Description

The number of rows to remove, starting with the last row. Can be absolute (px),
percentage (%), or distance in terms of the DICOM coordinate system. Note the
DICOM coordinate system can be flipped, so the first row can be either on the
top or bottom of the image.

##### Default

- ```"0px"```

##### Examples

- ```"0px"```
- ```"10px"```
- ```"100px"```
- ```"15%"```
- ```"15.75%"```
- ```"123.45"```

#### ColumnsL

##### Description

The number of columns to remove, starting with the first column. Can be absolute
(px), percentage (%), or distance in terms of the DICOM coordinate system. Note
the DICOM coordinate system can be flipped, so the first column can be either on
the top or bottom of the image.

##### Default

- ```"0px"```

##### Examples

- ```"0px"```
- ```"10px"```
- ```"100px"```
- ```"15%"```
- ```"15.75%"```
- ```"123.45"```

#### ColumnsH

##### Description

The number of columns to remove, starting with the last column. Can be absolute
(px), percentage (%), or distance in terms of the DICOM coordinate system. Note
the DICOM coordinate system can be flipped, so the first column can be either on
the top or bottom of the image.

##### Default

- ```"0px"```

##### Examples

- ```"0px"```
- ```"10px"```
- ```"100px"```
- ```"15%"```
- ```"15.75%"```
- ```"123.45"```

#### DICOMMargin

##### Description

The amount of margin (in the DICOM coordinate system) to spare from cropping.

##### Default

- ```"0.0"```

##### Examples

- ```"0.1"```
- ```"2.0"```
- ```"-0.5"```
- ```"20.0"```


## CropROIDose

### Description

This operation provides a simplified interface for overriding the dose outside a
ROI. For example, this operation can be used to modify a base plan by
eliminating dose outside an OAR.

### Notes

- This operation performs the opposite of the 'Trim' operation, which trims the
  dose inside a ROI.

- The inclusivity of a dose voxel that straddles the ROI boundary can be
  specified in various ways. Refer to the Inclusivity parameter documentation.

- By default this operation only overrides dose within a ROI. The opposite,
  overriding dose outside of a ROI, can be accomplished using the expert
  interface.

### Parameters

- Channel
- ImageSelection
- ContourOverlap
- Inclusivity
- ExteriorVal
- InteriorVal
- ExteriorOverwrite
- InteriorOverwrite
- NormalizedROILabelRegex
- ROILabelRegex
- Filename
- ParanoiaLevel

#### Channel

##### Description

The image channel to use. Zero-based. Use '-1' to operate on all available
channels.

##### Default

- ```"-1"```

##### Examples

- ```"-1"```
- ```"0"```
- ```"1"```
- ```"2"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"all"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### ContourOverlap

##### Description

Controls overlapping contours are treated. The default 'ignore' treats
overlapping contours as a single contour, regardless of contour orientation. The
option 'honour_opposite_orientations' makes overlapping contours with opposite
orientation cancel. Otherwise, orientation is ignored. The latter is useful for
Boolean structures where contour orientation is significant for interior
contours (holes). The option 'overlapping_contours_cancel' ignores orientation
and cancels all contour overlap.

##### Default

- ```"ignore"```

##### Examples

- ```"ignore"```
- ```"honour_opposite_orientations"```
- ```"overlapping_contours_cancel"```
- ```"honour_opps"```
- ```"overlap_cancel"```

#### Inclusivity

##### Description

Controls how voxels are deemed to be 'within' the interior of the selected
ROI(s). The default 'center' considers only the central-most point of each
voxel. There are two corner options that correspond to a 2D projection of the
voxel onto the image plane. The first, 'planar_corner_inclusive', considers a
voxel interior if ANY corner is interior. The second, 'planar_corner_exclusive',
considers a voxel interior if ALL (four) corners are interior.

##### Default

- ```"planar_inc"```

##### Examples

- ```"center"```
- ```"centre"```
- ```"planar_corner_inclusive"```
- ```"planar_inc"```
- ```"planar_corner_exclusive"```
- ```"planar_exc"```

#### ExteriorVal

##### Description

The value to give to voxels outside the specified ROI(s). Note that this value
will be ignored if exterior overwrites are disabled.

##### Default

- ```"0.0"```

##### Examples

- ```"0.0"```
- ```"-1.0"```
- ```"1.23"```
- ```"2.34E26"```

#### InteriorVal

##### Description

The value to give to voxels within the volume of the specified ROI(s). Note that
this value will be ignored if interior overwrites are disabled.

##### Default

- ```"0.0"```

##### Examples

- ```"0.0"```
- ```"-1.0"```
- ```"1.23"```
- ```"2.34E26"```

#### ExteriorOverwrite

##### Description

Whether to overwrite voxels exterior to the specified ROI(s).

##### Default

- ```"true"```

##### Examples

- ```"true"```
- ```"false"```

#### InteriorOverwrite

##### Description

Whether to overwrite voxels interior to the specified ROI(s).

##### Default

- ```"false"```

##### Examples

- ```"true"```
- ```"false"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### Filename

##### Description

The filename (or full path name) to which the DICOM file should be written.

##### Default

- ```"/tmp/RD.dcm"```

##### Examples

- ```"/tmp/RD.dcm"```
- ```"./RD.dcm"```
- ```"RD.dcm"```

#### ParanoiaLevel

##### Description

At low paranoia setting, only top-level UIDs are replaced. At medium paranoia
setting, many UIDs, descriptions, and labels are replaced, but the PatientID and
FrameOfReferenceUID are retained. The high paranoia setting is the same as the
medium setting, but the PatientID and FrameOfReferenceUID are also replaced.
(Note: this is not a full anonymization.) Use the low setting if you want to
retain linkage to the originating data set. Use the medium setting if you don't.
Use the high setting if your TPS goes overboard linking data sets by PatientID
and/or FrameOfReferenceUID.

##### Default

- ```"medium"```

##### Examples

- ```"low"```
- ```"medium"```
- ```"high"```


## DCEMRI_IAUC

### Description

This operation will compute the Integrated Area Under the Curve (IAUC) for any
images present.

### Notes

- This operation is not optimized in any way and operates on whole images. It
  can be fairly slow, especially if the image volume is huge, so it is best to
  crop images if possible.

### Parameters

No registered options.

## DCEMRI_Nonparametric_CE

### Description

This operation takes a single DCE-MRI scan ('measurement') and generates a
"poor-mans's" contrast enhancement signal. This is accomplished by subtracting
the pre-contrast injection images average ('baseline') from later images (and
then possibly/optionally averaging relative to the baseline).

### Notes

- Only a single image volume is required. It is expected to have temporal
  sampling beyond the contrast injection timepoint (or some default value --
  currently around ~30s). The resulting images retain the baseline portion, so
  you'll need to trim yourself if needed.

- Be aware that this method of deriving contrast enhancement is not valid! It
  ignores nuances due to differing T1 or T2 values due to the presence of
  contrast agent. It should only be used for exploratory purposes or cases where
  the distinction with reality is irrelevant.

### Parameters

No registered options.

## DICOMExportImagesAsDose

### Description

This operation exports the last Image_Array to a DICOM dose file.

### Notes

- There are various 'paranoia' levels that can be used to partially anonymize
  the output. In particular, most metadata and UIDs are replaced, but the files
  may still be recognized by a determined individual by comparing the coordinate
  system and pixel values. Do NOT rely on this routine to fully anonymize the
  data!

### Parameters

- Filename
- ParanoiaLevel

#### Filename

##### Description

The filename (or full path name) to which the DICOM file should be written.

##### Default

- ```"/tmp/RD.dcm"```

##### Examples

- ```"/tmp/RD.dcm"```
- ```"./RD.dcm"```
- ```"RD.dcm"```

#### ParanoiaLevel

##### Description

At low paranoia setting, only top-level UIDs are replaced. At medium paranoia
setting, many UIDs, descriptions, and labels are replaced, but the PatientID and
FrameOfReferenceUID are retained. The high paranoia setting is the same as the
medium setting, but the PatientID and FrameOfReferenceUID are also replaced.
(Note: this is not a full anonymization.) Use the low setting if you want to
retain linkage to the originating data set. Use the medium setting if you don't.
Use the high setting if your TPS goes overboard linking data sets by PatientID
and/or FrameOfReferenceUID.

##### Default

- ```"medium"```

##### Examples

- ```"low"```
- ```"medium"```
- ```"high"```


## DecayDoseOverTimeHalve

### Description

This operation transforms a dose map (assumed to be delivered some distant time
in the past) to simulate 'decay' or 'evaporation' or 'forgivance' of radiation
dose by simply halving the value. This model is only appropriate at long
time-scales, but there is no cut-off or threshold to denote what is sufficiently
'long'. So use at your own risk. As a rule of thumb, do not use this routine if
fewer than 2-3y have elapsed.

### Notes

- This routine uses image_arrays so convert dose_arrays beforehand.

- This routine will combine spatially-overlapping images by summing voxel
  intensities. So if you have a time course it may be more sensible to aggregate
  images in some way (e.g., spatial averaging) prior to calling this routine.

- Since this routine is meant to be applied multiple times in succession for
  different ROIs (which possibly overlap), all images are imbued with a second
  channel that is treated as a mask. Mask channels are permanently attached so
  that multiple passes will not erroneously decay dose. If this will be
  problematic, the extra column should be trimmed immediately after calling this
  routine.

### Parameters

- NormalizedROILabelRegex
- ROILabelRegex

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```


## DecayDoseOverTimeJones2014

### Description

This operation transforms a dose map (assumed to be delivered some time in the
past) to 'decay' or 'evaporate' or 'forgive' some of the dose using the
time-dependent model of Jones and Grant (2014; doi:10.1016/j.clon.2014.04.027).
This model is specific to reirradiation of central nervous tissues. See the
Jones and Grant paper or 'Nasopharyngeal Carcinoma' by Wai Tong Ng et al. (2016;
doi:10.1007/174_2016_48) for more information.

### Notes

- This routine uses image_arrays so convert dose_arrays beforehand.

- This routine will combine spatially-overlapping images by summing voxel
  intensities. So if you have a time course it may be more sensible to aggregate
  images in some way (e.g., spatial averaging) prior to calling this routine.

- Since this routine is meant to be applied multiple times in succession for
  different ROIs (which possibly overlap), all images are imbued with a second
  channel that is treated as a mask. Mask channels are permanently attached so
  that multiple passes will not erroneously decay dose. If this will be
  problematic, the extra column should be trimmed immediately after calling this
  routine.

### Parameters

- NormalizedROILabelRegex
- ROILabelRegex
- Course1NumberOfFractions
- ToleranceTotalDose
- ToleranceNumberOfFractions
- TimeGap
- AlphaBetaRatio
- UseMoreConservativeRecovery

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### Course1NumberOfFractions

##### Description

The number of fractions delivered for the first (i.e., previous) course. If
several apply, you can provide a single effective fractionation scheme's 'n'.

##### Default

- ```"35"```

##### Examples

- ```"15"```
- ```"25"```
- ```"30.001"```
- ```"35.3"```

#### ToleranceTotalDose

##### Description

The dose delivered (in Gray) for a hypothetical 'lifetime dose tolerance'
course. This dose corresponds to a hypothetical radiation course that nominally
corresponds to the toxicity of interest. For CNS tissues, it will probably be
myelopathy or necrosis at some population-level onset risk (e.g., 5% risk of
myelopathy). The value provided will be converted to a BED_{a/b} so you can
safely provide a 'nominal' value. Be aware that each voxel is treated
independently, rather than treating OARs/ROIs as a whole. (Many dose limits
reported in the literature use whole-ROI D_mean or D_max, and so may be not be
directly applicable to per-voxel risk estimation!) Note that the QUANTEC 2010
reports almost all assume 2 Gy/fraction. If several fractionation schemes were
used, you should provide a cumulative BED-derived dose here.

##### Default

- ```"52"```

##### Examples

- ```"15"```
- ```"20"```
- ```"25"```
- ```"50"```
- ```"83.2"```

#### ToleranceNumberOfFractions

##### Description

The number of fractions ('n') the 'lifetime dose tolerance' toxicity you are
interested in. Note that this is converted to a BED_{a/b} so you can safely
provide a 'nominal' value. If several apply, you can provide a single effective
fractionation scheme's 'n'.

##### Default

- ```"35"```

##### Examples

- ```"15"```
- ```"25"```
- ```"30.001"```
- ```"35.3"```

#### TimeGap

##### Description

The number of years between radiotherapy courses. Note that this is normally
estimated by (1) extracting study/series dates from the provided dose files and
(2) using the current date as the second course date. Use this parameter to
override the autodetected gap time. Note: if the provided value is negative,
autodetection will be used.

##### Default

- ```"-1"```

##### Examples

- ```"0.91"```
- ```"2.6"```
- ```"5"```

#### AlphaBetaRatio

##### Description

The ratio alpha/beta (in Gray) to use when converting to a
biologically-equivalent dose distribution for central nervous tissues. Jones and
Grant (2014) recommend alpha/beta = 2 Gy to be conservative. It is more
commonplace to use alpha/beta = 3 Gy, but this is less conservative and there is
some evidence that it may be erroneous to use 3 Gy.

##### Default

- ```"2"```

##### Examples

- ```"2"```
- ```"3"```

#### UseMoreConservativeRecovery

##### Description

Jones and Grant (2014) provide two ways to estimate the function 'r'. One is
fitted to experimental data, and one is a more conservative estimate of the
fitted function. This parameter controls whether or not the more conservative
function is used.

##### Default

- ```"true"```

##### Examples

- ```"true"```
- ```"false"```


## DecimatePixels

### Description

This operation spatially aggregates blocks of pixels, thereby decimating them
and making the images consume far less memory. The precise size reduction and
spatial aggregate can be set in the source.

### Parameters

- OutSizeR
- OutSizeC

#### OutSizeR

##### Description

The number of pixels along the row unit vector to group into an outgoing pixel.
Must be a multiplicative factor of the incoming image's row count. No decimation
occurs if either this or 'OutSizeC' is zero or negative.

##### Default

- ```"8"```

##### Examples

- ```"0"```
- ```"2"```
- ```"4"```
- ```"8"```
- ```"16"```
- ```"32"```
- ```"64"```
- ```"128"```
- ```"256"```
- ```"512"```

#### OutSizeC

##### Description

The number of pixels along the column unit vector to group into an outgoing
pixel. Must be a multiplicative factor of the incoming image's column count. No
decimation occurs if either this or 'OutSizeR' is zero or negative.

##### Default

- ```"8"```

##### Examples

- ```"0"```
- ```"2"```
- ```"4"```
- ```"8"```
- ```"16"```
- ```"32"```
- ```"64"```
- ```"128"```
- ```"256"```
- ```"512"```


## DeleteImages

### Description

This routine deletes images from memory. It is most useful when working with
positional operations in stages.

### Parameters

- ImageSelection

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"all"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```


## DetectShapes3D

### Description

This operation attempts to detect shapes in image volumes.

### Parameters

- ImageSelection

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"all"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```


## DroverDebug

### Description

This operation reports basic information on the state of the main Drover class.
It can be used to report on the state of the data, which can be useful for
debugging.

### Parameters

No registered options.

## DumpAllOrderedImageMetadataToFile

### Description

Dump exactly what order the data will be in for the following analysis.

### Parameters

No registered options.

## DumpAnEncompassedPoint

### Description

This operation estimates the number of spatially-overlapping images. It finds an
arbitrary point within an arbitrary image, and then finds all other images which
encompass the point.

### Parameters

No registered options.

## DumpFilesPartitionedByTime

### Description

This operation prints PACS filenames along with the associated time. It is more
focused than the metadata dumpers above. This data can be used for many things,
such as image viewers which are not DICOM-aware or deformable registration on
time series data.

### Parameters

No registered options.

## DumpImageMetadataOccurrencesToFile

### Description

Dump all the metadata elements, but group like-items together and also print the
occurence number.

### Parameters

No registered options.

## DumpPerROIParams_KineticModel_1C2I_5P

### Description

Given a perfusion model, this routine computes parameter estimates for ROIs.

### Parameters

- ROILabelRegex
- Filename
- Separator

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### Filename

##### Description

A file into which the results should be dumped. If the filename is empty, the
results are dumped to the console only.

##### Default

- ```""```

##### Examples

- ```"/tmp/results.txt"```
- ```"/dev/null"```
- ```"~/output.txt"```

#### Separator

##### Description

The token(s) to place between adjacent columns of output. Note: because
whitespace is trimmed from user parameters, whitespace separators other than the
default are shortened to an empty string. So non-default whitespace are not
currently supported.

##### Default

- ```" "```

##### Examples

- ```","```
- ```";"```
- ```"_a_long_separator_"```


## DumpPixelValuesOverTimeForAnEncompassedPoint

### Description

Output the pixel values over time for a generic point. Currently the point is
arbitrarily taken to tbe the centre of the first image. This is useful for
quickly and programmatically inspecting trends, but the SFML_Viewer operation is
better for interactive exploration.

### Parameters

No registered options.

## DumpROIContours

### Description

This operation exports contours in a standard surface mesh format (structured
ASCII Wavefront OBJ) in planar polygon format. A companion material library file
(MTL) assigns colours to each ROI to help differentiate them.

### Notes

- Contours that are grouped together into a contour_collection are treated as a
  logical within the output. For example, all contours in a collection will
  share a common material property (e.g., colour). If more fine-grained grouping
  is required, this routine can be called once for each group which will result
  in a logical grouping of one ROI per file.

### Parameters

- DumpFileName
- MTLFileName
- NormalizedROILabelRegex
- ROILabelRegex

#### DumpFileName

##### Description

A filename (or full path) in which to (over)write with contour data. File format
is Wavefront obj. Leave empty to dump to generate a unique temporary file.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile.obj"```
- ```"localfile.obj"```
- ```"derivative_data.obj"```

#### MTLFileName

##### Description

A filename (or full path) in which to (over)write a Wavefront material library
file. This file is used to colour the contours to help differentiate them. Leave
empty to dump to generate a unique temporary file.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/materials.mtl"```
- ```"localfile.mtl"```
- ```"somefile.mtl"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```


## DumpROIData

### Description

This operation dumps ROI contour information for debugging and quick inspection
purposes.

### Parameters

No registered options.

## DumpROIDoseInfo

### Description

This operation computes mean voxel doses with the given ROIs.

### Parameters

- ROILabelRegex

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses grep syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*parotid.*|.*sub.*mand.*"```
- ```"left_parotid|right_parotid|eyes"```


## DumpROISNR

### Description

This operation computes the Signal-to-Noise ratio (SNR) for each ROI. The
specific 'SNR' computed is SNR = (mean pixel) / (pixel std dev) which is the
inverse of the coefficient of variation.

### Notes

- This routine uses image_arrays so convert dose_arrays beforehand if dose SNR
  is desired.

- This routine will combine spatially-overlapping images by summing voxel
  intensities. So if you have a time course it may be more sensible to aggregate
  images in some way (e.g., spatial averaging) prior to calling this routine.

### Parameters

- SNRFileName
- NormalizedROILabelRegex
- ROILabelRegex

#### SNRFileName

##### Description

A filename (or full path) in which to append SNR data generated by this routine.
The format is CSV. Leave empty to dump to generate a unique temporary file.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"derivative_data.csv"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```


## DumpROISurfaceMeshes

### Description

This operation generates surface meshes from contour volumes. Output is written
to file(s) for viewing with an external viewer (e.g., meshlab).

### Parameters

- OutDir
- NormalizedROILabelRegex
- ROILabelRegex

#### OutDir

##### Description

The directory in which to dump surface mesh files. It will be created if it
doesn't exist.

##### Default

- ```"/tmp/"```

##### Examples

- ```"./"```
- ```"../somedir/"```
- ```"/path/to/some/destination"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses grep syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*parotid.*|.*sub.*mand.*"```
- ```"left_parotid|right_parotid|eyes"```


## DumpVoxelDoseInfo

### Description

This operation locates the minimum and maximum dose voxel values. It is useful
for estimating prescription doses.

### Notes

- This implementation makes use of an older way of estimating dose. Please
  verify it works (or re-write using the new methods) before using for anything
  important.

### Parameters

No registered options.

## EQD2Convert

### Description

This operation performs a BED-based conversion to a dose-equivalent that would
have 2Gy fractions.

### Notes

- This operation requires NumberOfFractions and cannot use DosePerFraction. The
  reasoning is that the DosePerFraction would need to be specified for each
  individual voxel; the prescription DosePerFraction is NOT the same as voxels
  outside the PTV.

### Parameters

- DoseImageSelection
- ImageSelection
- AlphaBetaRatioNormal
- AlphaBetaRatioTumour
- NumberOfFractions
- PrescriptionDose
- NormalizedROILabelRegex
- ROILabelRegex

#### DoseImageSelection

##### Description

Dose images to operate on. Either 'none', 'last', or 'all'.

##### Default

- ```"last"```

##### Examples

- ```"none"```
- ```"last"```
- ```"all"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### AlphaBetaRatioNormal

##### Description

The value to use for alpha/beta in normal (non-cancerous) tissues. Generally a
value of 3.0 Gy is used. Tissues that are sensitive to fractionation may warrant
smaller ratios, such as 1.5-3 Gy for cervical central nervous tissues and
2.3-4.9 for lumbar central nervous tissues (consult table 8.1, page 107 in:
Joiner et al., 'Fractionation: the linear-quadratic approach', 4th Ed., 2009, in
the book 'Basic Clinical Radiobiology', ISBN: 0340929669). Note that the
selected ROIs denote which tissues are diseased. The remaining tissues are
considered to be normal.

##### Default

- ```"3.0"```

##### Examples

- ```"2.0"```
- ```"3.0"```

#### AlphaBetaRatioTumour

##### Description

The value to use for alpha/beta in diseased (tumourous) tissues. Generally a
value of 10.0 is used. Note that the selected ROIs denote which tissues are
diseased. The remaining tissues are considered to be normal.

##### Default

- ```"10.0"```

##### Examples

- ```"10.0"```

#### NumberOfFractions

##### Description

The number of fractions in which a plan was (or will be) delivered. Decimal
fractions are supported to accommodate previous BED conversions.

##### Default

- ```"35"```

##### Examples

- ```"10"```
- ```"20.5"```
- ```"35"```
- ```"40.123"```

#### PrescriptionDose

##### Description

The prescription dose that was (or will be) delivered to the PTV. Note that this
is a theoretical dose since the PTV or CTV will only nominally receive this
dose. Also note that the specified dose need not exist somewhere in the image.
It can be purely theoretical to accommodate previous BED conversions.

##### Default

- ```"70"```

##### Examples

- ```"15"```
- ```"22.5"```
- ```"45.0"```
- ```"66"```
- ```"70.001"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider as bounding tumourous tissues. The
default will match all available ROIs. Be aware that input spaces are trimmed to
a single space. If your ROI name has more than two sequential spaces, use regex
to avoid them. All ROIs have to match the single regex, so use the 'or' token if
needed. Regex is case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*GTV.*"```
- ```"PTV66"```
- ```".*PTV.*|.*GTV.**"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider as bounding tumourous tissues. The
default will match all available ROIs. Be aware that input spaces are trimmed to
a single space. If your ROI name has more than two sequential spaces, use regex
to avoid them. All ROIs have to match the single regex, so use the 'or' token if
needed. Regex is case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*GTV.*"```
- ```"PTV66"```
- ```".*PTV.*|.*GTV.**"```


## EvaluateDoseVolumeHistograms

### Description

This operation evaluates dose-volume histograms for the selected ROI(s).

### Notes

- This routine generates cumulative DVHs with absolute dose on the x-axis and
  fractional volume on the y-axis.

- This routine will naively treat voxels of different size with the same
  weighting rather than weighting each voxel using its spatial extent. This is
  done to improve precision and reduce numerical issues. If necessary, resample
  images to have uniform spatial extent.

- This routine uses image_arrays so convert dose_arrays beforehand.

- This routine will combine spatially-overlapping images by summing voxel
  intensities. It will not combine separate image_arrays though. If needed,
  you'll have to perform a meld on them beforehand.

### Parameters

- OutFileName
- NormalizedROILabelRegex
- ROILabelRegex
- dDose
- UserComment

#### OutFileName

##### Description

A filename (or full path) in which to append the histogram data generated by
this routine. The format is a two-column data file suitable for plotting. A
short header separates entries. Leave empty to dump to generate a unique
temporary file.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.dat"```
- ```"derivative_data.dat"```

#### NormalizedROILabelRegex

##### Description

A regex matching the ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching the ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### dDose

##### Description

The (fixed) bin width, in units of dose.

##### Default

- ```"2.0"```

##### Examples

- ```"0.1"```
- ```"0.5"```
- ```"2.0"```
- ```"5.0"```
- ```"10"```
- ```"50"```

#### UserComment

##### Description

A string that will be inserted into the output file which will simplify merging
output with differing parameters, from different sources, or using
sub-selections of the data. If left empty, the column will be omitted from the
output.

##### Default

- ```""```

##### Examples

- ```"Using XYZ"```
- ```"Patient treatment plan C"```


## EvaluateDoseVolumeStats

### Description

This operation evaluates a variety of Dose-Volume statistics. It is geared
toward PTV ROIs. Currently the following are implemented: (1) Dose Homogeneity
Index: H = (D_{2%} - D_{98%})/D_{median} | over one or more PTVs, where D_{2%}
is the maximum dose that covers 2% of the volume of the PTV, and D_{98%} is the
minimum dose that covers 98% of the volume of the PTV. (2) Conformity Number: C
= V_{T,pres}^{2} / ( V_{T} * V_{pres} ) where V_{T,pres} is the PTV volume
receiving at least 95% of the PTV prescription dose, V_{T} is the volume of the
PTV, and V_{pres} is volume of all (tissue) voxels receiving at least 95% of the
PTV prescription dose.

### Notes

- This routine uses image_arrays so convert dose_arrays beforehand.

- This routine will combine spatially-overlapping images by summing voxel
  intensities. It will not combine separate image_arrays though. If needed,
  you'll have to perform a meld on them beforehand.

### Parameters

- OutFileName
- PTVPrescriptionDose
- PTVNormalizedROILabelRegex
- PTVROILabelRegex
- BodyNormalizedROILabelRegex
- BodyROILabelRegex
- UserComment

#### OutFileName

##### Description

A filename (or full path) in which to append dose statistic data generated by
this routine. The format is CSV. Leave empty to dump to generate a unique
temporary file.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"derivative_data.csv"```

#### PTVPrescriptionDose

##### Description

The dose prescribed to the PTV of interest (in Gy).

##### Default

- ```"70"```

##### Examples

- ```"50"```
- ```"66"```
- ```"70"```
- ```"82.5"```

#### PTVNormalizedROILabelRegex

##### Description

A regex matching PTV ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### PTVROILabelRegex

##### Description

A regex matching PTV ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*PTV.*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### BodyNormalizedROILabelRegex

##### Description

A regex matching body ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### BodyROILabelRegex

##### Description

A regex matching PTV ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*body.*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### UserComment

##### Description

A string that will be inserted into the output file which will simplify merging
output with differing parameters, from different sources, or using
sub-selections of the data. If left empty, the column will be omitted from the
output.

##### Default

- ```""```

##### Examples

- ```""```
- ```"Using XYZ"```
- ```"Patient treatment plan C"```


## EvaluateNTCPModels

### Description

This operation evaluates a variety of NTCP models for each provided ROI. The
selected ROI should be OARs. Currently the following are implemented: (1) The
LKB model. (2) The 'Fenwick' model for solid tumours (in the lung; for a
whole-lung OAR).

### Notes

- Generally these models require dose in 2Gy/fractions equivalents ('EQD2'). You
  must pre-convert the data if the RT plan is not already 2Gy/fraction. There is
  no easy way to ensure this conversion has taken place or was unnecessary.

- This routine uses image_arrays so convert dose_arrays beforehand.

- This routine will combine spatially-overlapping images by summing voxel
  intensities. So if you have a time course it may be more sensible to aggregate
  images in some way (e.g., spatial averaging) prior to calling this routine.

- The LKB and mEUD both have their own gEUD 'alpha' parameter, but they are not
  necessarily shared. Huang et al. 2015 (doi:10.1038/srep18010) used alpha=1 for
  the LKB model and alpha=5 for the mEUD model.

### Parameters

- NTCPFileName
- NormalizedROILabelRegex
- ROILabelRegex
- LKB_TD50
- LKB_M
- LKB_Alpha
- UserComment

#### NTCPFileName

##### Description

A filename (or full path) in which to append NTCP data generated by this
routine. The format is CSV. Leave empty to dump to generate a unique temporary
file.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"derivative_data.csv"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### LKB_TD50

##### Description

The dose (in Gray) needed to deliver to the selected OAR that will induce the
effect in 50% of cases.

##### Default

- ```"26.8"```

##### Examples

- ```"26.8"```

#### LKB_M

##### Description

No description given...

##### Default

- ```"0.45"```

##### Examples

- ```"0.45"```

#### LKB_Alpha

##### Description

The weighting factor $\alpha$ that controls the relative weighting of volume and
dose in the generalized Equivalent Uniform Dose (gEUD) model. When $\alpha=1$,
the gEUD is equivalent to the mean; when $\alpha=0$, the gEUD is equivalent to
the geometric mean. Wu et al. (doi:10.1016/S0360-3016(01)02585-8) claim that for
normal tissues, $\alpha$ can be related to the Lyman-Kutcher-Burman (LKB) model
volume parameter 'n' via $\alpha=1/n$. Sovik et al.
(doi:10.1016/j.ejmp.2007.09.001) found that gEUD is not strongly impacted by
errors in $\alpha$. Niemierko et al. ('A generalized concept of equivalent
uniform dose. Med Phys 26:1100, 1999) generated maximum likelihood estimates for
'several tumors and normal structures' which ranged from -13.1 for local control
of chordoma tumors to +17.7 for perforation of esophagus. Gay et al.
(doi:10.1016/j.ejmp.2007.07.001) table 2 lists estimates based on the work of
Emami (doi:10.1016/0360-3016(91)90171-Y) for normal tissues ranging from 1-31.
Brenner et al. (doi:10.1016/0360-3016(93)90189-3) recommend -7.2 for breast
cancer, -10 for melanoma, and -13 for squamous cell carcinomas. A 2017
presentation by Ontida Apinorasethkul claims the tumour range spans [-40:-1] and
the organs at risk range spans [1:40]. AAPM TG report 166 also provides a
listing of recommended values, suggesting -10 for PTV and GTV, +1 for parotid,
20 for spinal cord, and 8-16 for rectum, bladder, brainstem, chiasm, eye, and
optic nerve. Burman (1991) and QUANTEC (2010) also provide estimates.

##### Default

- ```"1.0"```

##### Examples

- ```"1"```
- ```"3"```
- ```"4"```
- ```"20"```
- ```"31"```

#### UserComment

##### Description

A string that will be inserted into the output file which will simplify merging
output with differing parameters, from different sources, or using
sub-selections of the data. If left empty, the column will be omitted from the
output.

##### Default

- ```""```

##### Examples

- ```""```
- ```"Using XYZ"```
- ```"Patient treatment plan C"```


## EvaluateTCPModels

### Description

This operation evaluates a variety of TCP models for each provided ROI. The
selected ROI should be the GTV (according to the Fenwick model). Currently the
following are implemented: (1) The 'Martel' model. (2) Equivalent Uniform Dose
(EUD) TCP. (3) The 'Fenwick' model for solid tumours.

### Notes

- Generally these models require dose in 2Gy/fractions equivalents ('EQD2'). You
  must pre-convert the data if the RT plan is not already 2Gy/fraction. There is
  no easy way to ensure this conversion has taken place or was unnecessary.

- This routine uses image_arrays so convert dose_arrays beforehand.

- This routine will combine spatially-overlapping images by summing voxel
  intensities. So if you have a time course it may be more sensible to aggregate
  images in some way (e.g., spatial averaging) prior to calling this routine.

- The Fenwick and Martel models share the value of D_{50}. There may be a slight
  difference in some cases. Huang et al. 2015 (doi:10.1038/srep18010) used both
  models and used 84.5 Gy for the Martel model while using 84.6 Gy for the
  Fenwick model. (The paper also reported using a Fenwick 'm' of 0.329 whereas
  the original report by Fenwick reported 0.392, so I don't think this should be
  taken as strong evidence of the equality of D_{50}. However, the difference
  seems relatively insignificant.)

### Parameters

- TCPFileName
- NormalizedROILabelRegex
- ROILabelRegex
- Gamma50
- Dose50
- EUD_Gamma50
- EUD_TCD50
- EUD_Alpha
- Fenwick_C
- Fenwick_M
- Fenwick_Vref
- UserComment

#### TCPFileName

##### Description

A filename (or full path) in which to append TCP data generated by this routine.
The format is CSV. Leave empty to dump to generate a unique temporary file.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"derivative_data.csv"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### Gamma50

##### Description

The unitless 'normalized dose-response gradient' or normalized slope of the
logistic dose-response model at the half-maximum point (e.g., D_50). Informally,
this parameter controls the steepness of the dose-response curve. (For more
specific information, consult a standard reference such as 'Basic Clinical
Radiobiology' 4th Edition by Joiner et al., sections 5.3-5.5.) This parameter is
empirically fit and not universal. Late endpoints for normal tissues have
gamma_50 around 2-6 whereas gamma_50 nominally varies around 1.5-2.5 for local
control of squamous cell carcinomas of the head and neck.

##### Default

- ```"2.3"```

##### Examples

- ```"1.5"```
- ```"2"```
- ```"2.5"```
- ```"6"```

#### Dose50

##### Description

The dose (in Gray) needed to achieve 50% probability of local tumour control
according to an empirical logistic dose-response model (e.g., D_50). Informally,
this parameter 'shifts' the model along the dose axis. (For more specific
information, consult a standard reference such as 'Basic Clinical Radiobiology'
4th Edition by Joiner et al., sections 5.1-5.3.) This parameter is empirically
fit and not universal. In 'Quantifying the position and steepness of radiation
dose-response curves' by Bentzen and Tucker in 1994, D_50 of around 60-65 Gy are
reported for local control of head and neck cancers (pyriform sinus carcinoma
and neck nodes with max diameter <= 3cm). Martel et al. report 84.5 Gy in lung.

##### Default

- ```"65"```

##### Examples

- ```"37.9"```
- ```"52"```
- ```"60"```
- ```"65"```
- ```"84.5"```

#### EUD_Gamma50

##### Description

The unitless 'normalized dose-response gradient' or normalized slope of the gEUD
TCP model. It is defined only for the generalized Equivalent Uniform Dose (gEUD)
model. This is sometimes referred to as the change in TCP for a unit change in
dose straddled at the TCD_50 dose. It is a counterpart to the Martel model's
'Gamma_50' parameter, but is not quite the same. Okunieff et al.
(doi:10.1016/0360-3016(94)00475-Z) computed Gamma50 for tumours in human
subjects across multiple institutions; they found a median of 0.8 for gross
disease and a median of 1.5 for microscopic disease. The inter-quartile range
was [0.7:1.8] and [0.7:2.2] respectively. (Refer to table 3 for site-specific
values.) Additionally, Gay et al. (doi:10.1016/j.ejmp.2007.07.001) claim that a
value of 4.0 for late effects a value of 2.0 for tumors in 'are reasonable
initial estimates in [our] experience.' Their table 2 lists (NTCP) estimates
based on the work of Emami (doi:10.1016/0360-3016(91)90171-Y).

##### Default

- ```"0.8"```

##### Examples

- ```"0.8"```
- ```"1.5"```

#### EUD_TCD50

##### Description

The uniform dose (in Gray) needed to deliver to the tumour to achieve 50%
probability of local control. It is defined only for the generalized Equivalent
Uniform Dose (gEUD) model. It is a counterpart to the Martel model's 'Dose_50'
parameter, but is not quite the same (n.b., TCD_50 is a uniform dose whereas
D_50 is more like a per voxel TCP-weighted mean.) Okunieff et al.
(doi:10.1016/0360-3016(94)00475-Z) computed TCD50 for tumours in human subjects
across multiple institutions; they found a median of 51.9 Gy for gross disease
and a median of 37.9 Gy for microscopic disease. The inter-quartile range was
[38.4:62.8] and [27.0:49.1] respectively. (Refer to table 3 for site-specific
values.) Gay et al. (doi:10.1016/j.ejmp.2007.07.001) table 2 lists (NTCP)
estimates based on the work of Emami (doi:10.1016/0360-3016(91)90171-Y) ranging
from 18-68 Gy.

##### Default

- ```"51.9"```

##### Examples

- ```"51.9"```
- ```"37.9"```

#### EUD_Alpha

##### Description

The weighting factor $\alpha$ that controls the relative weighting of volume and
dose in the generalized Equivalent Uniform Dose (gEUD) model. When $\alpha=1$,
the gEUD is equivalent to the mean; when $\alpha=0$, the gEUD is equivalent to
the geometric mean. Wu et al. (doi:10.1016/S0360-3016(01)02585-8) claim that for
normal tissues, $\alpha$ can be related to the Lyman-Kutcher-Burman (LKB) model
volume parameter 'n' via $\alpha=1/n$. Sovik et al.
(doi:10.1016/j.ejmp.2007.09.001) found that gEUD is not strongly impacted by
error in $\alpha$. Niemierko et al. ('A generalized concept of equivalent
uniform dose. Med Phys 26:1100, 1999) generated maximum likelihood estimates for
'several tumors and normal structures' which ranged from -13.1 for local control
of chordoma tumors to +17.7 for perforation of esophagus. Gay et al.
(doi:10.1016/j.ejmp.2007.07.001) table 2 lists estimates based on the work of
Emami (doi:10.1016/0360-3016(91)90171-Y) for normal tissues ranging from 1-31.
Brenner et al. (doi:10.1016/0360-3016(93)90189-3) recommend -7.2 for breast
cancer, -10 for melanoma, and -13 for squamous cell carcinomas. A 2017
presentation by Ontida Apinorasethkul claims the tumour range spans [-40:-1] and
the organs at risk range spans [1:40]. AAPM TG report 166 also provides a
listing of recommended values, suggesting -10 for PTV and GTV, +1 for parotid,
20 for spinal cord, and 8-16 for rectum, bladder, brainstem, chiasm, eye, and
optic nerve. Burman (1991) and QUANTEC (2010) also provide estimates.

##### Default

- ```"-13.0"```

##### Examples

- ```"-40"```
- ```"-13.0"```
- ```"-10"```
- ```"-7.2"```
- ```"0.3"```
- ```"1"```
- ```"3"```
- ```"4"```
- ```"20"```
- ```"40"```

#### Fenwick_C

##### Description

This parameter describes the degree that superlinear doses are required to
control large tumours. In other words, as tumour volume grows, a
disproportionate amount of additional dose is required to maintain the same
level of control. The Fenwick model is semi-empirical, so this number must be
fitted or used from values reported in the literature. Fenwick et al. 2008
(doi:10.1016/j.clon.2008.12.011) provide values: 9.58 for local progression free
survival at 30 months for NSCLC tumours and 5.00 for head-and-neck tumours.

##### Default

- ```"9.58"```

##### Examples

- ```"9.58"```
- ```"5.00"```

#### Fenwick_M

##### Description

This parameter describes the dose-response steepness in the Fenwick model.
Fenwick et al. 2008 (doi:10.1016/j.clon.2008.12.011) provide values: 0.392 for
local progression free survival at 30 months for NSCLC tumours and 0.280 for
head-and-neck tumours.

##### Default

- ```"0.392"```

##### Examples

- ```"0.392"```
- ```"0.280"```

#### Fenwick_Vref

##### Description

This parameter is the volume (in DICOM units; usually mm^3) of a reference
tumour (i.e., GTV; primary tumour and involved nodes) which the D_{50} are
estimated using. In other words, this is a 'nominal' tumour volume. Fenwick et
al. 2008 (doi:10.1016/j.clon.2008.12.011) recommend 148'410 mm^3 (i.e., a sphere
of diameter 6.6 cm). However, an appropriate value depends on the nature of the
tumour.

##### Default

- ```"148410.0"```

##### Examples

- ```"148410.0"```

#### UserComment

##### Description

A string that will be inserted into the output file which will simplify merging
output with differing parameters, from different sources, or using
sub-selections of the data. If left empty, the column will be omitted from the
output.

##### Default

- ```""```

##### Examples

- ```""```
- ```"Using XYZ"```
- ```"Patient treatment plan C"```


## ExtractRadiomicFeatures

### Description

This operation extracts radiomic features from the selected images. Features are
implemented as per specification in the Image Biomarker Standardisation
Initiative (IBSI) or pyradiomics documentation if the IBSI specification is
unclear or ambiguous.

### Notes

- This routine is meant to be processed by an external analysis.

- If this routine is slow, simplifying ROI contours may help speed
  surface-mesh-based feature extraction. Often removing the highest-frequency
  components of the contour will help, such as edges that conform tightly to
  individual voxels.

### Parameters

- UserComment
- FeaturesFileName
- ImageSelection
- NormalizedROILabelRegex
- ROILabelRegex

#### UserComment

##### Description

A string that will be inserted into the output file which will simplify merging
output with differing parameters, from different sources, or using
sub-selections of the data.

##### Default

- ```""```

##### Examples

- ```""```
- ```"Using XYZ"```
- ```"Patient treatment plan C"```

#### FeaturesFileName

##### Description

Features will be appended to this file. The format is CSV. Leave empty to dump
to generate a unique temporary file. If an existing file is present, rows will
be appended without writing a header.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"derivative_data.csv"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```


## FVPicketFence

### Description

This operation performs a picket fence QA test using an RTIMAGE file.

### Notes

- This is a 'simplified' version of the full picket fence analysis program that
  uses defaults that are expected to be reasonable across a wide range of
  scenarios.

### Parameters

- ImageSelection
- DICOMMargin
- RTIMAGE
- ImageSelection
- RowsL
- RowsH
- ColumnsL
- ColumnsH
- DICOMMargin
- ImageSelection
- MLCModel
- MLCROILabel
- JunctionROILabel
- PeakROILabel
- MinimumJunctionSeparation
- ThresholdDistance
- LeafGapsFileName
- ResultsSummaryFileName
- UserComment
- InteractivePlots
- ScaleFactor
- ImageFileName

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### DICOMMargin

##### Description

The amount of margin (in the DICOM coordinate system) to spare from cropping.

##### Default

- ```"0.0"```

##### Examples

- ```"0.1"```
- ```"2.0"```
- ```"-0.5"```
- ```"20.0"```

#### RTIMAGE

##### Description

If true, attempt to crop the image using information embedded in an RTIMAGE.
This option cannot be used with the other options.

##### Default

- ```"true"```

##### Examples

- ```"true"```
- ```"false"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### RowsL

##### Description

The number of rows to remove, starting with the first row. Can be absolute (px),
percentage (%), or distance in terms of the DICOM coordinate system. Note the
DICOM coordinate system can be flipped, so the first row can be either on the
top or bottom of the image.

##### Default

- ```"5px"```

##### Examples

- ```"0px"```
- ```"10px"```
- ```"100px"```
- ```"15%"```
- ```"15.75%"```
- ```"123.45"```

#### RowsH

##### Description

The number of rows to remove, starting with the last row. Can be absolute (px),
percentage (%), or distance in terms of the DICOM coordinate system. Note the
DICOM coordinate system can be flipped, so the first row can be either on the
top or bottom of the image.

##### Default

- ```"5px"```

##### Examples

- ```"0px"```
- ```"10px"```
- ```"100px"```
- ```"15%"```
- ```"15.75%"```
- ```"123.45"```

#### ColumnsL

##### Description

The number of columns to remove, starting with the first column. Can be absolute
(px), percentage (%), or distance in terms of the DICOM coordinate system. Note
the DICOM coordinate system can be flipped, so the first column can be either on
the top or bottom of the image.

##### Default

- ```"5px"```

##### Examples

- ```"0px"```
- ```"10px"```
- ```"100px"```
- ```"15%"```
- ```"15.75%"```
- ```"123.45"```

#### ColumnsH

##### Description

The number of columns to remove, starting with the last column. Can be absolute
(px), percentage (%), or distance in terms of the DICOM coordinate system. Note
the DICOM coordinate system can be flipped, so the first column can be either on
the top or bottom of the image.

##### Default

- ```"5px"```

##### Examples

- ```"0px"```
- ```"10px"```
- ```"100px"```
- ```"15%"```
- ```"15.75%"```
- ```"123.45"```

#### DICOMMargin

##### Description

The amount of margin (in the DICOM coordinate system) to spare from cropping.

##### Default

- ```"0.0"```

##### Examples

- ```"0.1"```
- ```"2.0"```
- ```"-0.5"```
- ```"20.0"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### MLCModel

##### Description

The MLC design geometry to use. 'VarianMillenniumMLC80' has 80 leafs in each
bank; leaves are 10mm wide at isocentre; and the maximum static field size is
40cm x 40cm. 'VarianMillenniumMLC120' has 120 leafs in each bank; the 40 central
leaves are 5mm wide at isocentre; the 20 peripheral leaves are 10mm wide; and
the maximum static field size is 40cm x 40cm. 'VarianHD120' has 120 leafs in
each bank; the 32 central leaves are 2.5mm wide at isocentre; the 28 peripheral
leaves are 5mm wide; and the maximum static field size is 40cm x 22cm.

##### Default

- ```"VarianMillenniumMLC120"```

##### Examples

- ```"VarianMillenniumMLC80"```
- ```"VarianMillenniumMLC120"```
- ```"VarianHD120"```

#### MLCROILabel

##### Description

An ROI imitating the MLC axes of leaf pairs is created. This is the label to
apply to it. Note that the leaves are modeled with thin contour rectangles of
virtually zero area. Also note that the outline colour is significant and
denotes leaf pair pass/fail.

##### Default

- ```"Leaves"```

##### Examples

- ```"MLC_leaves"```
- ```"MLC"```
- ```"approx_leaf_axes"```

#### JunctionROILabel

##### Description

An ROI imitating the junction is created. This is the label to apply to it. Note
that the junctions are modeled with thin contour rectangles of virtually zero
area.

##### Default

- ```"Junction"```

##### Examples

- ```"Junction"```
- ```"Picket_Fence_Junction"```

#### PeakROILabel

##### Description

ROIs encircling the leaf profile peaks are created. This is the label to apply
to it. Note that the peaks are modeled with small squares.

##### Default

- ```"Peak"```

##### Examples

- ```"Peak"```
- ```"Picket_Fence_Peak"```

#### MinimumJunctionSeparation

##### Description

The minimum distance between junctions on the SAD isoplane in DICOM units (mm).
This number is used to de-duplicate automatically detected junctions. Analysis
results should not be sensitive to the specific value.

##### Default

- ```"10.0"```

##### Examples

- ```"5.0"```
- ```"10.0"```
- ```"15.0"```
- ```"25.0"```

#### ThresholdDistance

##### Description

The threshold distance in DICOM units (mm) above which MLC separations are
considered to 'fail'. Each leaf pair is evaluated separately. Pass/fail status
is also indicated by setting the leaf axis contour colour (blue for pass, red
for fail).

##### Default

- ```"0.5"```

##### Examples

- ```"0.5"```
- ```"1.0"```
- ```"2.0"```

#### LeafGapsFileName

##### Description

This file will contain gap and nominal-vs-actual offset distances for each leaf
pair. The format is CSV. Leave empty to dump to generate a unique temporary
file. If an existing file is present, rows will be appended without writing a
header.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"derivative_data.csv"```

#### ResultsSummaryFileName

##### Description

This file will contain a brief summary of the results. The format is CSV. Leave
empty to dump to generate a unique temporary file. If an existing file is
present, rows will be appended without writing a header.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"derivative_data.csv"```

#### UserComment

##### Description

A string that will be inserted into the output file which will simplify merging
output with differing parameters, from different sources, or using
sub-selections of the data.

##### Default

- ```""```

##### Examples

- ```""```
- ```"Using XYZ"```
- ```"Patient treatment plan C"```

#### InteractivePlots

##### Description

Whether to interactively show plots showing detected edges.

##### Default

- ```"false"```

##### Examples

- ```"true"```
- ```"false"```

#### ScaleFactor

##### Description

This factor is applied to the image width and height to magnify (larger than 1)
or shrink (less than 1) the image. This factor only affects the output image
size. Note that aspect ratio is retained, but rounding for non-integer factors
may lead to small (1-2 pixel) discrepancies.

##### Default

- ```"1.5"```

##### Examples

- ```"0.5"```
- ```"1.0"```
- ```"2.0"```
- ```"5.23"```

#### ImageFileName

##### Description

The file name to use for the image. If blank, a filename will be generated
sequentially.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/an_image.png"```
- ```"afile.png"```


## GenerateSurfaceMask

### Description

This operation generates a surface image mask, which contains information about
whether each voxel is within, on, or outside the selected ROI(s).

### Parameters

- BackgroundVal
- InteriorVal
- SurfaceVal
- NormalizedROILabelRegex
- ROILabelRegex

#### BackgroundVal

##### Description

The value to give to voxels neither inside nor on the surface of the ROI(s).

##### Default

- ```"0.0"```

##### Examples

- ```"0.0"```
- ```"-1.0"```
- ```"1.23"```
- ```"2.34E26"```

#### InteriorVal

##### Description

The value to give to voxels within the volume of the ROI(s) but not on the
surface.

##### Default

- ```"1.0"```

##### Examples

- ```"0.0"```
- ```"-1.0"```
- ```"1.23"```
- ```"2.34E26"```

#### SurfaceVal

##### Description

The value to give to voxels on the surface/boundary of ROI(s).

##### Default

- ```"2.0"```

##### Examples

- ```"0.0"```
- ```"-1.0"```
- ```"1.23"```
- ```"2.34E26"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```


## GenerateVirtualDataContourViaThresholdTestV1

### Description

This operation generates data suitable for testing the ContourViaThreshold
operation.

### Parameters

No registered options.

## GenerateVirtualDataDoseStairsV1

### Description

This operation generates a dosimetric stairway. It can be used for testing how
dosimetric data is transformed.

### Parameters

No registered options.

## GenerateVirtualDataPerfusionV1

### Description

This operation generates data suitable for testing perfusion modeling
operations. There are no specific checks in this code. Another operation
performs the actual validation. You might be able to manually verify if the
perfusion model admits a simple solution.

### Parameters

No registered options.

## GiveWholeImageArrayABoneWindowLevel

### Description

This operation runs the images in an image array through a uniform
window-and-leveler instead of per-slice window-and-level or no window-and-level
at all. Data is modified and no copy is made!

### Parameters

No registered options.

## GiveWholeImageArrayAHeadAndNeckWindowLevel

### Description

This operation runs the images in an image array through a uniform
window-and-leveler instead of per-slice window-and-level or no window-and-level
at all. Data is modified and no copy is made!

### Parameters

No registered options.

## GiveWholeImageArrayAThoraxWindowLevel

### Description

This operation runs the images in an image array through a uniform
window-and-leveler instead of per-slice window-and-level or no window-and-level
at all. Data is modified and no copy is made!

### Parameters

No registered options.

## GiveWholeImageArrayAnAbdominalWindowLevel

### Description

This operation runs the images in an image array through a uniform
window-and-leveler instead of per-slice window-and-level or no window-and-level
at all. Data is modified and no copy is made!

### Parameters

No registered options.

## GridBasedRayCastDoseAccumulate

### Description

This operation performs a ray casting to estimate the surface dose of an ROI.

### Parameters

- DoseMapFileName
- DoseLengthMapFileName
- LengthMapFileName
- NormalizedReferenceROILabelRegex
- NormalizedROILabelRegex
- ReferenceROILabelRegex
- ROILabelRegex
- SmallestFeature
- RaydL
- GridRows
- GridColumns
- SourceDetectorRows
- SourceDetectorColumns
- NumberOfImages

#### DoseMapFileName

##### Description

A filename (or full path) for the dose image map. Note that this file is
approximate, and may not be accurate. There is more information available when
you use the length and dose*length maps instead. However, this file is useful
for viewing and eyeballing tuning settings. The format is FITS. Leave empty to
dump to generate a unique temporary file.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/dose.fits"```
- ```"localfile.fits"```
- ```"derivative_data.fits"```

#### DoseLengthMapFileName

##### Description

A filename (or full path) for the (dose)*(length traveled through the ROI peel)
image map. The format is FITS. Leave empty to dump to generate a unique
temporary file.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/doselength.fits"```
- ```"localfile.fits"```
- ```"derivative_data.fits"```

#### LengthMapFileName

##### Description

A filename (or full path) for the (length traveled through the ROI peel) image
map. The format is FITS. Leave empty to dump to generate a unique temporary
file.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/surfacelength.fits"```
- ```"localfile.fits"```
- ```"derivative_data.fits"```

#### NormalizedReferenceROILabelRegex

##### Description

A regex matching reference ROI labels/names to consider. The default will match
all available ROIs, which is non-sensical. The reference ROI is used to orient
the cleaving plane to trim the grid surface mask.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Prostate.*"```
- ```"Left Kidney"```
- ```"Gross Liver"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ReferenceROILabelRegex

##### Description

A regex matching reference ROI labels/names to consider. The default will match
all available ROIs, which is non-sensical. The reference ROI is used to orient
the cleaving plane to trim the grid surface mask.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*[pP]rostate.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### SmallestFeature

##### Description

A length giving an estimate of the smallest feature you want to resolve.
Quantity is in the DICOM coordinate system.

##### Default

- ```"0.5"```

##### Examples

- ```"1.0"```
- ```"2.0"```
- ```"0.5"```
- ```"5.0"```

#### RaydL

##### Description

The distance to move a ray each iteration. Should be << img_thickness and <<
cylinder_radius. Making too large will invalidate results, causing rays to pass
through the surface without registering any dose accumulation. Making too small
will cause the run-time to grow and may eventually lead to truncation or
round-off errors. Quantity is in the DICOM coordinate system.

##### Default

- ```"0.1"```

##### Examples

- ```"0.1"```
- ```"0.05"```
- ```"0.01"```
- ```"0.005"```

#### GridRows

##### Description

The number of rows in the surface mask grid images.

##### Default

- ```"512"```

##### Examples

- ```"10"```
- ```"50"```
- ```"128"```
- ```"1024"```

#### GridColumns

##### Description

The number of columns in the surface mask grid images.

##### Default

- ```"512"```

##### Examples

- ```"10"```
- ```"50"```
- ```"128"```
- ```"1024"```

#### SourceDetectorRows

##### Description

The number of rows in the resulting images. Setting too fine relative to the
surface mask grid or dose grid is futile.

##### Default

- ```"1024"```

##### Examples

- ```"10"```
- ```"50"```
- ```"128"```
- ```"1024"```

#### SourceDetectorColumns

##### Description

The number of columns in the resulting images. Setting too fine relative to the
surface mask grid or dose grid is futile.

##### Default

- ```"1024"```

##### Examples

- ```"10"```
- ```"50"```
- ```"128"```
- ```"1024"```

#### NumberOfImages

##### Description

The number of images used for grid-based surface detection. Leave negative for
computation of a reasonable value; set to something specific to force an
override.

##### Default

- ```"-1"```

##### Examples

- ```"-1"```
- ```"10"```
- ```"50"```
- ```"100"```


## GroupImages

### Description

This operation will group individual image slices into partitions (Image_Arrays)
based on the values of the specified metadata tags. DICOMautomaton operations
are usually performed on containers rather than individual images, and grouping
can express connections between images. For example a group could contain the
scans belonging to a whole study, one of the series in a study, individual image
volumes within a single series (e.g., a 3D volume in a temporal perfusion scan),
or individual slices. A group could also contain all the slices that intersect a
given plane, or were taken on a specified StationName.

### Notes

- Images are moved, not copied.

- Image order within a group is retained (i.e., stable grouping), but groups are
  appended to the back of the Image_Array list according to the default sort for
  the group's key-value value.

- Images that do not contain the specified metadata will be grouped into a
  special N/A group at the end.

### Parameters

- ImageSelection
- KeysCommon
- Enforce

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"all"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### KeysCommon

##### Description

Image metadata keys to use for exact-match groupings. For each group that is
produced, every image will share the same key-value pair. This is generally
useful for non-numeric (or integer, date, etc.) key-values. A ';'-delimited list
can be specified to group on multiple criteria simultaneously. An empty string
disables metadata-based grouping.

##### Default

- ```""```

##### Examples

- ```"SeriesNumber"```
- ```"BodyPartExamined;StudyDate"```
- ```"SeriesInstanceUID"```
- ```"StationName"```

#### Enforce

##### Description

Other specialized grouping operations that involve custom logic. Currently, only
'no-overlap' is available, but it has two variants. Both partition based on the
spatial extent of images; in each non-overlapping partition, no two images will
spatially overlap. 'No-overlap-as-is' will effectively insert partitions without
altering the order. A partition is inserted when an image is found to overlap
with an image already within the partition. For this grouping to be useful,
images must be sorted so that partitions can be inserted without any necessary
reordering. An example of when this grouping is useful is CT shuttling in which
the ordering of images alternate between increasing and decreasing SliceNumber.
Note that, depending on the ordering, some partitions may therefore be
incomplete. 'No-overlap-adjust' will rearrange images so that the first
partition is always complete. This is achieved by building a queue of
spatially-overlapping images and greedily stealing one of each kind when
constructing partitions. An example of when this grouping is useful are 4DCTs
which have been acquired for all phases while the couch remains at a single
SliceNumber; the images are ordered on disk in the acquisition order (i.e.,
alike SliceNumbers are bunched together) but the logical analysis order is that
each contiguous volume should represent a single phase. An empty string disables
logic-based grouping.

##### Default

- ```""```

##### Examples

- ```"no-overlap-as-is"```
- ```"no-overlap-adjust"```


## GrowContours

### Description

This routine will grow (or shrink) 2D contours in their plane by the specified
amount. Growth is accomplish by translating vertices away from the interior by
the specified amount. The direction is chosen to be the direction opposite of
the in-plane normal produced by averaging the line segments connecting the
contours.

### Parameters

- NormalizedROILabelRegex
- ROILabelRegex
- Distance

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### Distance

##### Description

The distance to translate contour vertices. (The direction is outward.)

##### Default

- ```"0.00354165798657632"```

##### Examples

- ```"1E-5"```
- ```"0.321"```
- ```"1.1"```
- ```"15.3"```


## HighlightROIs

### Description

This operation overwrites voxel data inside and/or outside of ROI(s) to
'highlight' them. It can handle overlapping or duplicate contours.

### Parameters

- Channel
- ImageSelection
- ContourOverlap
- Inclusivity
- ExteriorVal
- InteriorVal
- ExteriorOverwrite
- InteriorOverwrite
- NormalizedROILabelRegex
- ROILabelRegex

#### Channel

##### Description

The image channel to use. Zero-based. Use '-1' to operate on all available
channels.

##### Default

- ```"-1"```

##### Examples

- ```"-1"```
- ```"0"```
- ```"1"```
- ```"2"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### ContourOverlap

##### Description

Controls overlapping contours are treated. The default 'ignore' treats
overlapping contours as a single contour, regardless of contour orientation. The
option 'honour_opposite_orientations' makes overlapping contours with opposite
orientation cancel. Otherwise, orientation is ignored. The latter is useful for
Boolean structures where contour orientation is significant for interior
contours (holes). The option 'overlapping_contours_cancel' ignores orientation
and cancels all contour overlap.

##### Default

- ```"ignore"```

##### Examples

- ```"ignore"```
- ```"honour_opposite_orientations"```
- ```"overlapping_contours_cancel"```
- ```"honour_opps"```
- ```"overlap_cancel"```

#### Inclusivity

##### Description

Controls how voxels are deemed to be 'within' the interior of the selected
ROI(s). The default 'center' considers only the central-most point of each
voxel. There are two corner options that correspond to a 2D projection of the
voxel onto the image plane. The first, 'planar_corner_inclusive', considers a
voxel interior if ANY corner is interior. The second, 'planar_corner_exclusive',
considers a voxel interior if ALL (four) corners are interior.

##### Default

- ```"center"```

##### Examples

- ```"center"```
- ```"centre"```
- ```"planar_corner_inclusive"```
- ```"planar_inc"```
- ```"planar_corner_exclusive"```
- ```"planar_exc"```

#### ExteriorVal

##### Description

The value to give to voxels outside the specified ROI(s). Note that this value
will be ignored if exterior overwrites are disabled.

##### Default

- ```"0.0"```

##### Examples

- ```"0.0"```
- ```"-1.0"```
- ```"1.23"```
- ```"2.34E26"```

#### InteriorVal

##### Description

The value to give to voxels within the volume of the specified ROI(s). Note that
this value will be ignored if interior overwrites are disabled.

##### Default

- ```"1.0"```

##### Examples

- ```"0.0"```
- ```"-1.0"```
- ```"1.23"```
- ```"2.34E26"```

#### ExteriorOverwrite

##### Description

Whether to overwrite voxels exterior to the specified ROI(s).

##### Default

- ```"true"```

##### Examples

- ```"true"```
- ```"false"```

#### InteriorOverwrite

##### Description

Whether to overwrite voxels interior to the specified ROI(s).

##### Default

- ```"true"```

##### Examples

- ```"true"```
- ```"false"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```


## ImageRoutineTests

### Description

This operation performs a series of sub-operations that are generally useful
when inspecting an image.

### Parameters

No registered options.

## LogScale

### Description

This operation log-scales pixels for all available image arrays. This
functionality is often desired for viewing purposes, to make the pixel level
changes appear more linear. Be weary of using for anything quantitative!

### Parameters

- ImageSelection

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```


## MaxMinPixels

### Description

This operation replaces pixels with the pixel-wise difference (max)-(min).

### Parameters

No registered options.

## MeldDose

### Description

This operation melds all available dose image data. At a high level, dose
melding sums overlapping pixel values for multi-part dose arrays. For more
information about what this specifically entails, refer to the appropriate
subroutine.

### Parameters

No registered options.

## MinkowskiSum3D

### Description

This operation computes a Minkowski sum or symmetric difference of a 3D surface
mesh generated from the selected ROIs with a sphere. The effect is that a margin
is added or subtracted to the ROIs, causing them to 'grow' outward or 'shrink'
inward. Exact and inexact routines can be used.

### Parameters

- NormalizedROILabelRegex
- ROILabelRegex
- ImageSelection
- Operation
- Distance

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses grep syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*parotid.*|.*sub.*mand.*"```
- ```"left_parotid|right_parotid|eyes"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax. Note that the selected images
are used to sample the new contours on. Image planes need not match the original
since a full 3D mesh surface is generated.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### Operation

##### Description

The specific operation to perform. Available options are:
'dilate_exact_surface', 'dilate_exact_vertex', 'dilate_inexact_isotropic',
'erode_inexact_isotropic', and 'shell_inexact_isotropic'.

##### Default

- ```"dilate_inexact_isotropic"```

##### Examples

- ```"dilate_exact_surface"```
- ```"dilate_exact_vertex"```
- ```"dilate_inexact_isotropic"```
- ```"erode_inexact_isotropic"```
- ```"shell_inexact_isotropic"```

#### Distance

##### Description

For dilation and erosion operations, this parameter controls the distance the
surface should travel. For shell operations, this parameter controls the
resultant thickness of the shell. In all cases DICOM units are assumed.

##### Default

- ```"1.0"```

##### Examples

- ```"0.5"```
- ```"1.0"```
- ```"2.0"```
- ```"3.0"```
- ```"5.0"```


## ModifyContourMetadata

### Description

This operation injects metadata into contours.

### Parameters

- NormalizedROILabelRegex
- ROILabelRegex
- KeyValues

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### KeyValues

##### Description

Key-value pairs in the form of 'key1@value1;key2@value2' that will be injected
into the selected images. Existing metadata will be overwritten. Both keys and
values are case-sensitive. Note that a semi-colon separates key-value pairs, not
a colon. Note that quotation marks are not stripped internally, but may have to
be provided for the shell to properly interpret the argument.

##### Default

- ```""```

##### Examples

- ```"Description@'some description'"```
- ```"'Description@some description'"```
- ```"MinimumSeparation@1.23"```
- ```"'Description@some description;MinimumSeparation@1.23'"```


## ModifyImageMetadata

### Description

This operation injects metadata into images.

### Parameters

- ImageSelection
- KeyValues

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### KeyValues

##### Description

Key-value pairs in the form of 'key1@value1;key2@value2' that will be injected
into the selected images. Existing metadata will be overwritten. Both keys and
values are case-sensitive. Note that a semi-colon separates key-value pairs, not
a colon. Note that quotation marks are not stripped internally, but may have to
be provided for the shell to properly interpret the argument.

##### Default

- ```""```

##### Examples

- ```"Description@'some description'"```
- ```"'Description@some description'"```
- ```"MinimumSeparation@1.23"```
- ```"'Description@some description;MinimumSeparation@1.23'"```


## NegatePixels

### Description

This operation negates pixels for the selected image arrays. This functionality
is often desired for processing MR images.

### Parameters

- ImageSelection

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```


## OptimizeStaticBeams

### Description

This operation takes dose matrices corresponding to single, static RT beams and
attempts to optimize beam weighting to create an optimal plan subject to various
criteria.

### Notes

- This routine is a simplisitic routine that attempts to estimate the optimal
  beam weighting. It should NOT be used for clinical purposes, except maybe as a
  secondary check or a means to guess reasonable beam weights prior to
  optimization within the clinical TPS.

- Because beam weights are (generally) not specified in DICOM RTDOSE files, the
  beam weights are assumed to all be 1.0. If they are not all 1.0, the weights
  reported here will be relative to whatever the existing weights are.

- This operation uses Image_Arrays, so convert from Dose_Arrays if necessary
  prior to calling this routine. This may be fixed in a future release. Patches
  are welcome.

- If no PTV ROI is available, the BODY contour may suffice. If this is not
  available, dose outside the body should somehow be set to zero to avoid
  confusing D_{max} metrics. For example, bolus D_{max} can be high, but is
  ultimately irrelevant.

- By default, this routine uses all available images. This may be fixed in a
  future release. Patches are welcome.

### Parameters

- ImageSelection
- ResultsSummaryFileName
- UserComment
- NormalizedROILabelRegex
- ROILabelRegex
- MaxVoxelSamples
- NormalizationD
- NormalizationV
- RxDose

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"all"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### ResultsSummaryFileName

##### Description

This file will contain a brief summary of the results. The format is CSV. Leave
empty to dump to generate a unique temporary file. If an existing file is
present, rows will be appended without writing a header.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"derivative_data.csv"```

#### UserComment

##### Description

A string that will be inserted into the output file which will simplify merging
output with differing parameters, from different sources, or using
sub-selections of the data.

##### Default

- ```""```

##### Examples

- ```""```
- ```"Using XYZ"```
- ```"Patient treatment plan C"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### MaxVoxelSamples

##### Description

The maximum number of voxels to randomly sample (deterministically) within the
PTV. Setting lower will result in faster calculation, but lower precision. A
reasonable setting depends on the size of the target structure; small targets
may suffice with a few hundred voxels, but larger targets probably require
several thousand.

##### Default

- ```"1000"```

##### Examples

- ```"200"```
- ```"500"```
- ```"1000"```
- ```"2000"```
- ```"5000"```

#### NormalizationD

##### Description

The isodose value that should envelop a given volume in the PTV ROI. In other
words, this parameter is the 'D' parameter in a DVH constraint of the form
$V_{D} \geq V_{min}$. It should be given as a fraction within [0:1] relative to
the prescription dose. For example, 95% isodose should be provided as '0.95'.

##### Default

- ```"0.95"```

##### Examples

- ```"0.90"```
- ```"0.95"```
- ```"0.98"```
- ```"0.99"```
- ```"1.0"```

#### NormalizationV

##### Description

The minimal fractional volume of ROI that should be enclosed within one or more
surfaces that demarcate the given isodose value. In other words, this parameter
is the 'Vmin' parameter in a DVH constraint of the form $V_{D} \geq V_{min}$. It
should be given as a fraction within [0:1] relative to the volume of the ROI
(typically discretized to the number of voxels in the ROI). For example, if Vmin
= 99%, provide the value '0.99'.

##### Default

- ```"0.99"```

##### Examples

- ```"0.90"```
- ```"0.95"```
- ```"0.98"```
- ```"0.99"```
- ```"1.0"```

#### RxDose

##### Description

The dose prescribed to the ROI that will be optimized. The units depend on the
DICOM file, but will likely be Gy.

##### Default

- ```"70.0"```

##### Examples

- ```"48.0"```
- ```"60.0"```
- ```"63.3"```
- ```"70.0"```
- ```"100.0"```


## OrderImages

### Description

This operation will order individual image slices within collections
(Image_Arrays) based on the values of the specified metadata tags.

### Notes

- Images are moved, not copied.

- Image groupings are retained, and the order of groupings is not altered.

- Images that do not contain the specified metadata will be sorted after the
  end.

### Parameters

- ImageSelection
- Key

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"all"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### Key

##### Description

Image metadata key to use for ordering. Images will be sorted according to the
key's value 'natural' sorting order, which compares sub-strings of numbers and
characters separately. Note this ordering is expected to be stable, but may not
always be on some systems.

##### Default

- ```""```

##### Examples

- ```"AcquisitionTime"```
- ```"ContentTime"```
- ```"SeriesNumber"```
- ```"SeriesDescription"```


## PlotPerROITimeCourses

### Description

Interactively plot time courses for the specified ROI(s).

### Parameters

- ROILabelRegex

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```


## PreFilterEnormousCTValues

### Description

This operation runs the data through a per-pixel filter, censoring pixels which
are too high to legitimately show up in a clinical CT. Censored pixels are set
to NaN. Data is modified and no copy is made!

### Parameters

No registered options.

## PresentationImage

### Description

This operation renders an image with any contours in-place and colour mapping
using an SFML backend.

### Notes

- By default this operation displays the last available image. This makes it
  easier to produce a sequence of images by inserting this operation into a
  sequence of operations.

- Iff there are no images available, this operation will silently convert dose
  arrays to image arrays. If there are images to display, dose arrays must be
  explicitly converted to be visible.

### Parameters

- ScaleFactor
- ImageFileName

#### ScaleFactor

##### Description

This factor is applied to the image width and height to magnify (larger than 1)
or shrink (less than 1) the image. This factor only affects the output image
size. Note that aspect ratio is retained, but rounding for non-integer factors
may lead to small (1-2 pixel) discrepancies.

##### Default

- ```"1.0"```

##### Examples

- ```"0.5"```
- ```"1.0"```
- ```"2.0"```
- ```"5.23"```

#### ImageFileName

##### Description

The file name to use for the image. If blank, a filename will be generated
sequentially.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/an_image.png"```
- ```"afile.png"```


## PruneEmptyImageDoseArrays

### Description

This operation deletes Image_Arrays that do not contain any images.

### Parameters

No registered options.

## PurgeContours

### Description

This routine purges contours if they satisfy various criteria.

### Notes

- This operation considers individual contours only at the moment. It could be
  extended to operate on whole ROIs (i.e., contour_collections), or to perform a
  separate vote within each ROI. The individual contour approach was taken since
  filtering out small contour 'islands' is the primary use-case.

### Parameters

- ROILabelRegex
- NormalizedROILabelRegex
- Invert
- AreaAbove
- AreaBelow
- PerimeterAbove
- PerimeterBelow

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### Invert

##### Description

If false, matching contours will be purged. If true, matching contours will be
retained and non-matching contours will be purged. Enabling this option is
equivalent to a 'Keep if and only if' operation.

##### Default

- ```"false"```

##### Examples

- ```"true"```
- ```"false"```

#### AreaAbove

##### Description

If this option is provided with a valid positive number, contour(s) with an area
greater than the specified value are purged. Note that the DICOM coordinate
space is used. (Supplying the default, inf, will disable this option.)

##### Default

- ```"inf"```

##### Examples

- ```"inf"```
- ```"100.0"```
- ```"1000"```
- ```"10.23E8"```

#### AreaBelow

##### Description

If this option is provided with a valid positive number, contour(s) with an area
less than the specified value are purged. Note that the DICOM coordinate space
is used. (Supplying the default, -inf, will disable this option.)

##### Default

- ```"-inf"```

##### Examples

- ```"-inf"```
- ```"100.0"```
- ```"1000"```
- ```"10.23E8"```

#### PerimeterAbove

##### Description

If this option is provided with a valid positive number, contour(s) with a
perimeter greater than the specified value are purged. Note that the DICOM
coordinate space is used. (Supplying the default, inf, will disable this
option.)

##### Default

- ```"inf"```

##### Examples

- ```"inf"```
- ```"10.0"```
- ```"100"```
- ```"10.23E4"```

#### PerimeterBelow

##### Description

If this option is provided with a valid positive number, contour(s) with a
perimeter less than the specified value are purged. Note that the DICOM
coordinate space is used. (Supplying the default, -inf, will disable this
option.)

##### Default

- ```"-inf"```

##### Examples

- ```"-inf"```
- ```"10.0"```
- ```"100"```
- ```"10.23E4"```


## RankPixels

### Description

This operation ranks pixels throughout an image array.

### Notes

- This routine operates on all images in an image array, so pixel value ranks
  are valid throughout the array. However, the window and level of each window
  is separately determined. You will need to set a uniform window and level
  manually if desired.

- This routine operates on all images in an image array. If images need to be
  processed individually, arrays will have to be exploded prior to calling this
  routine. Note that if this functionality is required, it can be implemented as
  an operation option easily. Likewise, if multiple image arrays must be
  considered simultaneously, they will need to be combined before invoking this
  operation.

### Parameters

- ImageSelection
- Method
- LowerThreshold
- UpperThreshold

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### Method

##### Description

Pixels participating in the ranking process will have their pixel values
replaced. They can be replaced with either a rank or the corresponding
percentile. Ranks start at zero and percentiles are centre-weighted (i.e.,
rank-averaged).

##### Default

- ```"Percentile"```

##### Examples

- ```"Rank"```
- ```"Percentile"```

#### LowerThreshold

##### Description

The (inclusive) threshold above which pixel values must be in order to
participate in the rank.

##### Default

- ```"-inf"```

##### Examples

- ```"-inf"```
- ```"0.0"```
- ```"-900"```

#### UpperThreshold

##### Description

The (inclusive) threshold below which pixel values must be in order to
participate in the rank.

##### Default

- ```"inf"```

##### Examples

- ```"inf"```
- ```"0.0"```
- ```"1500"```


## SFML_Viewer

### Description

Launch an interactive viewer based on SFML. Using this viewer, it is possible to
contour ROIs, generate plots of pixel intensity along profiles or through time,
inspect and compare metadata, and various other things.

### Notes

- If there are no images available, this operation will silently convert dose
  arrays to image arrays. If there are images to display, dose arrays must be
  explicitly converted to be visible.

### Parameters

- SingleScreenshot
- SingleScreenshotFileName

#### SingleScreenshot

##### Description

If 'true', a single screenshot is taken and then the viewer is exited. This
option works best for quick visual inspections, and should not be used for later
processing or analysis.

##### Default

- ```"false"```

##### Examples

- ```"true"```
- ```"false"```

#### SingleScreenshotFileName

##### Description

Iff invoking the 'SingleScreenshot' argument, use this string as the screenshot
filename. If blank, a filename will be generated sequentially.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/a_screenshot.png"```
- ```"afile.png"```


## SeamContours

### Description

This routine converts contours that represent 'outer' and 'inner' via contour
orientation into contours that are uniformly outer but have a zero-area seam
connecting the inner and outer portions.

### Notes

- This routine currently operates on all available ROIs.

- This routine operates on one contour_collection at a time. It will combine
  contours that are in the same contour_collection and overlap, even if they
  have different ROINames. Consider making a complementary routine that
  partitions contours into ROIs based on ROIName (or other metadata) if more
  rigorous enforcement is needed.

- This routine actually computes the XOR Boolean of contours that overlap. So if
  contours partially overlap, this routine will treat the overlapping parts as
  if they are holes, and the non-overlapping parts as if they represent the ROI.
  This behaviour may be surprising in some cases.

- This routine will also treat overlapping contours with like orientation as if
  the smaller contour were a hole of the larger contour.

- This routine will ignore contour orientation if there is only a single
  contour. More specifically, for a given ROI label, planes with a single
  contour will be unaltered.

- Only the common metadata between outer and inner contours is propagated to the
  seamed contours.

- This routine will NOT combine disconnected contours with a seam. Disconnected
  contours will remain disconnected.

### Parameters

No registered options.

## SelectSlicesIntersectingROI

### Description

This operation applies a whitelist to the most-recently loaded images. Images
must 'slice' through one of the described ROIs in order to make the whitelist.
This operation is typically used to reduce long computations by trimming the
field of view of extraneous image slices.

### Parameters

- NormalizedROILabelRegex
- ROILabelRegex

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```


## SimplifyContours

### Description

This operation performs simplification on contours by removing or moving
vertices. This operation is mostly used to reduce the computational complexity
of other operations.

### Notes

- Contours are currently processed individually, not as a volume.

- Simplification is generally performed most eagerly on regions with relatively
  low curvature. Regions of high curvature are generally simplified only as
  necessary.

### Parameters

- NormalizedROILabelRegex
- ROILabelRegex
- FractionalAreaTolerance
- SimplificationMethod

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### FractionalAreaTolerance

##### Description

The fraction of area each contour will tolerate during simplified. This is a
measure of how much the contour area can change due to simplification.

##### Default

- ```"0.01"```

##### Examples

- ```"0.001"```
- ```"0.01"```
- ```"0.02"```
- ```"0.05"```
- ```"0.10"```

#### SimplificationMethod

##### Description

The specific algorithm used to perform contour simplification. 'Vertex removal'
is a simple algorithm that removes vertices one-by-one without replacement. It
iteratively ranks vertices and removes the single vertex that has the least
impact on contour area. It is best suited to removing redundant vertices or
whenever new vertices should not be added. 'Vertex collapse' combines two
adjacent vertices into a single vertex at their midpoint. It iteratively ranks
vertex pairs and removes the single vertex that has the least total impact on
contour area. Note that small sharp features that alternate inward and outward
will have a small total area cost, so will be pruned early. Thus this technique
acts as a low-pass filter and will defer simplification of high-curvature
regions until necessary. It is more economical compared to vertex removal in
that it will usually simplify contours more for a given tolerance (or,
equivalently, can retain contour fidelity better than vertex removal for the
same number of vertices). However, vertex collapse performs an averaging that
may result in numerical imprecision.

##### Default

- ```"vert-collapse"```

##### Examples

- ```"vertex-collapse"```
- ```"vertex-removal"```


## SpatialBlur

### Description

This operation blurs pixels (within the plane of the image only) using the
specified estimator.

### Parameters

- ImageSelection
- Estimator
- GaussianOpenSigma

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"all"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### Estimator

##### Description

Controls the (in-plane) blur estimator to use. Options are currently: box_3x3,
box_5x5, gaussian_3x3, gaussian_5x5, and gaussian_open. The latter
(gaussian_open) is adaptive and requires a supplementary parameter that controls
the number of adjacent pixels to consider. The former ('...3x3' and '...5x5')
are 'fixed' estimators that use a convolution kernel with a fixed size (3x3 or
5x5 pixel neighbourhoods). All estimators operate in 'pixel-space' and are
ignorant about the image spatial extent. All estimators are normalized, and thus
won't significantly affect the pixel magnitude scale.

##### Default

- ```"gaussian_open"```

##### Examples

- ```"box_3x3"```
- ```"box_5x5"```
- ```"gaussian_3x3"```
- ```"gaussian_5x5"```
- ```"gaussian_open"```

#### GaussianOpenSigma

##### Description

Controls the number of neighbours to consider (only) when using the
gaussian_open estimator. The number of pixels is computed automatically to
accommodate the specified sigma (currently ignored pixels have 3*sigma or less
weighting). Be aware this operation can take an enormous amount of time, since
the pixel neighbourhoods quickly grow large.

##### Default

- ```"1.5"```

##### Examples

- ```"0.5"```
- ```"1.0"```
- ```"1.5"```
- ```"2.5"```
- ```"5.0"```


## SpatialDerivative

### Description

This operation estimates various partial derivatives (of pixel values) within an
image.

### Parameters

- ImageSelection
- Estimator
- Method

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### Estimator

##### Description

Controls the finite-difference partial derivative order or estimator used. All
estimators are centred and use mirror boundary conditions. First-order
estimators include the basic nearest-neighbour first derivative, and Roberts'
cross, Prewitt, Sobel, Scharr estimators. 'XxY' denotes the size of the
convolution kernel (i.e., the number of adjacent pixels considered). The only
second-order estimator is the basic nearest-neighbour second derivative.

##### Default

- ```"Scharr-3x3"```

##### Examples

- ```"first"```
- ```"Roberts-cross-3x3"```
- ```"Prewitt-3x3"```
- ```"Sobel-3x3"```
- ```"Sobel-5x5"```
- ```"Scharr-3x3"```
- ```"Scharr-5x5"```
- ```"second"```

#### Method

##### Description

Controls partial derivative method. First-order derivatives can be row- or
column-aligned, Roberts' cross can be (+row,+col)-aligned or
(-row,+col)-aligned. Second-order derivatives can be row-aligned,
column-aligned, or 'cross' --meaning the compound partial derivative. All
methods support non-maximum-suppression for edge thinning, but currently only
the magnitude is output. All methods support magnitude (addition of orthogonal
components in quadrature) and orientation (in radians; [0,2pi) ).

##### Default

- ```"magnitude"```

##### Examples

- ```"row-aligned"```
- ```"column-aligned"```
- ```"prow-pcol-aligned"```
- ```"nrow-pcol-aligned"```
- ```"magnitude"```
- ```"orientation"```
- ```"non-maximum-suppression"```
- ```"cross"```


## SpatialSharpen

### Description

This operation 'sharpens' pixels (within the plane of the image only) using the
specified estimator.

### Parameters

- ImageSelection
- Estimator

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"all"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### Estimator

##### Description

Controls the (in-plane) sharpening estimator to use. Options are currently:
sharpen_3x3 and unsharp_mask_5x5. The latter is based on a 5x5 Gaussian blur
estimator.

##### Default

- ```"unsharp_mask_5x5"```

##### Examples

- ```"sharpen_3x3"```
- ```"unsharp_mask_5x5"```


## Subsegment_ComputeDose_VanLuijk

### Description

This operation sub-segments the selected ROI(s) and computes dose within the
resulting sub-segments.

### Parameters

- AreaDataFileName
- DerivativeDataFileName
- DistributionDataFileName
- NormalizedROILabelRegex
- PlanarOrientation
- ReplaceAllWithSubsegment
- RetainSubsegment
- ROILabelRegex
- SubsegMethod
- XSelection
- YSelection
- ZSelection
- FractionalTolerance
- MaxBisects

#### AreaDataFileName

##### Description

A filename (or full path) in which to append sub-segment areaa data generated by
this routine. The format is CSV. Note that if a sub-segment has zero area or
does not exist, no area will be printed. You'll have to manually add
sub-segments with zero area as needed if this info is relevant to you (e.g., if
you are deriving a population average). Leave empty to NOT dump anything.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"area_data.csv"```

#### DerivativeDataFileName

##### Description

A filename (or full path) in which to append derivative data generated by this
routine. The format is CSV. Leave empty to dump to generate a unique temporary
file.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"derivative_data.csv"```

#### DistributionDataFileName

##### Description

A filename (or full path) in which to append raw distribution data generated by
this routine. The format is one line of description followed by one line for the
distribution; pixel intensities are listed with a single space between elements;
the descriptions contain the patient ID, ROIName, and subsegment description
(guaranteed) and possibly various other data afterward. Leave empty to NOT dump
anything.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/somefile"```
- ```"localfile.csv"```
- ```"distributions.data"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### PlanarOrientation

##### Description

A string instructing how to orient the cleaving planes. Currently only
'AxisAligned' (i.e., align with the image/dose grid row and column unit vectors)
and 'StaticOblique' (i.e., same as AxisAligned but rotated 22.5 degrees to
reduce colinearity, which sometimes improves sub-segment area consistency).

##### Default

- ```"AxisAligned"```

##### Examples

- ```"AxisAligned"```
- ```"StaticOblique"```

#### ReplaceAllWithSubsegment

##### Description

Keep the sub-segment and remove any existing contours from the original ROIs.
This is most useful for further processing, such as nested sub-segmentation.
Note that sub-segment contours currently have identical metadata to their parent
contours.

##### Default

- ```"false"```

##### Examples

- ```"true"```
- ```"false"```

#### RetainSubsegment

##### Description

Keep the sub-segment as part of the original ROIs. The contours are appended to
the original ROIs, but the contour ROIName and NormalizedROIName are set to the
argument provided. (If no argument is provided, sub-segments are not retained.)
This is most useful for inspection of sub-segments. Note that sub-segment
contours currently have identical metadata to their parent contours, except they
are renamed accordingly.

##### Default

- ```""```

##### Examples

- ```"subsegment_01"```
- ```"subsegment_02"```
- ```"selected_subsegment"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### SubsegMethod

##### Description

The method to use for sub-segmentation. Nested sub-segmentation should almost
always be preferred unless you know what you're doing. It should be faster too.
The compound method was used in the van Luijk paper, but it is known to have
serious problems.

##### Default

- ```"nested"```

##### Examples

- ```"nested"```
- ```"compound"```

#### XSelection

##### Description

(See ZSelection description.) The "X" direction is defined in terms of movement
on an image when the row number increases. This is generally VERTICAL and
DOWNWARD. All selections are defined in terms of the original ROIs.

##### Default

- ```"1.0;0.0"```

##### Examples

- ```"0.50;0.50"```
- ```"0.50;0.0"```
- ```"0.30;0.0"```
- ```"0.30;0.70"```

#### YSelection

##### Description

(See ZSelection description.) The "Y" direction is defined in terms of movement
on an image when the column number increases. This is generally HORIZONTAL and
RIGHTWARD. All selections are defined in terms of the original ROIs.

##### Default

- ```"1.0;0.0"```

##### Examples

- ```"0.50;0.50"```
- ```"0.50;0.0"```
- ```"0.30;0.0"```
- ```"0.30;0.70"```

#### ZSelection

##### Description

The thickness and offset defining the single, continuous extent of the
sub-segmentation in terms of the fractional area remaining above a plane. The
planes define the portion extracted and are determined such that
sub-segmentation will give the desired fractional planar areas. The numbers
specify the thickness and offset from the bottom of the ROI volume to the bottom
of the extent. The 'upper' direction is take from the contour plane orientation
and assumed to be positive if pointing toward the positive-z direction. Only a
single 3D selection can be made per operation invocation. Sub-segmentation can
be performed in transverse ("Z"), row_unit ("X"), and column_unit ("Y")
directions (in that order). All selections are defined in terms of the original
ROIs. Note that it is possible to perform nested sub-segmentation (including
passing along the original contours) by opting to replace the original ROI
contours with this sub-segmentation and invoking this operation again with the
desired sub-segmentation. If you want the middle 50% of an ROI, specify
'0.50;0.25'. If you want the upper 50% then specify '0.50;0.50'. If you want the
lower 50% then specify '0.50;0.0'. If you want the upper 30% then specify
'0.30;0.70'. If you want the lower 30% then specify '0.30;0.70'.

##### Default

- ```"1.0;0.0"```

##### Examples

- ```"0.50;0.50"```
- ```"0.50;0.0"```
- ```"0.30;0.0"```
- ```"0.30;0.70"```

#### FractionalTolerance

##### Description

The tolerance of X, Y, and Z fractional area bisection criteria (see ZSelection
description). This parameter specifies a stopping condition for the bisection
procedure. If it is set too high, sub-segments may be inadequatly rough. If it
is set too low, bisection below the machine precision floor may be attempted,
which will result in instabilities. Note that the number of permitted iterations
will control whether this tolerance can possibly be reached; if strict adherence
is required, set the maximum number of iterations to be excessively large.

##### Default

- ```"0.001"```

##### Examples

- ```"1E-2"```
- ```"1E-3"```
- ```"1E-4"```
- ```"1E-5"```

#### MaxBisects

##### Description

The maximum number of iterations the bisection procedure can perform. This
parameter specifies a stopping condition for the bisection procedure. If it is
set too low, sub-segments may be inadequatly rough. If it is set too high,
bisection below the machine precision floor may be attempted, which will result
in instabilities. Note that the fractional tolerance will control whether this
tolerance can possibly be reached; if an exact number of iterations is required,
set the fractional tolerance to be excessively small.

##### Default

- ```"20"```

##### Examples

- ```"10"```
- ```"20"```
- ```"30"```


## SubtractImages

### Description

This routine subtracts images that spatially overlap.

### Notes

- This operation currently performs a subtraction necessarily using the first
  image volume.

- This operation is currently extremely limited in that the selected images must
  be selected by position. A more flexible approach will be eventually be
  implemented when the image selection mechanism is overhauled.

### Parameters

- ImageSelection

#### ImageSelection

##### Description

Images to operate on. Either 'none', 'last', 'first', or 'all'.

##### Default

- ```"all"```

##### Examples

- ```"none"```
- ```"last"```
- ```"first"```
- ```"all"```


## SupersampleImageGrid

### Description

This operation scales supersamples images so they have more rows and/or columns,
but the whole image keeps its shape and spatial extent. This operation is
typically used for zooming into images or trying to ensure a sufficient number
of voxels are within small contours.

### Notes

- Be aware that specifying large multipliers (or even small multipliers on large
  images) will consume much memory. It is best to pre-crop images to a region of
  interest if possible.

### Parameters

- ColumnScaleFactor
- DoseImageSelection
- ImageSelection
- RowScaleFactor
- SamplingMethod

#### ColumnScaleFactor

##### Description

A positive integer specifying how many columns will be in the new images. The
number is relative to the incoming image column count. Specifying '1' will
result in nothing happening. Specifying '8' will result in 8x as many columns.

##### Default

- ```"2"```

##### Examples

- ```"1"```
- ```"2"```
- ```"3"```
- ```"8"```

#### DoseImageSelection

##### Description

Dose images to operate on. Either 'none', 'last', or 'all'.

##### Default

- ```"none"```

##### Examples

- ```"none"```
- ```"last"```
- ```"all"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### RowScaleFactor

##### Description

A positive integer specifying how many rows will be in the new images. The
number is relative to the incoming image row count. Specifying '1' will result
in nothing happening. Specifying '8' will result in 8x as many rows.

##### Default

- ```"2"```

##### Examples

- ```"1"```
- ```"2"```
- ```"3"```
- ```"8"```

#### SamplingMethod

##### Description

The supersampling method to use. Note: 'inplane-' methods only consider
neighbours in the plane of a single image -- neighbours in adjacent images are
not considered.

##### Default

- ```"inplane-bicubic"```

##### Examples

- ```"inplane-bicubic"```
- ```"inplane-bilinear"```


## SurfaceBasedRayCastDoseAccumulate

### Description

This routine uses rays (actually: line segments) to estimate point-dose on the
surface of an ROI. The ROI is approximated by surface mesh and rays are passed
through. Dose is interpolated at the intersection points and intersecting lines
(i.e., where the ray 'glances' the surface) are discarded. The surface
reconstruction can be tweaked, but appear to reasonably approximate the ROI
contours; both can be output to compare visually. Though it is not required by
the implementation, only the ray-surface intersection nearest to the detector is
considered. All other intersections (i.e., on the far side of the surface mesh)
are ignored. This routine is fairly fast compared to the slow grid-based
counterpart previously implemented. The speedup comes from use of an AABB-tree
to accelerate intersection queries and avoid having to 'walk' rays step-by-step
through over/through the geometry.

### Parameters

- TotalDoseMapFileName
- IntersectionCountMapFileName
- DepthMapFileName
- RadialDistMapFileName
- ROISurfaceMeshFileName
- SubdividedROISurfaceMeshFileName
- ROICOMCOMLineFileName
- NormalizedReferenceROILabelRegex
- NormalizedROILabelRegex
- ReferenceROILabelRegex
- ROILabelRegex
- SourceDetectorRows
- SourceDetectorColumns
- MeshingAngularBound
- MeshingFacetSphereRadiusBound
- MeshingCentreCentreBound
- MeshingSubdivisionIterations
- MaxRaySurfaceIntersections
- OnlyGenerateSurface

#### TotalDoseMapFileName

##### Description

A filename (or full path) for the total dose image map (at all ray-surface
intersection points). The dose for each ray is summed over all ray-surface point
intersections. The format is FITS. This file is always generated. Leave the
argument empty to generate a unique filename.

##### Default

- ```""```

##### Examples

- ```""```
- ```"total_dose_map.fits"```
- ```"/tmp/out.fits"```

#### IntersectionCountMapFileName

##### Description

A filename (or full path) for the (number of ray-surface intersections) image
map. Each pixel in this map (and the total dose map) represents a single ray;
the number of times the ray intersects the surface can be useful for various
purposes, but most often it will simply be a sanity check for the
cross-sectional shape or that a specific number of intersections were recorded
in regions with geometrical folds. Pixels will all be within
[0,MaxRaySurfaceIntersections]. The format is FITS. Leave empty to dump to
generate a unique filename.

##### Default

- ```""```

##### Examples

- ```""```
- ```"intersection_count_map.fits"```
- ```"/tmp/out.fits"```

#### DepthMapFileName

##### Description

A filename (or full path) for the distance (depth) of each ray-surface
intersection point from the detector. Has DICOM coordinate system units. This
image is potentially multi-channel with [MaxRaySurfaceIntersections] channels
(when MaxRaySurfaceIntersections = 1 there is 1 channel). The format is FITS.
Leaving empty will result in no file being written.

##### Default

- ```""```

##### Examples

- ```""```
- ```"depth_map.fits"```
- ```"/tmp/out.fits"```

#### RadialDistMapFileName

##### Description

A filename (or full path) for the distance of each ray-surface intersection
point from the line joining reference and target ROI centre-of-masses. This
helps quantify position in 3D. Has DICOM coordinate system units. This image is
potentially multi-channel with [MaxRaySurfaceIntersections] channels (when
MaxRaySurfaceIntersections = 1 there is 1 channel). The format is FITS. Leaving
empty will result in no file being written.

##### Default

- ```""```

##### Examples

- ```""```
- ```"radial_dist_map.fits"```
- ```"/tmp/out.fits"```

#### ROISurfaceMeshFileName

##### Description

A filename (or full path) for the (pre-subdivided) surface mesh that is
contructed from the ROI contours. The format is OFF. This file is mostly useful
for inspection of the surface or comparison with contours. Leaving empty will
result in no file being written.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/roi_surface_mesh.off"```
- ```"roi_surface_mesh.off"```

#### SubdividedROISurfaceMeshFileName

##### Description

A filename (or full path) for the Loop-subdivided surface mesh that is
contructed from the ROI contours. The format is OFF. This file is mostly useful
for inspection of the surface or comparison with contours. Leaving empty will
result in no file being written.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/subdivided_roi_surface_mesh.off"```
- ```"subdivided_roi_surface_mesh.off"```

#### ROICOMCOMLineFileName

##### Description

A filename (or full path) for the line segment that connected the centre-of-mass
(COM) of reference and target ROI. The format is OFF. This file is mostly useful
for inspection of the surface or comparison with contours. Leaving empty will
result in no file being written.

##### Default

- ```""```

##### Examples

- ```""```
- ```"/tmp/roi_com_com_line.off"```
- ```"roi_com_com_line.off"```

#### NormalizedReferenceROILabelRegex

##### Description

A regex matching reference ROI labels/names to consider. The default will match
all available ROIs, which is non-sensical. The reference ROI is used to orient
the cleaving plane to trim the grid surface mask.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Prostate.*"```
- ```"Left Kidney"```
- ```"Gross Liver"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ReferenceROILabelRegex

##### Description

A regex matching reference ROI labels/names to consider. The default will match
all available ROIs, which is non-sensical. The reference ROI is used to orient
the cleaving plane to trim the grid surface mask.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*[pP]rostate.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### SourceDetectorRows

##### Description

The number of rows in the resulting images, which also defines how many rays are
used. (Each pixel in the source image represents a single ray.) Setting too fine
relative to the surface mask grid or dose grid is futile.

##### Default

- ```"1024"```

##### Examples

- ```"100"```
- ```"128"```
- ```"1024"```
- ```"4096"```

#### SourceDetectorColumns

##### Description

The number of columns in the resulting images. (Each pixel in the source image
represents a single ray.) Setting too fine relative to the surface mask grid or
dose grid is futile.

##### Default

- ```"1024"```

##### Examples

- ```"100"```
- ```"128"```
- ```"1024"```
- ```"4096"```

#### MeshingAngularBound

##### Description

The minimum internal angle each triangle facet must have in the surface mesh (in
degrees). The computation may become unstable if an angle larger than 30 degree
is specified. Note that for intersection purposes triangles with small angles
isn't a big deal. Rather, having a large minimal angle can constrain the surface
in strange ways. Consult the CGAL '3D Surface Mesh Generation' package
documentation for additional info.

##### Default

- ```"1.0"```

##### Examples

- ```"1.0"```
- ```"10.0"```
- ```"25.0"```
- ```"30.0"```

#### MeshingFacetSphereRadiusBound

##### Description

The maximum radius of facet-bounding spheres, which are centred on each facet
(one per facet) and grown as large as possible without enclosing any facet
vertices. In a nutshell, this controls the maximum individual facet size. Units
are in DICOM space. Setting too low will cause triangulation to be slow and many
facets; it is recommended instead to rely on subdivision to create a smooth
surface approximation. Consult the CGAL '3D Surface Mesh Generation' package
documentation for additional info.

##### Default

- ```"5.0"```

##### Examples

- ```"1.0"```
- ```"2.0"```
- ```"5.0"```

#### MeshingCentreCentreBound

##### Description

The maximum facet centre-centre distance between facet circumcentres and
facet-bounding spheres, which are centred on each facet (one per facet) and
grown as large as possible without enclosing any facet vertices. In a nutshell,
this controls the trade-off between minimizing deviation from the (implicit) ROI
contour-derived surface and having smooth connectivity between facets. Units are
in DICOM space. Setting too low will cause triangulation to be slow and many
facets; it is recommended instead to rely on subdivision to create a smooth
surface approximation. Consult the CGAL '3D Surface Mesh Generation' package
documentation for additional info.

##### Default

- ```"5.0"```

##### Examples

- ```"1.0"```
- ```"2.0"```
- ```"5.0"```
- ```"10.0"```

#### MeshingSubdivisionIterations

##### Description

The number of iterations of Loop's subdivision to apply to the surface mesh. The
aim of subdivision in this context is to have a smooth surface to work with, but
too many applications will create too many facets. More facets will not lead to
more precise results beyond a certain (modest) amount of smoothing. If the
geometry is relatively spherical already, and meshing bounds produce reasonably
smooth (but 'blocky') surface meshes, then 2-3 iterations should suffice. More
than 3-4 iterations will almost always be inappropriate.

##### Default

- ```"2"```

##### Examples

- ```"0"```
- ```"1"```
- ```"2"```
- ```"3"```

#### MaxRaySurfaceIntersections

##### Description

The maximum number of ray-surface intersections to accumulate before retiring
each ray. Note that intersections are sorted spatially by their distance to the
detector, and those closest to the detector are considered first. If the ROI
surface is opaque, setting this value to 1 will emulate visibility. Setting to 2
will permit rays continue through the ROI and pass through the other side; dose
will be the accumulation of dose at each ray-surface intersection. This value
should most often be 1 or some very high number (e.g., 1000) to make the surface
either completely opaque or completely transparent. (A transparent surface may
help to visualize geometrical 'folds' or other surface details of interest.)

##### Default

- ```"1"```

##### Examples

- ```"1"```
- ```"4"```
- ```"1000"```

#### OnlyGenerateSurface

##### Description

Stop processing after writing the surface and subdivided surface meshes. This
option is primarily used for debugging and visualization.

##### Default

- ```"false"```

##### Examples

- ```"true"```
- ```"false"```


## ThresholdImages

### Description

This operation applies thresholds to images.

### Notes

- This routine operates on individual images. When thresholds are specified on a
  percentile basis, each image is considered separately and therefore each image
  may be thresholded with different values.

### Parameters

- Lower
- Low
- Upper
- High
- Channel
- ImageSelection

#### Lower

##### Description

The lower bound (inclusive). Pixels with values < this number are replaced with
the 'low' value. If this number is followed by a '%', the bound will be scaled
between the min and max pixel values [0-100%]. If this number is followed by
'tile', the bound will be replaced with the corresponding percentile
[0-100tile]. Note that upper and lower bounds can be specified separately (e.g.,
lower bound is a percentage, but upper bound is a percentile).

##### Default

- ```"-inf"```

##### Examples

- ```"0.0"```
- ```"-1E-99"```
- ```"1.23"```
- ```"0.2%"```
- ```"23tile"```
- ```"23.123 tile"```

#### Low

##### Description

The value a pixel will take when below the lower threshold.

##### Default

- ```"-inf"```

##### Examples

- ```"0.0"```
- ```"-1000.0"```
- ```"-inf"```
- ```"nan"```

#### Upper

##### Description

The upper bound (inclusive). Pixels with values > this number are replaced with
the 'high' value. If this number is followed by a '%', the bound will be scaled
between the min and max pixel values [0-100%]. If this number is followed by
'tile', the bound will be replaced with the corresponding percentile
[0-100tile]. Note that upper and lower bounds can be specified separately (e.g.,
lower bound is a percentage, but upper bound is a percentile).

##### Default

- ```"inf"```

##### Examples

- ```"1.0"```
- ```"1E-99"```
- ```"2.34"```
- ```"98.12%"```
- ```"94tile"```
- ```"94.123 tile"```

#### High

##### Description

The value a pixel will take when above the upper threshold.

##### Default

- ```"inf"```

##### Examples

- ```"0.0"```
- ```"1000.0"```
- ```"inf"```
- ```"nan"```

#### Channel

##### Description

The image channel to use. Zero-based.

##### Default

- ```"0"```

##### Examples

- ```"0"```
- ```"1"```
- ```"2"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"last"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```


## TrimROIDose

### Description

This operation provides a simplified interface for overriding the dose within a
ROI. For example, this operation can be used to modify a base plan by
eliminating dose that coincides with a PTV/CTV/GTV/ROI etc.

### Notes

- This operation performs the opposite of the 'Crop' operation, which trims the
  dose outside a ROI.

- The inclusivity of a dose voxel that straddles the ROI boundary can be
  specified in various ways. Refer to the Inclusivity parameter documentation.

- By default this operation only overrides dose within a ROI. The opposite,
  overriding dose outside of a ROI, can be accomplished using the expert
  interface.

### Parameters

- Channel
- ImageSelection
- ContourOverlap
- Inclusivity
- ExteriorVal
- InteriorVal
- ExteriorOverwrite
- InteriorOverwrite
- NormalizedROILabelRegex
- ROILabelRegex
- Filename
- ParanoiaLevel

#### Channel

##### Description

The image channel to use. Zero-based. Use '-1' to operate on all available
channels.

##### Default

- ```"-1"```

##### Examples

- ```"-1"```
- ```"0"```
- ```"1"```
- ```"2"```

#### ImageSelection

##### Description

Select image arrays to operate on. Specifiers can be of two types: positional or
metadata-based key@value regex. Positional specifiers can be 'first', 'last',
'none', or 'all' literals. Additionally '#N' for some positive integer N selects
the Nth image array (with zero-based indexing). Likewise, '#-N' selects the
Nth-from-last image array. Positional specifiers can be inverted by prefixing
with a '!'. Metadata-based key@value expressions are applied by matching the
keys verbatim and the values with regex. In order to invert metadata-based
selectors, the regex logic must be inverted (i.e., you can *not* prefix
metadata-based selectors with a '!'). Multiple criteria can be specified by
separating them with a ';' and are applied in the order specified. Both
positional and metadata-based criteria can be mixed together. Note that image
arrays can hold anything, but will typically represent a single contiguous 3D
volume (i.e., a volumetric CT scan) or '4D' time-series. Note regexes are case
insensitive and should use extended POSIX syntax.

##### Default

- ```"all"```

##### Examples

- ```"last"```
- ```"first"```
- ```"all"```
- ```"none"```
- ```"#0"```
- ```"#-0"```
- ```"!last"```
- ```"!#-3"```
- ```"key@.*value.*"```
- ```"key1@.*value1.*;key2@^value2$;first"```

#### ContourOverlap

##### Description

Controls overlapping contours are treated. The default 'ignore' treats
overlapping contours as a single contour, regardless of contour orientation. The
option 'honour_opposite_orientations' makes overlapping contours with opposite
orientation cancel. Otherwise, orientation is ignored. The latter is useful for
Boolean structures where contour orientation is significant for interior
contours (holes). The option 'overlapping_contours_cancel' ignores orientation
and cancels all contour overlap.

##### Default

- ```"ignore"```

##### Examples

- ```"ignore"```
- ```"honour_opposite_orientations"```
- ```"overlapping_contours_cancel"```
- ```"honour_opps"```
- ```"overlap_cancel"```

#### Inclusivity

##### Description

Controls how voxels are deemed to be 'within' the interior of the selected
ROI(s). The default 'center' considers only the central-most point of each
voxel. There are two corner options that correspond to a 2D projection of the
voxel onto the image plane. The first, 'planar_corner_inclusive', considers a
voxel interior if ANY corner is interior. The second, 'planar_corner_exclusive',
considers a voxel interior if ALL (four) corners are interior.

##### Default

- ```"planar_inc"```

##### Examples

- ```"center"```
- ```"centre"```
- ```"planar_corner_inclusive"```
- ```"planar_inc"```
- ```"planar_corner_exclusive"```
- ```"planar_exc"```

#### ExteriorVal

##### Description

The value to give to voxels outside the specified ROI(s). Note that this value
will be ignored if exterior overwrites are disabled.

##### Default

- ```"0.0"```

##### Examples

- ```"0.0"```
- ```"-1.0"```
- ```"1.23"```
- ```"2.34E26"```

#### InteriorVal

##### Description

The value to give to voxels within the volume of the specified ROI(s). Note that
this value will be ignored if interior overwrites are disabled.

##### Default

- ```"0.0"```

##### Examples

- ```"0.0"```
- ```"-1.0"```
- ```"1.23"```
- ```"2.34E26"```

#### ExteriorOverwrite

##### Description

Whether to overwrite voxels exterior to the specified ROI(s).

##### Default

- ```"false"```

##### Examples

- ```"true"```
- ```"false"```

#### InteriorOverwrite

##### Description

Whether to overwrite voxels interior to the specified ROI(s).

##### Default

- ```"true"```

##### Examples

- ```"true"```
- ```"false"```

#### NormalizedROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*Body.*"```
- ```"Body"```
- ```"Gross_Liver"```
- ```".*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*"```
- ```"Left Parotid|Right Parotid"```

#### ROILabelRegex

##### Description

A regex matching ROI labels/names to consider. The default will match all
available ROIs. Be aware that input spaces are trimmed to a single space. If
your ROI name has more than two sequential spaces, use regex to avoid them. All
ROIs have to match the single regex, so use the 'or' token if needed. Regex is
case insensitive and uses extended POSIX syntax.

##### Default

- ```".*"```

##### Examples

- ```".*"```
- ```".*body.*"```
- ```"body"```
- ```"Gross_Liver"```
- ```".*left.*parotid.*|.*right.*parotid.*|.*eyes.*"```
- ```"left_parotid|right_parotid"```

#### Filename

##### Description

The filename (or full path name) to which the DICOM file should be written.

##### Default

- ```"/tmp/RD.dcm"```

##### Examples

- ```"/tmp/RD.dcm"```
- ```"./RD.dcm"```
- ```"RD.dcm"```

#### ParanoiaLevel

##### Description

At low paranoia setting, only top-level UIDs are replaced. At medium paranoia
setting, many UIDs, descriptions, and labels are replaced, but the PatientID and
FrameOfReferenceUID are retained. The high paranoia setting is the same as the
medium setting, but the PatientID and FrameOfReferenceUID are also replaced.
(Note: this is not a full anonymization.) Use the low setting if you want to
retain linkage to the originating data set. Use the medium setting if you don't.
Use the high setting if your TPS goes overboard linking data sets by PatientID
and/or FrameOfReferenceUID.

##### Default

- ```"medium"```

##### Examples

- ```"low"```
- ```"medium"```
- ```"high"```


## UBC3TMRI_DCE

### Description

This operation is used to generate dynamic contrast-enhanced MRI contrast
enhancement maps.

### Parameters

No registered options.

## UBC3TMRI_DCE_Differences

### Description

This operation is used to generate dynamic contrast-enhanced MRI contrast
enhancement maps.

### Notes

- This routine generates difference maps using both long DCE scans. Thus it
  takes up a LOT of memory! Try avoid unnecessary copies of large (temporally
  long) arrays.

### Parameters

No registered options.

## UBC3TMRI_DCE_Experimental

### Description

This operation is an experimental operation for processing dynamic
contrast-enhanced MR images.

### Parameters

No registered options.

## UBC3TMRI_IVIM_ADC

### Description

This operation is an experimental operation for processing IVIM MR images into
ADC maps.

### Parameters

No registered options.

# Known Issues and Limitations

## Hanging on Debian

The SFML_Viewer operation hangs on some systems after viewing a plot with
Gnuplot. This stems from a known issue in Ygor.

## Build Requirements

DICOMautomaton depends on several heavily templated libraries and external
projects. It requires a considerable amount of memory to build.

## DICOM-RT Support Incomplete

Support for the DICOM Radiotherapy extensions are limited. In particular, only
RTDOSE files can currently be exported, and RTPLAN files are not supported at
all. Read support for DICOM image modalities and RTSTRUCTS are generally
supported well. Broader DICOM support is planned for a future release.

