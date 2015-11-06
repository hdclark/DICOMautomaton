/*

Imebra 2011 build 2012-12-19_20-05-13

Imebra: a C++ Dicom library

Copyright (c) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 by Paolo Brandoli
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the authors
 and should not be interpreted as representing official policies, either expressed or implied,
 of the Imebra Project.

Imebra is available at http://imebra.com



*/

/*! \file dicomDict.cpp
    \brief Implementation of the class dicomDict.

*/


#include "../../base/include/exception.h"
#include "../include/dicomDict.h"

namespace puntoexe
{

namespace imebra
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// dicomDictionary
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Constructor. Register all the known tags and VRs
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
dicomDictionary::dicomDictionary()
{
	PUNTOEXE_FUNCTION_START(L"dicomDictionary::dicomDictionary");

	registerVR("AE", false, 0, 16);
	registerVR("AS", false, 0, 0);
	registerVR("AT", false, 2, 0);
	registerVR("CS", false, 0, 16);
	registerVR("DA", false, 0, 0);
	registerVR("DS", false, 0, 16);
	registerVR("DT", false, 0, 26);
	registerVR("FL", false, 0, 0);
	registerVR("FD", false, 0, 0);
	registerVR("IS", false, 0, 12);
	registerVR("LO", false, 0, 64);
	registerVR("LT", false, 0, 10240);
	registerVR("OB", true,  0, 0);
	registerVR("OF", true,  4, 0);
	registerVR("OW", true,  2, 0);
	registerVR("PN", false, 0, 64);
	registerVR("SH", false, 0, 16);
	registerVR("SL", false, 4, 0);
	registerVR("SQ", true,  0, 0);
	registerVR("SS", false, 2, 0);
	registerVR("ST", false, 0, 1024);
	registerVR("TM", false, 0, 16);
	registerVR("UI", false, 0, 64);
	registerVR("UL", false, 4, 0);
	registerVR("UN", true,  0, 0);
	registerVR("US", false, 2, 0);
	registerVR("UT", true, 0, 0);
	registerVR("IS", false, 0, 0);
	
	registerTag(0x00020000, L"File meta elements","UL");
	registerTag(0x00020001, L"File Meta Information Version","OB");
	registerTag(0x00020002, L"Media Storage SOP Class","UI");
	registerTag(0x00020003, L"Media Storage SOP Instance","UI");
	registerTag(0x00020010, L"Transfer Syntax","UI");
	registerTag(0x00020012, L"Implementation Class","UI");
	registerTag(0x00020013, L"Implementation Version Name","SH");
	registerTag(0x00020016, L"Source Application Entity Title","AE");
	registerTag(0x00020100, L"Private Information Creator","UI");
	registerTag(0x00020102, L"Private Information","OB");
	registerTag(0x00040000, L"Directory structuring elements","UL");
	registerTag(0x00041130, L"File-set","CS");
	registerTag(0x00041141, L"File-set Descriptor File","CS");
	registerTag(0x00041142, L"Specific Character Set of File-set Descriptor File","CS");
	registerTag(0x00041200, L"Offset of the First Directory Record of the Root Directory Entity","UL");
	registerTag(0x00041202, L"Offset of the Last Directory Record of the Root Directory Entity","UL");
	registerTag(0x00041212, L"File-set Consistency Flag","US");
	registerTag(0x00041220, L"Directory Record Sequence","SQ");
	registerTag(0x00041400, L"Offset of the Next Directory Record","UL");
	registerTag(0x00041410, L"Record In-use Flag","US");
	registerTag(0x00041420, L"Offset of Referenced Lower-Level Directory Entity","UL");
	registerTag(0x00041430, L"Directory Record Type","CS");
	registerTag(0x00041432, L"Private Record","UI");
	registerTag(0x00041500, L"Referenced File","CS");
	registerTag(0x00041504, L"MRDR Directory Record Offset","UL");
	registerTag(0x00041510, L"Referenced SOP Class in File","UI");
	registerTag(0x00041511, L"Referenced SOP Instance in File","UI");
	registerTag(0x00041512, L"Referenced Transfer Syntax in File","UI");
	registerTag(0x00041600, L"Number of References","UL");
	registerTag(0x00080000, L"General data","UL");
	registerTag(0x00080001, L"Length to End","UL");
	registerTag(0x00080005, L"Specific Character Set","CS");
	registerTag(0x00080008, L"Image Type","CS");
	registerTag(0x00080010, L"Recognition Code","");
	registerTag(0x00080012, L"Instance Creation Date","DA");
	registerTag(0x00080013, L"Instance Creation Time","TM");
	registerTag(0x00080014, L"Instance Creator","UI");
	registerTag(0x00080016, L"SOP Class","UI");
	registerTag(0x00080018, L"SOP Instance","UI");
	registerTag(0x00080020, L"Study Date","DA");
	registerTag(0x00080021, L"Series Date","DA");
	registerTag(0x00080022, L"Acquisition Date","DA");
	registerTag(0x00080023, L"Content Date","DA");
	registerTag(0x00080024, L"Overlay Date","DA");
	registerTag(0x00080025, L"Curve Date","DA");
	registerTag(0x0008002A, L"Acquisition Datetime","DT");
	registerTag(0x00080030, L"Study Time","TM");
	registerTag(0x00080031, L"Series Time","TM");
	registerTag(0x00080032, L"Acquisition Time","TM");
	registerTag(0x00080033, L"Content Time","TM");
	registerTag(0x00080034, L"Overlay Time","TM");
	registerTag(0x00080035, L"Curve Time","TM");
	registerTag(0x00080040, L"Data Set Type","");
	registerTag(0x00080041, L"Data Set Subtype","");
	registerTag(0x00080042, L"Nuclear Medicine Series Type","CS");
	registerTag(0x00080050, L"Accession Number","SH");
	registerTag(0x00080052, L"Query/Retrieve Level","CS");
	registerTag(0x00080054, L"Retrieve AE Title","AE");
	registerTag(0x00080056, L"Instance Availability","CS");
	registerTag(0x00080058, L"Failed SOP InstanceD List","UI");
	registerTag(0x00080060, L"Modality","CS");
	registerTag(0x00080061, L"Modalities in Study","CS");
	registerTag(0x00080064, L"Conversion Type","CS");
	registerTag(0x00080068, L"Presentation Intent Type","CS");
	registerTag(0x00080070, L"Manufacturer","LO");
	registerTag(0x00080080, L"Institution Name","LO");
	registerTag(0x00080081, L"Institution Address","ST");
	registerTag(0x00080082, L"Institution Code Sequence","SQ");
	registerTag(0x00080090, L"Referring Physician's Name","PN");
	registerTag(0x00080092, L"Referring Physician's Address","ST");
	registerTag(0x00080094, L"Referring Physician's Telephone Numbers","SH");
	registerTag(0x00080100, L"Code Value","SH");
	registerTag(0x00080102, L"Coding Scheme Designator","SH");
	registerTag(0x00080103, L"Coding Scheme Version","SH");
	registerTag(0x00080104, L"Code Meaning","LO");
	registerTag(0x00080105, L"Mapping Resource","CS");
	registerTag(0x00080106, L"Context Group Version","DT");
	registerTag(0x00080107, L"Context Group Local Version","DT");
	registerTag(0x0008010B, L"Code Set Extension Flag","CS");
	registerTag(0x0008010C, L"Private Coding Scheme Creator","UI");
	registerTag(0x0008010D, L"Code Set Extension Creator","UI");
	registerTag(0x0008010F, L"Context Identifier","CS");
	registerTag(0x00080201, L"Timezone Offset From UTC","SH");
	registerTag(0x00081000, L"Network","");
	registerTag(0x00081010, L"Station Name","SH");
	registerTag(0x00081030, L"Study Description","LO");
	registerTag(0x00081032, L"Procedure Code Sequence","SQ");
	registerTag(0x0008103E, L"Series Description","LO");
	registerTag(0x00081040, L"Institutional Department Name","LO");
	registerTag(0x00081048, L"Physician","PN");
	registerTag(0x00081050, L"Performing Physician's Name","PN");
	registerTag(0x00081060, L"Name of Physician","PN");
	registerTag(0x00081070, L"Operator's Name","PN");
	registerTag(0x00081080, L"Admitting Diagnoses Description","LO");
	registerTag(0x00081084, L"Admitting Diagnoses Code Sequence","SQ");
	registerTag(0x00081090, L"Manufacturer's Model Name","LO");
	registerTag(0x00081100, L"Referenced Results Sequence","SQ");
	registerTag(0x00081110, L"Referenced Study Sequence","SQ");
	registerTag(0x00081111, L"Referenced Study Component Sequence","SQ");
	registerTag(0x00081115, L"Referenced Series Sequence","SQ");
	registerTag(0x00081120, L"Referenced Patient Sequence","SQ");
	registerTag(0x00081125, L"Referenced Visit Sequence","SQ");
	registerTag(0x00081130, L"Referenced Overlay Sequence","SQ");
	registerTag(0x00081140, L"Referenced Image Sequence","SQ");
	registerTag(0x00081145, L"Referenced Curve Sequence","SQ");
	registerTag(0x0008114A, L"Referenced Instance Sequence","SQ");
	registerTag(0x00081150, L"Referenced SOP Class","UI");
	registerTag(0x00081155, L"Referenced SOP Instance","UI");
	registerTag(0x0008115A, L"SOP Classes Supported","UI");
	registerTag(0x00081160, L"Referenced Frame Number","IS");
	registerTag(0x00081195, L"Transaction","UI");
	registerTag(0x00081197, L"Failure Reason","US");
	registerTag(0x00081198, L"Failed SOP Sequence","SQ");
	registerTag(0x00081199, L"Referenced SOP Sequence","SQ");
	registerTag(0x0008113A, L"Referenced Waveform Sequence","SQ");
	registerTag(0x00082110, L"Lossy Image Compression","CS");
	registerTag(0x00082111, L"Derivation Description","ST");
	registerTag(0x00082112, L"Source Image Sequence","SQ");
	registerTag(0x00082120, L"Stage Name","SH");
	registerTag(0x00082122, L"Stage Number","IS");
	registerTag(0x00082124, L"Number of Stages","IS");
	registerTag(0x00082127, L"View Name","SH");
	registerTag(0x00082128, L"View Number","IS");
	registerTag(0x00082129, L"Number of Event Timers","IS");
	registerTag(0x0008212A, L"Number of Views in Stage","IS");
	registerTag(0x00082130, L"Event Elapsed Time","DS");
	registerTag(0x00082132, L"Event Timer Name","DS");
	registerTag(0x00082142, L"Start Trim","IS");
	registerTag(0x00082143, L"Stop Trim","IS");
	registerTag(0x00082144, L"Recommended Display Frame Rate","IS");
	registerTag(0x00082200, L"Transducer Position","CS");
	registerTag(0x00082204, L"Transducer Orientation","CS");
	registerTag(0x00082208, L"Anatomic Structure","CS");
	registerTag(0x00082218, L"Anatomic Region Sequence","SQ");
	registerTag(0x00082220, L"Anatomic Region Modifier Sequence","SQ");
	registerTag(0x00082228, L"Primary Anatomic Structure Sequence","SQ");
	registerTag(0x00082229, L"Anatomic Structure Space or Region Sequence","SQ");
	registerTag(0x00082230, L"Primary Anatomic Structure Modifier Sequence","SQ");
	registerTag(0x00082240, L"Transducer Position Sequence","SQ");
	registerTag(0x00082242, L"Transducer Position Modifier Sequence","SQ");
	registerTag(0x00082244, L"Transducer Orientation Sequence","SQ");
	registerTag(0x00082246, L"Transducer Orientation Modifier Sequence","SQ");
	registerTag(0x00100000, L"Patient's data","UL");
	registerTag(0x00100010, L"Patient's Name","PN");
	registerTag(0x00100020, L"Patient","LO");
	registerTag(0x00100021, L"Issuer of Patient","LO");
	registerTag(0x00100030, L"Patient's Birth Date","DA");
	registerTag(0x00100032, L"Patient's Birth Time","TM");
	registerTag(0x00100040, L"Patient's Sex","CS");
	registerTag(0x00100050, L"Patient's Insurance Plan Code Sequence","SQ");
	registerTag(0x00100101, L"Patient's Primary Language Code Sequence","SQ");
	registerTag(0x00100102, L"Patient's Primary Language Code Modifier Sequence","SQ");
	registerTag(0x00101000, L"Other Patients","LO");
	registerTag(0x00101001, L"Other Patient Names","PN");
	registerTag(0x00101005, L"Patient's Birth Name","PN");
	registerTag(0x00101010, L"Patient's Age","AS");
	registerTag(0x00101020, L"Patient's Size","DS");
	registerTag(0x00101030, L"Patient's Weight","DS");
	registerTag(0x00101040, L"Patient's Address","LO");
	registerTag(0x00101050, L"Insurance Plan Identification","");
	registerTag(0x00101060, L"Patient's Mother's Birth Name","PN");
	registerTag(0x00101080, L"Military Rank","LO");
	registerTag(0x00101081, L"Branch of Service","LO");
	registerTag(0x00101090, L"Medical Record Locator","LO");
	registerTag(0x00102000, L"Medical Alerts","LO");
	registerTag(0x00102110, L"Contrast Allergies","LO");
	registerTag(0x00102150, L"Country of Residence","LO");
	registerTag(0x00102152, L"Region of Residence","LO");
	registerTag(0x00102154, L"Patient's Telephone Numbers","SH");
	registerTag(0x00102160, L"Ethnic Group","SH");
	registerTag(0x00102180, L"Occupation","SH");
	registerTag(0x001021A0, L"Smoking Status","CS");
	registerTag(0x001021B0, L"Additional Patient History","LT");
	registerTag(0x001021C0, L"Pregnancy Status","US");
	registerTag(0x001021D0, L"Last Menstrual Date","DA");
	registerTag(0x001021F0, L"Patient's Religious Preference","LO");
	registerTag(0x00104000, L"Patient Comments","LT");
	registerTag(0x00180000, L"Acquisition","UL");
	registerTag(0x00180010, L"Contrast/Bolus Agent","LO");
	registerTag(0x00180012, L"Contrast/Bolus Agent Sequence","SQ");
	registerTag(0x00180014, L"Contrast/Bolus Administration Route Sequence","SQ");
	registerTag(0x00180015, L"Body Part Examined","CS");
	registerTag(0x00180020, L"Scanning Sequence","CS");
	registerTag(0x00180021, L"Sequence Variant","CS");
	registerTag(0x00180022, L"Scan Options","CS");
	registerTag(0x00180023, L"MR Acquisition Type","CS");
	registerTag(0x00180024, L"Sequence Name","SH");
	registerTag(0x00180025, L"Angio Flag","CS");
	registerTag(0x00180026, L"Intervention Drug Information Sequence","SQ");
	registerTag(0x00180027, L"Intervention Drug Stop Time","TM");
	registerTag(0x00180028, L"Intervention Drug Dose","DS");
	registerTag(0x00180029, L"Intervention Drug Code Sequence","SQ");
	registerTag(0x0018002A, L"Additional Drug Sequence","SQ");
	registerTag(0x00180030, L"Radionuclide","LO");
	registerTag(0x00180031, L"Radiopharmaceutical","LO");
	registerTag(0x00180032, L"Energy Window Centerline","DS");
	registerTag(0x00180033, L"Energy Window Total Width","DS");
	registerTag(0x00180034, L"Intervention Drug Name","LO");
	registerTag(0x00180035, L"Intervention Drug Start Time","TM");
	registerTag(0x00180036, L"Interventional Therapy Sequence","SQ");
	registerTag(0x00180037, L"Therapy Type","CS");
	registerTag(0x00180038, L"Interventional Status","CS");
	registerTag(0x00180039, L"Therapy Description","CS");
	registerTag(0x00180040, L"Cine Rate","IS");
	registerTag(0x00180050, L"Slice Thickness","DS");
	registerTag(0x00180060, L"KVP","DS");
	registerTag(0x00180070, L"Counts Accumulated","IS");
	registerTag(0x00180071, L"Acquisition Termination Condition","CS");
	registerTag(0x00180072, L"Effective Series Duration","DS");
	registerTag(0x00180073, L"Acquisition Start Condition","CS");
	registerTag(0x00180074, L"Acquisition Start Condition Data","IS");
	registerTag(0x00180075, L"Acquisition Termination Condition Data","IS");
	registerTag(0x00180080, L"Repetition Time","DS");
	registerTag(0x00180081, L"Echo Time","DS");
	registerTag(0x00180082, L"Inversion Time","DS");
	registerTag(0x00180083, L"Number of Averages","DS");
	registerTag(0x00180084, L"Imaging Frequency","DS");
	registerTag(0x00180085, L"Imaged Nucleus","SH");
	registerTag(0x00180086, L"Echo Number","IS");
	registerTag(0x00180087, L"Magnetic Field Strength","DS");
	registerTag(0x00180088, L"Spacing Between Slices","DS");
	registerTag(0x00180089, L"Number of Phase Encoding Steps","IS");
	registerTag(0x00180090, L"Data Collection Diameter","DS");
	registerTag(0x00180091, L"Echo Train Length","IS");
	registerTag(0x00180093, L"Percent Sampling","DS");
	registerTag(0x00180094, L"Percent Phase Field of View","DS");
	registerTag(0x00180095, L"Pixel Bandwidth","DS");
	registerTag(0x00181000, L"Device Serial Number","LO");
	registerTag(0x00181004, L"Plate","LO");
	registerTag(0x00181010, L"Secondary Capture Device","LO");
	registerTag(0x00181011, L"Hardcopy Creation Device","LO");
	registerTag(0x00181012, L"Date of Secondary Capture","DA");
	registerTag(0x00181014, L"Time of Secondary Capture","TM");
	registerTag(0x00181016, L"Secondary Capture Device Manufacturer","LO");
	registerTag(0x00181017, L"Hardcopy Device Manufacturer","LO");
	registerTag(0x00181018, L"Secondary Capture Device Manufacturer's Model Name","LO");
	registerTag(0x00181019, L"Secondary Capture Device Software Version","LO");
	registerTag(0x0018101A, L"Hardcopy Device Software Version","LO");
	registerTag(0x0018101B, L"Hardcopy Device Manfuacturer's Model Name","LO");
	registerTag(0x00181020, L"Software Version","LO");
	registerTag(0x00181022, L"Video Image Format Acquired","SH");
	registerTag(0x00181023, L"Digital Image Format Acquired","LO");
	registerTag(0x00181030, L"Protocol Name","LO");
	registerTag(0x00181040, L"Contrast/Bolus Route","LO");
	registerTag(0x00181041, L"Contrast/Bolus Volume","DS");
	registerTag(0x00181042, L"Contrast/Bolus Start Time","TM");
	registerTag(0x00181043, L"Contrast/Bolus Stop Time","TM");
	registerTag(0x00181044, L"Contrast/Bolus Total Dose","DS");
	registerTag(0x00181045, L"Syringe Counts","IS");
	registerTag(0x00181046, L"Contrast Flow Rate","DS");
	registerTag(0x00181047, L"Contrast Flow Duration","DS");
	registerTag(0x00181048, L"Contrast/Bolus Ingredient","CS");
	registerTag(0x00181049, L"Contrast/Bolus Ingredient Concentration","DS");
	registerTag(0x00181050, L"Spatial Resolution","DS");
	registerTag(0x00181060, L"Trigger Time","DS");
	registerTag(0x00181061, L"Trigger Source or Type","LO");
	registerTag(0x00181062, L"Nominal Interval","IS");
	registerTag(0x00181063, L"Frame Time","DS");
	registerTag(0x00181064, L"Framing Type","LO");
	registerTag(0x00181065, L"Frame Time Vector","DS");
	registerTag(0x00181066, L"Frame Delay","DS");
	registerTag(0x00181067, L"Image Trigger Delay","DS");
	registerTag(0x00181068, L"Multiplex Group Time Offset","DS");
	registerTag(0x00181069, L"Trigger Time Offset","DS");
	registerTag(0x0018106A, L"Synchronization Trigger","CS");
	registerTag(0x0018106C, L"Synchronization Channel","US");
	registerTag(0x0018106E, L"Trigger Sample Position","UL");
	registerTag(0x00181070, L"Radiopharmaceutical Route","LO");
	registerTag(0x00181071, L"Radiopharmaceutical Volume","DS");
	registerTag(0x00181072, L"Radiopharmaceutical Start Time","TM");
	registerTag(0x00181073, L"Radiopharmaceutical Stop Time","TM");
	registerTag(0x00181074, L"Radionuclide Total Dose","DS");
	registerTag(0x00181075, L"Radionuclide Half Life","DS");
	registerTag(0x00181076, L"Radionuclide Positron Fraction","DS");
	registerTag(0x00181077, L"Radiopharmaceutical Specific Activity","DS");
	registerTag(0x00181080, L"Beat Rejection Flag","CS");
	registerTag(0x00181081, L"Low R-R Value","IS");
	registerTag(0x00181082, L"High R-R Value","IS");
	registerTag(0x00181083, L"Intervals Acquired","IS");
	registerTag(0x00181084, L"Intervals Rejected","IS");
	registerTag(0x00181085, L"PVC Rejection","LO");
	registerTag(0x00181086, L"Skip Beats","IS");
	registerTag(0x00181088, L"Heart Rate","IS");
	registerTag(0x00181090, L"Cardiac Number of Images","IS");
	registerTag(0x00181094, L"Trigger Window","IS");
	registerTag(0x00181100, L"Reconstruction Diameter","DS");
	registerTag(0x00181110, L"Distance Source to Detector","DS");
	registerTag(0x00181111, L"Distance Source to Patient","DS");
	registerTag(0x00181114, L"Estimated Radiographic Magnification Factor","DS");
	registerTag(0x00181120, L"Gantry/Detector Tilt","DS");
	registerTag(0x00181121, L"Gantry/Detector Slew","DS");
	registerTag(0x00181130, L"Table Height","DS");
	registerTag(0x00181131, L"Table Traverse","DS");
	registerTag(0x00181134, L"Table Motion","CS");
	registerTag(0x00181135, L"Table Vertical Increment","DS");
	registerTag(0x00181136, L"Table Lateral Increment","DS");
	registerTag(0x00181137, L"Table Longitudinal Increment","DS");
	registerTag(0x00181138, L"Table Angle","DS");
	registerTag(0x0018113A, L"Table Type","CS");
	registerTag(0x00181140, L"Rotation Direction","CS");
	registerTag(0x00181141, L"Angular Position","DS");
	registerTag(0x00181142, L"Radial Position","DS");
	registerTag(0x00181143, L"Scan Arc","DS");
	registerTag(0x00181144, L"Angular Step","DS");
	registerTag(0x00181145, L"Center of Rotation Offset","DS");
	registerTag(0x00181146, L"Rotation Offset","DS");
	registerTag(0x00181147, L"Field of View Shape","CS");
	registerTag(0x00181149, L"Field of View Dimension","IS");
	registerTag(0x00181150, L"Exposure Time","IS");
	registerTag(0x00181151, L"X-ray Tube Current","IS");
	registerTag(0x00181152, L"Exposure","IS");
	registerTag(0x00181153, L"Exposure in uAs","IS");
	registerTag(0x00181154, L"Average Pulse Width","DS");
	registerTag(0x00181155, L"Radiation Setting","CS");
	registerTag(0x00181156, L"Rectification Type","CS");
	registerTag(0x0018115A, L"Radiation Mode","CS");
	registerTag(0x0018115E, L"Image Area Dose Product","DS");
	registerTag(0x00181160, L"Filter Type","SH");
	registerTag(0x00181161, L"Type of Filters","LO");
	registerTag(0x00181162, L"Intensifier Size","DS");
	registerTag(0x00181164, L"Imager Pixel Spacing","DS");
	registerTag(0x00181166, L"Grid","CS");
	registerTag(0x00181170, L"Generator Power","IS");
	registerTag(0x00181180, L"Collimator/grid Name","SH");
	registerTag(0x00181181, L"Collimator Type","CS");
	registerTag(0x00181182, L"Focal Distance","IS");
	registerTag(0x00181183, L"X Focus Center","DS");
	registerTag(0x00181184, L"Y Focus Center","DS");
	registerTag(0x00181190, L"Focal Spot","DS");
	registerTag(0x00181191, L"Anode Target Material","CS");
	registerTag(0x001811A0, L"Body Part Thickness","DS");
	registerTag(0x001811A2, L"Compression Force","DS");
	registerTag(0x00181200, L"Date of Last Calibration","DA");
	registerTag(0x00181201, L"Time of Last Calibration","TM");
	registerTag(0x00181210, L"Convolution Kernel","SH");
	registerTag(0x00181240, L"Upper/Lower Pixel Values","");
	registerTag(0x00181242, L"Actual Frame Duration","IS");
	registerTag(0x00181243, L"Count Rate","IS");
	registerTag(0x00181244, L"Preferred Playback Sequencing","US");
	registerTag(0x00181250, L"Receiving Coil","SH");
	registerTag(0x00181251, L"Transmitting Coil","SH");
	registerTag(0x00181260, L"Plate Type","SH");
	registerTag(0x00181261, L"Phosphor Type","LO");
	registerTag(0x00181300, L"Scan Velocity","DS");
	registerTag(0x00181301, L"Whole Body Technique","CS");
	registerTag(0x00181302, L"Scan Length","IS");
	registerTag(0x00181310, L"Acquisition Matrix","US");
	registerTag(0x00181312, L"Phase Encoding Direction","CS");
	registerTag(0x00181314, L"Flip Angle","DS");
	registerTag(0x00181315, L"Variable Flip Angle Flag","CS");
	registerTag(0x00181316, L"SAR","DS");
	registerTag(0x00181318, L"dB/dt","DS");
	registerTag(0x00181400, L"Acquisition Device Processing Description","LO");
	registerTag(0x00181401, L"Acquisition Device Processing Code","LO");
	registerTag(0x00181402, L"Cassette Orientation","CS");
	registerTag(0x00181403, L"Cassette Size","CS");
	registerTag(0x00181404, L"Exposures on Plate","US");
	registerTag(0x00181405, L"Relative X-ray Exposure","IS");
	registerTag(0x00181450, L"Column Angulation","CS");
	registerTag(0x00181460, L"Tomo Layer Height","DS");
	registerTag(0x00181470, L"Tomo Angle","DS");
	registerTag(0x00181480, L"Tomo Time","DS");
	registerTag(0x00181490, L"Tomo Type","CS");
	registerTag(0x00181491, L"Tomo Class","CS");
	registerTag(0x00181495, L"Number of Tomosynthesis Source Images","IS");
	registerTag(0x00181500, L"Positioner Motion","CS");
	registerTag(0x00181508, L"Positioner Type","CS");
	registerTag(0x00181510, L"Positioner Primary Angle","DS");
	registerTag(0x00181511, L"Positioner Secondary Angle","DS");
	registerTag(0x00181520, L"Positioner Primary Angle Increment","DS");
	registerTag(0x00181521, L"Positioner Secondary Angle Increment","DS");
	registerTag(0x00181530, L"Detector Primary Angle","DS");
	registerTag(0x00181531, L"Detector Secondary Angle","DS");
	registerTag(0x00181600, L"Shutter Shape","CS");
	registerTag(0x00181602, L"Shutter Left Vertical Edge","IS");
	registerTag(0x00181604, L"Shutter Right Vertical Edge","IS");
	registerTag(0x00181606, L"Shutter Upper Horizontal Edge","IS");
	registerTag(0x00181608, L"Shutter Lower Horizontal Edge","IS");
	registerTag(0x00181610, L"Center of Circular Shutter","IS");
	registerTag(0x00181612, L"Radius of Circular Shutter","IS");
	registerTag(0x00181620, L"Vertices of the Polygonal Shutter","IS");
	registerTag(0x00181622, L"Shutter Presentation Value","US");
	registerTag(0x00181623, L"Shutter Overlay Group","US");
	registerTag(0x00181700, L"Collimator Shape","CS");
	registerTag(0x00181702, L"Collimator Left Vertical Edge","IS");
	registerTag(0x00181704, L"Collimator Right Vertical Edge","IS");
	registerTag(0x00181706, L"Collimator Upper Horizontal Edge","IS");
	registerTag(0x00181708, L"Collimator Lower Horizontal Edge","IS");
	registerTag(0x00181710, L"Center of Circular Collimator","IS");
	registerTag(0x00181712, L"Radius of Circular Collimator","IS");
	registerTag(0x00181720, L"Vertices of the Polygonal Collimator","IS");
	registerTag(0x00181800, L"Acquisition Time Synchronized","CS");
	registerTag(0x00181801, L"Time Source","SH");
	registerTag(0x00181802, L"Time Distribution Protocol","CS");
	registerTag(0x00182001, L"Page Number Vector","IS");
	registerTag(0x00182002, L"Frame Label Vector","SH");
	registerTag(0x00182003, L"Frame Primary Angle Vector","DS");
	registerTag(0x00182004, L"Frame Secondary Angle Vector","DS");
	registerTag(0x00182005, L"Slice Location Vector","DS");
	registerTag(0x00182006, L"Display Window Label Vector","SH");
	registerTag(0x00182010, L"Nominal Scanned Pixel Spacing","DS");
	registerTag(0x00182020, L"Digitizing Device Transport Direction","CS");
	registerTag(0x00182030, L"Rotation of Scanned Film","DS");
	registerTag(0x00183100, L"IVUS Acquisition","CS");
	registerTag(0x00183101, L"IVUS Pullback Rate","DS");
	registerTag(0x00183102, L"IVUS Gated Rate","DS");
	registerTag(0x00183103, L"IVUS Pullback Start Frame Number","IS");
	registerTag(0x00183104, L"IVUS Pullback Stop Frame Number","IS");
	registerTag(0x00183105, L"Lesion Number","IS");
	registerTag(0x00184000, L"Comments","");
	registerTag(0x00185000, L"Output Power","SH");
	registerTag(0x00185010, L"Transducer Data","LO");
	registerTag(0x00185012, L"Focus Depth","DS");
	registerTag(0x00185020, L"Processing Function","LO");
	registerTag(0x00185021, L"Postprocessing Function","LO");
	registerTag(0x00185022, L"Mechanical Index","DS");
	registerTag(0x00185024, L"Thermal Index","DS");
	registerTag(0x00185026, L"Cranial Thermal Index","DS");
	registerTag(0x00185027, L"Soft Tissue Thermal Index","DS");
	registerTag(0x00185028, L"Soft Tissue-focus Thermal Index","DS");
	registerTag(0x00185029, L"Soft Tissue-surface Thermal Index","DS");
	registerTag(0x00185030, L"Dynamic Range","");
	registerTag(0x00185040, L"Total Gain","");
	registerTag(0x00185050, L"Depth of Scan Field","IS");
	registerTag(0x00185100, L"Patient Position","CS");
	registerTag(0x00185101, L"View Position","CS");
	registerTag(0x00185104, L"Projection Eponymous Name Code Sequence","SQ");
	registerTag(0x00185210, L"Image Transformation Matrix","DS");
	registerTag(0x00185212, L"Image Translation Vector","DS");
	registerTag(0x00186000, L"Sensitivity","DS");
	registerTag(0x00186011, L"Sequence of Ultrasound Regions","SQ");
	registerTag(0x00186012, L"Region Spatial Format","US");
	registerTag(0x00186014, L"Region Data Type","US");
	registerTag(0x00186016, L"Region Flags","UL");
	registerTag(0x00186018, L"Region Location Min X0","UL");
	registerTag(0x0018601A, L"Region Location Min Y0","UL");
	registerTag(0x0018601C, L"Region Location Max X1","UL");
	registerTag(0x0018601E, L"Region Location Max Y1","UL");
	registerTag(0x00186020, L"Reference Pixel X0","SL");
	registerTag(0x00186022, L"Reference Pixel Y0","SL");
	registerTag(0x00186024, L"Physical Units X Direction","US");
	registerTag(0x00186026, L"Physical Units Y Direction","US");
	registerTag(0x00186028, L"Reference Pixel Physical Value X","FD");
	registerTag(0x0018602A, L"Reference Pixel Physical Value Y","FD");
	registerTag(0x0018602C, L"Physical Delta X","FD");
	registerTag(0x0018602E, L"Physical Delta Y","FD");
	registerTag(0x00186030, L"Transducer Frequency","UL");
	registerTag(0x00186031, L"Transducer Type","CS");
	registerTag(0x00186032, L"Pulse Repetition Frequency","UL");
	registerTag(0x00186034, L"Doppler Correction Angle","FD");
	registerTag(0x00186036, L"Steering Angle","FD");
	registerTag(0x00186038, L"Doppler Sample Volume X Position","UL");
	registerTag(0x0018603A, L"Doppler Sample Volume Y Position","UL");
	registerTag(0x0018603C, L"TM-Line Position X0","UL");
	registerTag(0x0018603E, L"TM-Line Position Y0","UL");
	registerTag(0x00186040, L"TM-Line Position X1","UL");
	registerTag(0x00186042, L"TM-Line Position Y1","UL");
	registerTag(0x00186044, L"Pixel Component Organization","US");
	registerTag(0x00186046, L"Pixel Component Mask","UL");
	registerTag(0x00186048, L"Pixel Component Range Start","UL");
	registerTag(0x0018604A, L"Pixel Component Range Stop","UL");
	registerTag(0x0018604C, L"Pixel Component Physical Units","US");
	registerTag(0x0018604E, L"Pixel Component Data Type","US");
	registerTag(0x00186050, L"Number of Table Break Points","UL");
	registerTag(0x00186052, L"Table of X Break Points","UL");
	registerTag(0x00186054, L"Table of Y Break Points","FD");
	registerTag(0x00186056, L"Number of Table Entries","UL");
	registerTag(0x00186058, L"Table of Pixel Values","UL");
	registerTag(0x0018605A, L"Table of Parameter Values","FL");
	registerTag(0x00187000, L"Detector Conditions Nominal Flag","CS");
	registerTag(0x00187001, L"Detector Temperature","DS");
	registerTag(0x00187004, L"Detector Type","CS");
	registerTag(0x00187005, L"Detector Configuration","CS");
	registerTag(0x00187006, L"Detector Description","LT");
	registerTag(0x00187008, L"Detector Mode","LT");
	registerTag(0x0018700A, L"Detector","SH");
	registerTag(0x0018700C, L"Date of Last Detector Calibration","DA");
	registerTag(0x0018700E, L"Time of Last Detector Calibration","TM");
	registerTag(0x00187010, L"Exposures on Detector Since Last Calibration","IS");
	registerTag(0x00187011, L"Exposures on Detector Since Manufactured","IS");
	registerTag(0x00187012, L"Detector Time Since Last Exposure","DS");
	registerTag(0x00187014, L"Detector Active Time","DS");
	registerTag(0x00187016, L"Detector Activation Offset From Exposure","DS");
	registerTag(0x0018701A, L"Detector Binning","DS");
	registerTag(0x00187020, L"Detector Element Physical Size","DS");
	registerTag(0x00187022, L"Detector Element Spacing","DS");
	registerTag(0x00187024, L"Detector Active Shape","CS");
	registerTag(0x00187026, L"Detector Active Dimension","DS");
	registerTag(0x00187028, L"Detector Active Origin","DS");
	registerTag(0x00187030, L"Field of View Origin","DS");
	registerTag(0x00187032, L"Field of View Rotation","DS");
	registerTag(0x00187034, L"Field of View Horizontal Flip","CS");
	registerTag(0x00187040, L"Grid Absorbing Material","LT");
	registerTag(0x00187041, L"Grid Spacing Material","LT");
	registerTag(0x00187042, L"Grid Thickness","DS");
	registerTag(0x00187044, L"Grid Pitch","DS");
	registerTag(0x00187046, L"Grid Aspect Ratio","IS");
	registerTag(0x00187048, L"Grid Period","DS");
	registerTag(0x0018704C, L"Grid Focal Distance","DS");
	registerTag(0x00187050, L"Filter Material","CS");
	registerTag(0x00187052, L"Filter Thickness Minimum","DS");
	registerTag(0x00187054, L"Filter Thickness Maximum","DS");
	registerTag(0x00187060, L"Exposure Control Mode","CS");
	registerTag(0x00187062, L"Exposure Control Mode Description","LT");
	registerTag(0x00187064, L"Exposure Status","CS");
	registerTag(0x00187065, L"Phototimer Setting","DS");
	registerTag(0x00188150, L"Exposure Time in uS","DS");
	registerTag(0x00188151, L"X-Ray Tube Current in uA","DS");
	registerTag(0x00200000, L"Relationship","UL");
	registerTag(0x0020000D, L"Study Instance","UI");
	registerTag(0x0020000E, L"Series Instance","UI");
	registerTag(0x00200010, L"Study","SH");
	registerTag(0x00200011, L"Series Number","IS");
	registerTag(0x00200012, L"Acquisition Number","IS");
	registerTag(0x00200013, L"Instance Number","IS");
	registerTag(0x00200014, L"Isotope Number","IS");
	registerTag(0x00200015, L"Phase Number","IS");
	registerTag(0x00200016, L"Interval Number","IS");
	registerTag(0x00200017, L"Time Slot Number","IS");
	registerTag(0x00200018, L"Angle Number","IS");
	registerTag(0x00200019, L"Item Number","IS");
	registerTag(0x00200020, L"Patient Orientation","CS");
	registerTag(0x00200022, L"Overlay Number","IS");
	registerTag(0x00200024, L"Curve Number","IS");
	registerTag(0x00200026, L"Lookup Table Number","IS");
	registerTag(0x00200030, L"Image Position","");
	registerTag(0x00200032, L"Image Position","DS");
	registerTag(0x00200035, L"Image Orientation","");
	registerTag(0x00200037, L"Image Orientation","DS");
	registerTag(0x00200050, L"Location","");
	registerTag(0x00200052, L"Frame of Reference","UI");
	registerTag(0x00200060, L"Laterality","CS");
	registerTag(0x00200062, L"Image Laterality","CS");
	registerTag(0x00200070, L"Image Geometry Type","");
	registerTag(0x00200080, L"Masking Image","");
	registerTag(0x00200100, L"Temporal Position Identifier","IS");
	registerTag(0x00200105, L"Number of Temporal Positions","IS");
	registerTag(0x00200110, L"Temporal Resolution","DS");
	registerTag(0x00200200, L"Synchronization Frame of Reference","UI");
	registerTag(0x00201000, L"Series in Study","IS");
	registerTag(0x00201001, L"Acquisitions in Series","");
	registerTag(0x00201002, L"Images in Acquisition","IS");
	registerTag(0x00201004, L"Acquisitions in Study","IS");
	registerTag(0x00201020, L"Reference","");
	registerTag(0x00201040, L"Position Reference Indicator","LO");
	registerTag(0x00201041, L"Slice Location","DS");
	registerTag(0x00201070, L"Other Study Numbers","IS");
	registerTag(0x00201200, L"Number of Patient Related Studies","IS");
	registerTag(0x00201202, L"Number of Patient Related Series","IS");
	registerTag(0x00201204, L"Number of Patient Related Instances","IS");
	registerTag(0x00201206, L"Number of Study Related Series","IS");
	registerTag(0x00201208, L"Number of Study Related Instances","IS");
	registerTag(0x00201209, L"Number of Series Related Instances","IS");
	registerTag(0x00203401, L"Modifying Device","");
	registerTag(0x00203402, L"Modified Image","");
	registerTag(0x00203403, L"Modified Image Date","");
	registerTag(0x00203404, L"Modifying Device Manufacturer","");
	registerTag(0x00203405, L"Modified Image Time","");
	registerTag(0x00203406, L"Modified Image Description","");
	registerTag(0x00204000, L"Image Comments","LT");
	registerTag(0x00205000, L"Original Image Identification","");
	registerTag(0x00205002, L"Original Image Identification Nomenclature","");
	registerTag(0x00280000, L"Image presentation","UL");
	registerTag(0x00280002, L"Samples per Pixel","US");
	registerTag(0x00280004, L"Photometric Interpretation","CS");
	registerTag(0x00280005, L"Image Dimensions","");
	registerTag(0x00280006, L"Planar Configuration","US");
	registerTag(0x00280008, L"Number of Frames","IS");
	registerTag(0x00280009, L"Frame Increment Pointer","AT");
	registerTag(0x00280010, L"Rows","US");
	registerTag(0x00280011, L"Columns","US");
	registerTag(0x00280012, L"Planes","US");
	registerTag(0x00280014, L"Ultrasound Color Data Present","US");
	registerTag(0x00280030, L"Pixel Spacing","DS");
	registerTag(0x00280031, L"Zoom Factor","DS");
	registerTag(0x00280032, L"Zoom Center","DS");
	registerTag(0x00280034, L"Pixel Aspect Ratio","IS");
	registerTag(0x00280040, L"Image Format","");
	registerTag(0x00280050, L"Manipulated Image","");
	registerTag(0x00280051, L"Corrected Image","CS");
	registerTag(0x00280060, L"Compression Code","");
	registerTag(0x00280100, L"Bits Allocated","US");
	registerTag(0x00280101, L"Bits Stored","US");
	registerTag(0x00280102, L"High Bit","US");
	registerTag(0x00280103, L"Pixel Representation","US");
	registerTag(0x00280104, L"Smallest Valid Pixel Value","");
	registerTag(0x00280105, L"Largest Valid Pixel Value","");
	registerTag(0x00280106, L"Smallest Image Pixel Value","SS");
	registerTag(0x00280107, L"Largest Image Pixel Value","SS");
	registerTag(0x00280108, L"Smallest Pixel Value in Series","SS");
	registerTag(0x00280109, L"Largest Pixel Value in Series","SS");
	registerTag(0x00280110, L"Smallest Image Pixel Value in Plane","SS");
	registerTag(0x00280111, L"Largest Image Pixel Value in Plane","SS");
	registerTag(0x00280120, L"Pixel Padding Value","SS");
	registerTag(0x00280200, L"Image Location","");
	registerTag(0x00280300, L"Quality Control Image","CS");
	registerTag(0x00280301, L"Burned In Annotation","CS");
	registerTag(0x00281040, L"Pixel Intensity Relationship","CS");
	registerTag(0x00281041, L"Pixel Intensity Relationship Sign","SS");
	registerTag(0x00281050, L"Window Center","DS");
	registerTag(0x00281051, L"Window Width","DS");
	registerTag(0x00281052, L"Rescale Intercept","DS");
	registerTag(0x00281053, L"Rescale Slope","DS");
	registerTag(0x00281054, L"Rescale Type","LO");
	registerTag(0x00281055, L"Window Center & Width Explanation","LO");
	registerTag(0x00281080, L"Gray Scale","");
	registerTag(0x00281090, L"Recommended Viewing Mode","CS");
	registerTag(0x00281100, L"Gray Lookup Table Descriptor","");
	registerTag(0x00281101, L"Red Palette Color Lookup Table Descriptor","SS");
	registerTag(0x00281102, L"Green Palette Color Lookup Table Descriptor","SS");
	registerTag(0x00281103, L"Blue Palette Color Lookup Table Descriptor","SS");
	registerTag(0x00281199, L"Palette Color Lookup Table","UI");
	registerTag(0x00281200, L"Gray Lookup Table Data","");
	registerTag(0x00281201, L"Red Palette Color Lookup Table Data","OW");
	registerTag(0x00281202, L"Green Palette Color Lookup Table Data","OW");
	registerTag(0x00281203, L"Blue Palette Color Lookup Table Data","OW");
	registerTag(0x00281221, L"Segmented Red Palette Color Lookup Table Data","OW");
	registerTag(0x00281222, L"Segmented Green Palette Color Lookup Table Data","OW");
	registerTag(0x00281223, L"Segmented Blue Palette Color Lookup Table Data","OW");
	registerTag(0x00281300, L"Implant Present","CS");
	registerTag(0x00281350, L"Partial View","CS");
	registerTag(0x00281351, L"Partial View Description","ST");
	registerTag(0x00282110, L"Lossy Image Compression","CS");
	registerTag(0x00282112, L"Lossy Image Compression Ratio","DS");
	registerTag(0x00282114, L"Lossy Image Compression Method","CS");
	registerTag(0x00283000, L"Modality LUT Sequence","SQ");
	registerTag(0x00283002, L"LUT Descriptor","SS");
	registerTag(0x00283003, L"LUT Explanation","LO");
	registerTag(0x00283004, L"Modality LUT Type","LO");
	registerTag(0x00283006, L"LUT Data","OW");
	registerTag(0x00283010, L"VOI LUT Sequence","SQ");
	registerTag(0x00283110, L"Softcopy VOI LUT Sequence","SQ");
	registerTag(0x00284000, L"Comments","");
	registerTag(0x00285000, L"Bi-Plane Acquisition Sequence","SQ");
	registerTag(0x00286010, L"Representative Frame Number","US");
	registerTag(0x00286020, L"Frame Numbers of Interest","US");
	registerTag(0x00286022, L"Frame of Interest Description","LO");
	registerTag(0x00286030, L"Mask Pointer","US");
	registerTag(0x00286040, L"R Wave Pointer","US");
	registerTag(0x00286100, L"Mask Subtraction Sequence","SQ");
	registerTag(0x00286101, L"Mask Operation","CS");
	registerTag(0x00286102, L"Applicable Frame Range","US");
	registerTag(0x00286110, L"Mask Frame Numbers","US");
	registerTag(0x00286112, L"Contrast Frame Averaging","US");
	registerTag(0x00286114, L"Mask Sub-pixel Shift","FL");
	registerTag(0x00286120, L"TID Offset","SS");
	registerTag(0x00286190, L"Mask Operation Explanation","ST");
	registerTag(0x00320000, L"Study","UL");
	registerTag(0x0032000A, L"Study Status","CS");
	registerTag(0x0032000C, L"Study Priority","CS");
	registerTag(0x00320012, L"Study Issuer","LO");
	registerTag(0x00320032, L"Study Verified Date","DA");
	registerTag(0x00320033, L"Study Verified Time","TM");
	registerTag(0x00320034, L"Study Read Date","DA");
	registerTag(0x00320035, L"Study Read Time","TM");
	registerTag(0x00321000, L"Scheduled Study Start Date","DA");
	registerTag(0x00321001, L"Scheduled Study Start Time","TM");
	registerTag(0x00321010, L"Scheduled Study Stop Date","DA");
	registerTag(0x00321011, L"Scheduled Study Stop Time","TM");
	registerTag(0x00321020, L"Scheduled Study Location","LO");
	registerTag(0x00321021, L"Scheduled Study Location AE Title","AE");
	registerTag(0x00321030, L"Reason for Study","LO");
	registerTag(0x00321032, L"Requesting Physician","PN");
	registerTag(0x00321033, L"Requesting Service","LO");
	registerTag(0x00321040, L"Study Arrival Date","DA");
	registerTag(0x00321041, L"Study Arrival Time","TM");
	registerTag(0x00321050, L"Study Completion Date","DA");
	registerTag(0x00321051, L"Study Completion Time","TM");
	registerTag(0x00321055, L"Study Component Status","CS");
	registerTag(0x00321060, L"Requested Procedure Description","LO");
	registerTag(0x00321064, L"Requested Procedure Code Sequence","SQ");
	registerTag(0x00321070, L"Requested Contrast Agent","LO");
	registerTag(0x00324000, L"Study Comments","LT");
	registerTag(0x00380000, L"","UL");
	registerTag(0x00380004, L"Referenced Patient Alias Sequence","SQ");
	registerTag(0x00380008, L"Visit Status","CS");
	registerTag(0x00380010, L"Admission","LO");
	registerTag(0x00380011, L"Issuer of Admission","LO");
	registerTag(0x00380016, L"Route of Admissions","LO");
	registerTag(0x0038001A, L"Scheduled Admission Date","DA");
	registerTag(0x0038001B, L"Scheduled Admission Time","TM");
	registerTag(0x0038001C, L"Scheduled Discharge Date","DA");
	registerTag(0x0038001D, L"Scheduled Discharge Time","TM");
	registerTag(0x0038001E, L"Scheduled Patient Institution Residence","LO");
	registerTag(0x00380020, L"Admitting Date","DA");
	registerTag(0x00380021, L"Admitting Time","TM");
	registerTag(0x00380030, L"Discharge Date","DA");
	registerTag(0x00380032, L"Discharge Time","TM");
	registerTag(0x00380040, L"Discharge Diagnosis Description","LO");
	registerTag(0x00380044, L"Discharge Diagnosis Code Sequence","SQ");
	registerTag(0x00380050, L"Special Needs","LO");
	registerTag(0x00380300, L"Current Patient Location","LO");
	registerTag(0x00380400, L"Patient's Institution Residence","LO");
	registerTag(0x00380500, L"Patient State","LO");
	registerTag(0x00384000, L"Visit Comments","LT");
	registerTag(0x003A0000, L"","UL");
	registerTag(0x003A0004, L"Waveform Originality","CS");
	registerTag(0x003A0005, L"Number of Waveform Channels","US");
	registerTag(0x003A0010, L"Number of Waveform Samples","UL");
	registerTag(0x003A001A, L"Sampling Frequency","DS");
	registerTag(0x003A0020, L"Multiplex Group Label","SH");
	registerTag(0x003A0200, L"Channel Definition Sequence","SQ");
	registerTag(0x003A0202, L"Waveform Channel Number","IS");
	registerTag(0x003A0203, L"Channel Label","SH");
	registerTag(0x003A0205, L"Channel Status","CS");
	registerTag(0x003A0208, L"Channel Source Sequence","SQ");
	registerTag(0x003A0209, L"Channel Source Modifiers Sequence","SQ");
	registerTag(0x003A020A, L"Source Waveform Sequence","SQ");
	registerTag(0x003A020C, L"Channel Derivation Description","LO");
	registerTag(0x003A0210, L"Channel Sensitivity","DS");
	registerTag(0x003A0211, L"Channel Sensitivity Units Sequence","SQ");
	registerTag(0x003A0212, L"Channel Sensitivity Correction Factor","DS");
	registerTag(0x003A0213, L"Channel Baseline","DS");
	registerTag(0x003A0214, L"Channel Time Skew","DS");
	registerTag(0x003A0215, L"Channel Sample Skew","DS");
	registerTag(0x003A0218, L"Channel Offset","DS");
	registerTag(0x003A021A, L"Waveform Bits Stored","US");
	registerTag(0x003A0220, L"Filter Low Frequency","DS");
	registerTag(0x003A0221, L"Filter High Frequency","DS");
	registerTag(0x003A0222, L"Notch Filter Frequency","DS");
	registerTag(0x003A0223, L"Notch Filter Bandwidth","DS");
	registerTag(0x00400000, L"","UL");
	registerTag(0x00400001, L"Scheduled Station AE Title ","AE");
	registerTag(0x00400002, L"Scheduled Procedure Step Start Date","DA");
	registerTag(0x00400003, L"Scheduled Procedure Step Start Time","TM");
	registerTag(0x00400004, L"Scheduled Procedure Step End Date","DA");
	registerTag(0x00400005, L"Scheduled Procedure Step End Time","TM");
	registerTag(0x00400006, L"Scheduled Performing Physician's Name","PN");
	registerTag(0x00400007, L"Scheduled Procedure Step Description","LO");
	registerTag(0x00400008, L"Scheduled Protocol Code Sequence","SQ");
	registerTag(0x00400009, L"Scheduled Procedure Step","SH");
	registerTag(0x0040000A, L"Stage Code Sequence","SQ");
	registerTag(0x00400010, L"Scheduled Station Name","SH");
	registerTag(0x00400011, L"Scheduled Procedure Step Location","SH");
	registerTag(0x00400012, L"Pre-Medication","LO");
	registerTag(0x00400020, L"Scheduled Procedure Step Status","CS");
	registerTag(0x00400100, L"Scheduled Procedure Step Sequence","SQ");
	registerTag(0x00400220, L"Referenced Non-Image Composite SOP Instance Sequence","SQ");
	registerTag(0x00400241, L"Performed Station AE Title","AE");
	registerTag(0x00400242, L"Performed Station Name","SH");
	registerTag(0x00400243, L"Performed Location","SH");
	registerTag(0x00400244, L"Performed Procedure Step Start Date","DA");
	registerTag(0x00400245, L"Performed Procedure Step Start Time","TM");
	registerTag(0x00400250, L"Performed Procedure Step End Date","DA");
	registerTag(0x00400251, L"Performed Procedure Step End Time","TM");
	registerTag(0x00400252, L"Performed Procedure Step Status","CS");
	registerTag(0x00400253, L"Performed Procedure Step","SH");
	registerTag(0x00400254, L"Performed Procedure Step Description","LO");
	registerTag(0x00400255, L"Performed Procedure Type Description","LO");
	registerTag(0x00400260, L"Performed Protocol Code Sequence","SQ");
	registerTag(0x00400270, L"Scheduled Step Attributes Sequence","SQ");
	registerTag(0x00400275, L"Request Attributes Sequence","SQ");
	registerTag(0x00400280, L"Comments on the Performed Procedure Step","ST");
	registerTag(0x00400293, L"Quantity Sequence","SQ");
	registerTag(0x00400294, L"Quantity","DS");
	registerTag(0x00400295, L"Measuring Units Sequence","SQ");
	registerTag(0x00400296, L"Billing Item Sequence","SQ");
	registerTag(0x00400300, L"Total Time of Fluoroscopy","US");
	registerTag(0x00400301, L"Total Number of Exposures","US");
	registerTag(0x00400302, L"Entrance Dose","US");
	registerTag(0x00400303, L"Exposed Area","US");
	registerTag(0x00400306, L"Distance Source to Entrance","DS");
	registerTag(0x00400307, L"Distance Source to Support","DS");
	registerTag(0x0040030E, L"Exposure Dose Sequence","SQ");
	registerTag(0x00400310, L"Comments on Radiation Dose","ST");
	registerTag(0x00400312, L"X-Ray Output","DS");
	registerTag(0x00400314, L"Half Value Layer","DS");
	registerTag(0x00400316, L"Organ Dose","DS");
	registerTag(0x00400318, L"Organ Exposed","CS");
	registerTag(0x00400320, L"Billing Procedure Step Sequence","SQ");
	registerTag(0x00400321, L"Film Consumption Sequence","SQ");
	registerTag(0x00400324, L"Billing Supplies and Devices Sequence","SQ");
	registerTag(0x00400330, L"Referenced Procedure Step Sequence","SQ");
	registerTag(0x00400340, L"Performed Series Sequence","SQ");
	registerTag(0x00400400, L"Comments on the Scheduled Procedure Step","LT");
	registerTag(0x0040050A, L"Specimen Accession Number","LO");
	registerTag(0x00400550, L"Specimen Sequence","SQ");
	registerTag(0x00400551, L"Specimen Identifier","LO");
	registerTag(0x00400555, L"Acquisition Context Sequence","SQ");
	registerTag(0x00400556, L"Acquisition Context Description","ST");
	registerTag(0x0040059A, L"Specimen Type Code Sequence","SQ");
	registerTag(0x004006FA, L"Slide Identifier","LO");
	registerTag(0x0040071A, L"Image Center Point Coordinates Sequence","SQ");
	registerTag(0x0040072A, L"X offset in Slide Coordinate System","DS");
	registerTag(0x0040073A, L"Y offset in Slide Coordinate System","DS");
	registerTag(0x0040074A, L"Z offset in Slide Coordinate System","DS");
	registerTag(0x004008D8, L"Pixel Spacing Sequence","SQ");
	registerTag(0x004008DA, L"Coordinate System Axis Code Sequence","SQ");
	registerTag(0x004008EA, L"Measurement Units Code Sequence","SQ");
	registerTag(0x00401001, L"Requested Procedure","SH");
	registerTag(0x00401002, L"Reason for the Requested Procedure","LO");
	registerTag(0x00401003, L"Requested Procedure Priority","SH");
	registerTag(0x00401004, L"Patient Transport Arrangements","LO");
	registerTag(0x00401005, L"Requested Procedure Location","LO");
	registerTag(0x00401006, L"Placer Order Number / Procedure","SH");
	registerTag(0x00401007, L"Filler Order Number / Procedure","SH");
	registerTag(0x00401008, L"Confidentiality Code","LO");
	registerTag(0x00401009, L"Reporting Priority","SH");
	registerTag(0x00401010, L"Names of Intended Recipients of Results","PN");
	registerTag(0x00401400, L"Requested Procedure Comments","LT");
	registerTag(0x00402001, L"Reason for the Imaging Service Request","LO");
	registerTag(0x00402004, L"Issue Date of Imaging Service Request","DA");
	registerTag(0x00402005, L"Issue Time of Imaging Service Request","TM");
	registerTag(0x00402006, L"Placer Order Number / Imaging Service Request","SH");
	registerTag(0x00402007, L"Filler Order Number / Imaging Service Request","SH");
	registerTag(0x00402008, L"Order Entered By","PN");
	registerTag(0x00402009, L"Order Enterer's Location","SH");
	registerTag(0x00402010, L"Order Callback Phone Number","SH");
	registerTag(0x00402016, L"Placer Order Number / Imaging Service Request","LO");
	registerTag(0x00402017, L"Filler Order Number / Imaging Service Request","LO");
	registerTag(0x00402400, L"Imaging Service Request Comments","LT");
	registerTag(0x00403001, L"Confidentiality Constraint on Patient Data Description","LO");
	registerTag(0x00404001, L"General Purpose Scheduled Procedure Step Status","CS");
	registerTag(0x00404002, L"General Purpose Performed Procedure Step Status","CS");
	registerTag(0x00404003, L"General Purpose Scheduled Procedure Step Priority","CS");
	registerTag(0x00404004, L"Scheduled Processing Applications Code Sequence","SQ");
	registerTag(0x00404005, L"Scheduled Procedure Step Start Date and Time","DT");
	registerTag(0x00404006, L"Multiple Copies Flag","CS");
	registerTag(0x00404007, L"Performed Processing Applications Code Sequence","SQ");
	registerTag(0x00404009, L"Human Performer Code Sequence","SQ");
	registerTag(0x00404011, L"Expected Completion Date and Time","DT");
	registerTag(0x00404015, L"Resulting General Purpose Performed Procedure Steps Sequence","SQ");
	registerTag(0x00404016, L"Referenced General Purpose Scheduled Procedure Step Sequence","SQ");
	registerTag(0x00404018, L"Scheduled Workitem Code Sequence","SQ");
	registerTag(0x00404019, L"Performed Workitem Code Sequence","SQ");
	registerTag(0x00404020, L"Input Availability Flag","CS");
	registerTag(0x00404021, L"Input InformationSequence","SQ");
	registerTag(0x00404022, L"Relevant Information Sequence","SQ");
	registerTag(0x00404023, L"Referenced General Purpose Scheduled Procedure Step Transaction","UI");
	registerTag(0x00404025, L"Scheduled Station Name Code Sequence","SQ");
	registerTag(0x00404026, L"Scheduled Station Class Code Sequence","SQ");
	registerTag(0x00404027, L"Scheduled Station Geographic Location Code Sequence","SQ");
	registerTag(0x00404028, L"Performed Station Name Code Sequence","SQ");
	registerTag(0x00404029, L"Performed Station Class Code Sequence","SQ");
	registerTag(0x00404030, L"Performed Station Geographic Location Code Sequence","SQ");
	registerTag(0x00404031, L"Requested Subsequent Workitem Code Sequence","SQ");
	registerTag(0x00404032, L"Non-DICOM Output Code Sequence","SQ");
	registerTag(0x00404033, L"Output Information Sequence","SQ");
	registerTag(0x00404034, L"Scheduled Human Performers Sequence","SQ");
	registerTag(0x00404035, L"Actual Human Performers Sequence","SQ");
	registerTag(0x00404036, L"Human Performer's Organization","LO");
	registerTag(0x00404037, L"Human Performer's Name","PN");
	registerTag(0x00408302, L"Entrance Dose in mGy","DS");
	registerTag(0x0040A010, L"Relationship Type","CS");
	registerTag(0x0040A027, L"Verifying Organization","LO");
	registerTag(0x0040A030, L"Verification DateTime","DT");
	registerTag(0x0040A032, L"Observation DateTime","DT");
	registerTag(0x0040A040, L"Value Type","CS");
	registerTag(0x0040A043, L"Concept-name Code Sequence","SQ");
	registerTag(0x0040A050, L"Continuity Of Content","CS");
	registerTag(0x0040A073, L"Verifying Observer Sequence","SQ");
	registerTag(0x0040A075, L"Verifying Observer Name","PN");
	registerTag(0x0040A088, L"Verifying Observer Identification Code Sequence","SQ");
	registerTag(0x0040A0B0, L"Referenced Waveform Channels","US");
	registerTag(0x0040A120, L"DateTime","DT");
	registerTag(0x0040A121, L"Date","DA");
	registerTag(0x0040A122, L"Time","TM");
	registerTag(0x0040A123, L"Person Name","PN");
	registerTag(0x0040A124, L"UI","UI");
	registerTag(0x0040A130, L"Temporal Range Type","CS");
	registerTag(0x0040A132, L"Referenced Sample Positions","UL");
	registerTag(0x0040A136, L"Referenced Frame Numbers","US");
	registerTag(0x0040A138, L"Referenced Time Offsets","DS");
	registerTag(0x0040A13A, L"Referenced Datetime","DT");
	registerTag(0x0040A160, L"Text Value","UT");
	registerTag(0x0040A168, L"Concept Code Sequence","SQ");
	registerTag(0x0040A170, L"Purpose of Reference Code Sequence","SQ");
	registerTag(0x0040A180, L"Annotation Group Number","US");
	registerTag(0x0040A195, L"Modifier Code Sequence","SQ");
	registerTag(0x0040A300, L"Measured Value Sequence","SQ");
	registerTag(0x0040A30A, L"Numeric Value","DS");
	registerTag(0x0040A360, L"Predecessor Documents Sequence","SQ");
	registerTag(0x0040A370, L"Referenced Request Sequence","SQ");
	registerTag(0x0040A372, L"Performed Procedure Code Sequence","SQ");
	registerTag(0x0040A375, L"Current Requested Procedure Evidence Sequence","SQ");
	registerTag(0x0040A385, L"Pertinent Other Evidence Sequence","SQ");
	registerTag(0x0040A491, L"Completion Flag","CS");
	registerTag(0x0040A492, L"Completion Flag Description","LO");
	registerTag(0x0040A493, L"Verification Flag","CS");
	registerTag(0x0040A504, L"Content Template Sequence","SQ");
	registerTag(0x0040A525, L"Identical Documents Sequence","SQ");
	registerTag(0x0040A730, L"Content Sequence","SQ");
	registerTag(0x0040B020, L"Annotation Sequence","SQ");
	registerTag(0x0040DB00, L"Template Identifier","CS");
	registerTag(0x0040DB06, L"Template Version","DT");
	registerTag(0x0040DB07, L"Template Local Version","DT");
	registerTag(0x0040DB0B, L"Template Extension Flag","CS");
	registerTag(0x0040DB0C, L"Template Extension Organization","UI");
	registerTag(0x0040DB0D, L"Template Extension Creator","UI");
	registerTag(0x0040DB73, L"Referenced Content Item Identifier","UL");
	registerTag(0x00500000, L"","UL");
	registerTag(0x00500004, L"Calibration Image","CS");
	registerTag(0x00500010, L"Device Sequence","SQ");
	registerTag(0x00500014, L"Device Length","DS");
	registerTag(0x00500016, L"Device Diameter","DS");
	registerTag(0x00500017, L"Device Diameter Units","CS");
	registerTag(0x00500018, L"Device Volume","DS");
	registerTag(0x00500019, L"Inter-marker Distance","DS");
	registerTag(0x00500020, L"Device Description","LO");
	registerTag(0x00540000, L"Nuclear Acquisition","UL");
	registerTag(0x00540010, L"Energy Window Vector","US");
	registerTag(0x00540011, L"Number of Energy Windows","US");
	registerTag(0x00540012, L"Energy Window Information Sequence","SQ");
	registerTag(0x00540013, L"Energy Window Range Sequence","SQ");
	registerTag(0x00540014, L"Energy Window Lower Limit","DS");
	registerTag(0x00540015, L"Energy Window Upper Limit","DS");
	registerTag(0x00540016, L"Radiopharmaceutical Information Sequence","SQ");
	registerTag(0x00540017, L"Residual Syringe Counts","IS");
	registerTag(0x00540018, L"Energy Window Name","SH");
	registerTag(0x00540020, L"Detector Vector","US");
	registerTag(0x00540021, L"Number of Detectors","US");
	registerTag(0x00540022, L"Detector Information Sequence","SQ");
	registerTag(0x00540030, L"Phase Vector","US");
	registerTag(0x00540031, L"Number of phases","US");
	registerTag(0x00540032, L"Phase Information Sequence","SQ");
	registerTag(0x00540033, L"Number of Frames in Phase","US");
	registerTag(0x00540036, L"Phase Delay","IS");
	registerTag(0x00540038, L"Pause Between Frames","IS");
	registerTag(0x00540050, L"Rotation Vector","US");
	registerTag(0x00540051, L"Number of Rotations","US");
	registerTag(0x00540052, L"Rotation Information Sequence","SQ");
	registerTag(0x00540053, L"Number of Frames in Rotation","US");
	registerTag(0x00540060, L"R-R Interval Vector","US");
	registerTag(0x00540061, L"Number of R-R Intervals","US");
	registerTag(0x00540062, L"Gated Information Sequence","SQ");
	registerTag(0x00540063, L"Data Information Sequence","SQ");
	registerTag(0x00540070, L"Time Slot Vector","US");
	registerTag(0x00540071, L"Number of Time Slots","US");
	registerTag(0x00540072, L"Time Slot Information Sequence","SQ");
	registerTag(0x00540073, L"Time Slot Time","DS");
	registerTag(0x00540080, L"Slice Vector","US");
	registerTag(0x00540081, L"Number of Slices","US");
	registerTag(0x00540090, L"Angular View Vector","US");
	registerTag(0x00540100, L"Time Slice Vector","US");
	registerTag(0x00540101, L"Number of Time Slices","US");
	registerTag(0x00540200, L"Start Angle","DS");
	registerTag(0x00540202, L"Type of Detector Motion","CS");
	registerTag(0x00540210, L"Trigger Vector","IS");
	registerTag(0x00540211, L"Number of Triggers in Phase","US");
	registerTag(0x00540220, L"View Code Sequence","SQ");
	registerTag(0x00540222, L"View Modifier Code Sequence","SQ");
	registerTag(0x00540300, L"Radionuclide Code Sequence","SQ");
	registerTag(0x00540302, L"Administration Route Code Sequence","SQ");
	registerTag(0x00540304, L"Radiopharmaceutical Code Sequence","SQ");
	registerTag(0x00540306, L"Calibration Data Sequence","SQ");
	registerTag(0x00540308, L"Energy Window Number","US");
	registerTag(0x00540400, L"Image","SH");
	registerTag(0x00540410, L"Patient Orientation Code Sequence","SQ");
	registerTag(0x00540412, L"Patient Orientation Modifier Code Sequence","SQ");
	registerTag(0x00540414, L"Patient Gantry Relationship Code Sequence","SQ");
	registerTag(0x00541000, L"Series Type","CS");
	registerTag(0x00541001, L"Units","CS");
	registerTag(0x00541002, L"Counts Source","CS");
	registerTag(0x00541004, L"Reprojection Method","CS");
	registerTag(0x00541100, L"Randoms Correction Method","CS");
	registerTag(0x00541101, L"Attenuation Correction Method","LO");
	registerTag(0x00541102, L"Decay Correction","CS");
	registerTag(0x00541103, L"Reconstruction Method","LO");
	registerTag(0x00541104, L"Detector Lines of Response Used","LO");
	registerTag(0x00541105, L"Scatter Correction Method","LO");
	registerTag(0x00541200, L"Axial Acceptance","DS");
	registerTag(0x00541201, L"Axial Mash","IS");
	registerTag(0x00541202, L"Transverse Mash","IS");
	registerTag(0x00541203, L"Detector Element Size","DS");
	registerTag(0x00541210, L"Coincidence Window Width","DS");
	registerTag(0x00541220, L"Secondary Counts Type","CS");
	registerTag(0x00541300, L"Frame Reference Time","DS");
	registerTag(0x00541310, L"Primary  Prompts, Counts Accumulated","IS");
	registerTag(0x00541311, L"Secondary Counts Accumulated","IS");
	registerTag(0x00541320, L"Slice Sensitivity Factor","DS");
	registerTag(0x00541321, L"Decay Factor","DS");
	registerTag(0x00541322, L"Dose Calibration Factor","DS");
	registerTag(0x00541323, L"Scatter Fraction Factor","DS");
	registerTag(0x00541324, L"Dead Time Factor","DS");
	registerTag(0x00541330, L"Image Index","US");
	registerTag(0x00541400, L"Counts Included","CS");
	registerTag(0x00541401, L"Dead Time Correction Flag","CS");
	registerTag(0x00600000, L"Histogram","UL");
	registerTag(0x00603000, L"Histogram Sequence","SQ");
	registerTag(0x00603002, L"Histogram Number of Bins","US");
	registerTag(0x00603004, L"Histogram First Bin Value","SS");
	registerTag(0x00603006, L"Histogram Last Bin Value","SS");
	registerTag(0x00603008, L"Histogram Bin Width","US");
	registerTag(0x00603010, L"Histogram Explanation","LO");
	registerTag(0x00603020, L"Histogram Data","UL");
	registerTag(0x00700000, L"Graphic annotation","UL");
	registerTag(0x00700001, L"Graphic Annotation Sequence","SQ");
	registerTag(0x00700002, L"Graphic Layer","CS");
	registerTag(0x00700003, L"Bounding Box Annotation Units","CS");
	registerTag(0x00700004, L"Anchor Point Annotation Units","CS");
	registerTag(0x00700005, L"Graphic Annotation Units","CS");
	registerTag(0x00700006, L"Unformatted Text Value","ST");
	registerTag(0x00700008, L"Text Object Sequence","SQ");
	registerTag(0x00700009, L"Graphic Object Sequence","SQ");
	registerTag(0x00700010, L"Bounding Box Top Left Hand Corner","FL");
	registerTag(0x00700011, L"Bounding Box Bottom Right Hand Corner","FL");
	registerTag(0x00700012, L"Bounding Box Text Horizontal Justification","CS");
	registerTag(0x00700014, L"Anchor Point","FL");
	registerTag(0x00700015, L"Anchor Point Visibility","CS");
	registerTag(0x00700020, L"Graphic Dimensions","US");
	registerTag(0x00700021, L"Number of Graphic Points","US");
	registerTag(0x00700022, L"Graphic Data","FL");
	registerTag(0x00700023, L"Graphic Type","CS");
	registerTag(0x00700024, L"Graphic Filled","CS");
	registerTag(0x00700041, L"Image Horizontal Flip","CS");
	registerTag(0x00700042, L"Image Rotation","US");
	registerTag(0x00700052, L"Displayed Area Top Left Hand Corner","SL");
	registerTag(0x00700053, L"Displayed Area Bottom Right Hand Corner","SL");
	registerTag(0x0070005A, L"Displayed Area Selection Sequence","SQ");
	registerTag(0x00700060, L"Graphic Layer Sequence","SQ");
	registerTag(0x00700062, L"Graphic Layer Order","IS");
	registerTag(0x00700066, L"Graphic Layer Recommended Display Grayscale Value","US");
	registerTag(0x00700067, L"Graphic Layer Recommended Display RGB Value","US");
	registerTag(0x00700068, L"Graphic Layer Description","LO");
	registerTag(0x00700080, L"Presentation Label","CS");
	registerTag(0x00700081, L"Presentation Description","LO");
	registerTag(0x00700082, L"Presentation Creation Date","DA");
	registerTag(0x00700083, L"Presentation Creation Time","TM");
	registerTag(0x00700084, L"Presentation Creator's Name","PN");
	registerTag(0x00700100, L"Presentation Size Mode","CS");
	registerTag(0x00700101, L"Presentation Pixel Spacing","DS");
	registerTag(0x00700102, L"Presentation Pixel Aspect Ratio","IS");
	registerTag(0x00700103, L"Presentation Pixel Magnification Ratio","FL");
	registerTag(0x00880000, L"Topic","UL");
	registerTag(0x00880130, L"Storage Media File-set ID","SH");
	registerTag(0x00880140, L"Storage Media File-set UID","UI");
	registerTag(0x00880200, L"Icon Image Sequence","SQ");
	registerTag(0x00880904, L"Topic Title","LO");
	registerTag(0x00880906, L"Topic Subject","ST");
	registerTag(0x00880910, L"Topic Author","LO");
	registerTag(0x00880912, L"Topic Key Words","LO");
	registerTag(0x01000000, L"Authorization","UL");
	registerTag(0x01000410, L"SOP Instance Status","CS");
	registerTag(0x01000420, L"SOP Authorization Date and Time","DT");
	registerTag(0x01000424, L"SOP Authorization Comment","LT");
	registerTag(0x01000426, L"Authorization Equipment Certification Number","LO");
	registerTag(0x04000000, L"Signature","UL");
	registerTag(0x04000005, L"MAC number","US");
	registerTag(0x04000010, L"MAC Calculation Transfer Syntax","UI");
	registerTag(0x04000015, L"MAC Algorithm","CS");
	registerTag(0x04000020, L"Data Elements Signed","AT");
	registerTag(0x04000100, L"Digital Signature","UI");
	registerTag(0x04000105, L"Digital Signature DateTime","DT");
	registerTag(0x04000110, L"Certificate Type","CS");
	registerTag(0x04000115, L"Certificate of Signer","OB");
	registerTag(0x04000120, L"Signature","OB");
	registerTag(0x04000305, L"Certified Timestamp Type","CS");
	registerTag(0x04000310, L"Certified Timestamp","OB");
	registerTag(0x20000000, L"","UL");
	registerTag(0x20000010, L"Number of Copies","IS");
	registerTag(0x2000001E, L"Printer Configuration Sequence","SQ");
	registerTag(0x20000020, L"Print Priority","CS");
	registerTag(0x20000030, L"Medium Type","CS");
	registerTag(0x20000040, L"Film Destination","CS");
	registerTag(0x20000050, L"Film Session Label","LO");
	registerTag(0x20000060, L"Memory Allocation","IS");
	registerTag(0x20000061, L"Maximum Memory Allocation","IS");
	registerTag(0x20000062, L"Color Image Printing Flag","CS");
	registerTag(0x20000063, L"Collation Flag","CS");
	registerTag(0x20000065, L"Annotation Flag","CS");
	registerTag(0x20000067, L"Image Overlay Flag","CS");
	registerTag(0x20000069, L"Presentation LUT Flag","CS");
	registerTag(0x2000006A, L"Image Box Presentation LUT Flag","");
	registerTag(0x200000A0, L"Memory Bit Depth","");
	registerTag(0x200000A1, L"Printing Bit Depth","");
	registerTag(0x200000A2, L"Media Installed Sequence","");
	registerTag(0x200000A4, L"Other Media Available Sequence","");
	registerTag(0x200000A8, L"Supported Image Display Formats Sequence","");
	registerTag(0x20000500, L"Referenced Film Box Sequence","");
	registerTag(0x20000510, L"Referenced Stored Print Sequence","");
	registerTag(0x20100000, L"Film box","UL");
	registerTag(0x20100010, L"Image Display Format","ST");
	registerTag(0x20100030, L"Annotation Display Format ID","CS");
	registerTag(0x20100040, L"Film Orientation","CS");
	registerTag(0x20100050, L"Film Size ID","CS");
	registerTag(0x20100052, L"Printer Resolution ID","CS");
	registerTag(0x20100054, L"Default Printer Resolution ID","CS");
	registerTag(0x20100060, L"Magnification Type","CS");
	registerTag(0x20100080, L"Smoothing Type","CS");
	registerTag(0x201000A6, L"Default Magnification Type","CS");
	registerTag(0x201000A7, L"Other Magnification Types Available","CS");
	registerTag(0x201000A8, L"Default Smoothing Type","CS");
	registerTag(0x201000A9, L"Other Smoothing Types Available","CS");
	registerTag(0x20100100, L"Border Density","CS");
	registerTag(0x20100110, L"Empty Image Density","CS");
	registerTag(0x20100120, L"Min Density","US");
	registerTag(0x20100130, L"Max Density","US");
	registerTag(0x20100140, L"Trim","CS");
	registerTag(0x20100150, L"Configuration Information","ST");
	registerTag(0x20100152, L"Configuration Information Description","LT");
	registerTag(0x20100154, L"Maximum Collated Films","IS");
	registerTag(0x2010015E, L"Illumination","US");
	registerTag(0x20100160, L"Reflected Ambient Light","US");
	registerTag(0x20100376, L"Printer Pixel Spacing","DS");
	registerTag(0x20100500, L"Referenced Film Session Sequence","SQ");
	registerTag(0x20100510, L"Referenced Image Box Sequence","SQ");
	registerTag(0x20100520, L"Referenced Basic Annotation Box Sequence","SQ");
	registerTag(0x20200000, L"","UL");
	registerTag(0x20200010, L"Image Position","US");
	registerTag(0x20200020, L"Polarity","CS");
	registerTag(0x20200030, L"Requested Image Size","DS");
	registerTag(0x20200040, L"Requested Decimate/Crop Behavior","CS");
	registerTag(0x20200050, L"Requested Resolution","CS");
	registerTag(0x202000A0, L"Requested Image Size Flag","CS");
	registerTag(0x202000A2, L"Decimate/Crop Result","CS");
	registerTag(0x20200110, L"Basic Grayscale Image Sequence","SQ");
	registerTag(0x20200111, L"Basic Color Image Sequence","SQ");
	registerTag(0x20200130, L"Referenced Image Overlay Box Sequence","SQ");
	registerTag(0x20200140, L"Referenced VOI LUT Box Sequence","SQ");
	registerTag(0x20300000, L"","UL");
	registerTag(0x20300010, L"Annotation Position","US");
	registerTag(0x20300020, L"Text String","LO");
	registerTag(0x20400000, L"","UL");
	registerTag(0x20400010, L"Referenced Overlay Plane Sequence","SQ");
	registerTag(0x20400011, L"Referenced Overlay Plane Groups","US");
	registerTag(0x20400020, L"Overlay Pixel Data Sequence","SQ");
	registerTag(0x20400060, L"Overlay Magnification Type","CS");
	registerTag(0x20400070, L"Overlay Smoothing Type","CS");
	registerTag(0x20400072, L"Overlay or Image Magnification","CS");
	registerTag(0x20400074, L"Magnify to Number of Columns","US");
	registerTag(0x20400080, L"Overlay Foreground Density","CS");
	registerTag(0x20400082, L"Overlay Background Density","CS");
	registerTag(0x20400090, L"Overlay Mode","CS");
	registerTag(0x20400100, L"Threshold Density","CS");
	registerTag(0x20400500, L"Referenced Image Box Sequence","SQ");
	registerTag(0x20500000, L"","UL");
	registerTag(0x20500010, L"Presentation LUT Sequence","SQ");
	registerTag(0x20500020, L"Presentation LUT Shape","CS");
	registerTag(0x20500500, L"Referenced Presentation LUT Sequence","SQ");
	registerTag(0x21000000, L"","UL");
	registerTag(0x21000010, L"Print Job","SH");
	registerTag(0x21000020, L"Execution Status","CS");
	registerTag(0x21000030, L"Execution Status Info","CS");
	registerTag(0x21000040, L"Creation Date","DA");
	registerTag(0x21000050, L"Creation Time","TM");
	registerTag(0x21000070, L"Originator","AE");
	registerTag(0x21000140, L"Destination","AE");
	registerTag(0x21000160, L"Owner ID","SH");
	registerTag(0x21000170, L"Number of Films","IS");
	registerTag(0x21000500, L"Referenced Print Job Sequence","SQ");
	registerTag(0x21100000, L"","UL");
	registerTag(0x21100010, L"Printer Status","CS");
	registerTag(0x21100020, L"Printer Status Info","CS");
	registerTag(0x21100030, L"Printer Name","LO");
	registerTag(0x21100099, L"Print Queue","SH");
	registerTag(0x21200000, L"","UL");
	registerTag(0x21200010, L"Queue Status","CS");
	registerTag(0x21200050, L"Print Job Description Sequence","SQ");
	registerTag(0x21200070, L"Referenced Print Job Sequence","SQ");
	registerTag(0x21300000, L"","UL");
	registerTag(0x21300010, L"Print Management Capabilities Sequence","SQ");
	registerTag(0x21300015, L"Printer Characteristics Sequence","SQ");
	registerTag(0x21300030, L"Film Box Content Sequence","SQ");
	registerTag(0x21300040, L"Image Box Content Sequence","SQ");
	registerTag(0x21300050, L"Annotation Content Sequence","SQ");
	registerTag(0x21300060, L"Image Overlay Box Content Sequence","SQ");
	registerTag(0x21300080, L"Presentation LUT Content Sequence","SQ");
	registerTag(0x213000A0, L"Proposed Study Sequence","SQ");
	registerTag(0x213000C0, L"Original Image Sequence","SQ");
	registerTag(0x30020000, L"","UL");
	registerTag(0x30020002, L"RT Image Label","SH");
	registerTag(0x30020003, L"RT Image Name","LO");
	registerTag(0x30020004, L"RT Image Description","ST");
	registerTag(0x3002000A, L"Reported Values Origin","CS");
	registerTag(0x3002000C, L"RT Image Plane","CS");
	registerTag(0x3002000D, L"X-Ray Image Receptor Translation","DS");
	registerTag(0x3002000E, L"X-Ray Image Receptor Angle","DS");
	registerTag(0x30020010, L"RT Image Orientation","DS");
	registerTag(0x30020011, L"Image Plane Pixel Spacing","DS");
	registerTag(0x30020012, L"RT Image Position","DS");
	registerTag(0x30020020, L"Radiation Machine Name","SH");
	registerTag(0x30020022, L"Radiation Machine SA","DS");
	registerTag(0x30020024, L"Radiation Machine SS","DS");
	registerTag(0x30020026, L"RT Image SI","DS");
	registerTag(0x30020028, L"Source to Reference Object Distance","DS");
	registerTag(0x30020029, L"Fraction Number","IS");
	registerTag(0x30020030, L"Exposure Sequence","SQ");
	registerTag(0x30020032, L"Meterset Exposure","DS");
	registerTag(0x30020034, L"Diaphragm Position","DS");
	registerTag(0x30040000, L"","UL");
	registerTag(0x30040001, L"DVH Type","CS");
	registerTag(0x30040002, L"Dose Units","CS");
	registerTag(0x30040004, L"Dose Type","CS");
	registerTag(0x30040006, L"Dose Comment","LO");
	registerTag(0x30040008, L"Normalization Point","DS");
	registerTag(0x3004000A, L"Dose Summation Type","CS");
	registerTag(0x3004000C, L"Grid Frame Offset Vector","DS");
	registerTag(0x3004000E, L"Dose Grid Scaling","DS");
	registerTag(0x30040010, L"RT Dose ROI Sequence","SQ");
	registerTag(0x30040012, L"Dose Value","DS");
	registerTag(0x30040040, L"DVH Normalization Point","DS");
	registerTag(0x30040042, L"DVH Normalization Dose Value","DS");
	registerTag(0x30040050, L"DVH Sequence","SQ");
	registerTag(0x30040052, L"DVH Dose Scaling","DS");
	registerTag(0x30040054, L"DVH Volume Units","CS");
	registerTag(0x30040056, L"DVH Number of Bins","IS");
	registerTag(0x30040058, L"DVH Data","DS");
	registerTag(0x30040060, L"DVH Referenced ROI Sequence","SQ");
	registerTag(0x30040062, L"DVH ROI Contribution Type","CS");
	registerTag(0x30040070, L"DVH Minimum Dose","DS");
	registerTag(0x30040072, L"DVH Maximum Dose","DS");
	registerTag(0x30040074, L"DVH Mean Dose","DS");
	registerTag(0x30060000, L"","UL");
	registerTag(0x30060002, L"Structure Set Label","SH");
	registerTag(0x30060004, L"Structure Set Name","LO");
	registerTag(0x30060006, L"Structure Set Description","ST");
	registerTag(0x30060008, L"Structure Set Date","DA");
	registerTag(0x30060009, L"Structure Set Time","TM");
	registerTag(0x30060010, L"Referenced Frame of Reference Sequence","SQ");
	registerTag(0x30060012, L"RT Referenced Study Sequence","SQ");
	registerTag(0x30060014, L"RT Referenced Series Sequence","SQ");
	registerTag(0x30060016, L"Contour Image Sequence","SQ");
	registerTag(0x30060020, L"Structure Set ROI Sequence","SQ");
	registerTag(0x30060022, L"ROI Number","IS");
	registerTag(0x30060024, L"Referenced Frame of Reference","UI");
	registerTag(0x30060026, L"ROI Name","LO");
	registerTag(0x30060028, L"ROI Description","ST");
	registerTag(0x3006002A, L"ROI Display Color","IS");
	registerTag(0x3006002C, L"ROI Volume","DS");
	registerTag(0x30060030, L"RT Related ROI Sequence","SQ");
	registerTag(0x30060033, L"RT ROI Relationship","CS");
	registerTag(0x30060036, L"ROI Generation Algorithm","CS");
	registerTag(0x30060038, L"ROI Generation Description","LO");
	registerTag(0x30060039, L"ROI Contour Sequence","SQ");
	registerTag(0x30060040, L"Contour Sequence","SQ");
	registerTag(0x30060042, L"Contour Geometric Type","CS");
	registerTag(0x30060044, L"Contour Slab Thickness","DS");
	registerTag(0x30060045, L"Contour Offset Vector","DS");
	registerTag(0x30060046, L"Number of Contour Points","IS");
	registerTag(0x30060048, L"Contour Number","IS");
	registerTag(0x30060049, L"Attached Contours","IS");
	registerTag(0x30060050, L"Contour Data","DS");
	registerTag(0x30060080, L"RT ROI Observations Sequence","SQ");
	registerTag(0x30060082, L"Observation Number","IS");
	registerTag(0x30060084, L"Referenced ROI Number","IS");
	registerTag(0x30060085, L"ROI Observation Label","SH");
	registerTag(0x30060086, L"RT ROI Identification Code Sequence","SQ");
	registerTag(0x30060088, L"ROI Observation Description","ST");
	registerTag(0x300600A0, L"Related RT ROI Observations Sequence","SQ");
	registerTag(0x300600A4, L"RT ROI Interpreted Type","CS");
	registerTag(0x300600A6, L"ROI Interpreter","PN");
	registerTag(0x300600B0, L"ROI Physical Properties Sequence","SQ");
	registerTag(0x300600B2, L"ROI Physical Property","CS");
	registerTag(0x300600B4, L"ROI Physical Property Value","DS");
	registerTag(0x300600C0, L"Frame of Reference Relationship Sequence","SQ");
	registerTag(0x300600C2, L"Related Frame of Reference","UI");
	registerTag(0x300600C4, L"Frame of Reference Transformation Type","CS");
	registerTag(0x300600C6, L"Frame of Reference Transformation Matrix","DS");
	registerTag(0x300600C8, L"Frame of Reference Transformation Comment","LO");
	registerTag(0x30080000, L"","UL");
	registerTag(0x30080010, L"Measured Dose Reference Sequence","SQ");
	registerTag(0x30080012, L"Measured Dose Description","ST");
	registerTag(0x30080014, L"Measured Dose Type","CS");
	registerTag(0x30080016, L"Measured Dose Value","DS");
	registerTag(0x30080020, L"Treatment Session Beam Sequence","SQ");
	registerTag(0x30080022, L"Current Fraction Number","IS");
	registerTag(0x30080024, L"Treatment Control Point Date","DA");
	registerTag(0x30080025, L"Treatment Control Point Time","TM");
	registerTag(0x3008002A, L"Treatment Termination Status","CS");
	registerTag(0x3008002B, L"Treatment Termination Code","SH");
	registerTag(0x3008002C, L"Treatment Verification Status","CS");
	registerTag(0x30080030, L"Referenced Treatment Record Sequence","SQ");
	registerTag(0x30080032, L"Specified Primary Meterset","DS");
	registerTag(0x30080033, L"Specified Secondary Meterset","DS");
	registerTag(0x30080036, L"Delivered Primary Meterset","DS");
	registerTag(0x30080037, L"Delivered Secondary Meterset","DS");
	registerTag(0x3008003A, L"Specified Treatment Time","DS");
	registerTag(0x3008003B, L"Delivered Treatment Time","DS");
	registerTag(0x30080040, L"Control Point Delivery Sequence","SQ");
	registerTag(0x30080041, L"Ion Control Point Delivery Sequence","SQ");
	registerTag(0x30080042, L"Specified Meterset","DS");
	registerTag(0x30080044, L"Delivered Meterset","DS");
	registerTag(0x30080045, L"Meterset Rate Set","FL");
	registerTag(0x30080046, L"Meterset Rate Delivered","FL");
	registerTag(0x30080047, L"Scan Spot Metersets Delivered","FL");
	registerTag(0x30080048, L"Dose Rate Delivered","DS");
	registerTag(0x30080050, L"Treatment Summary Calculated Dose Reference Sequence","SQ");
	registerTag(0x30080052, L"Cumulative Dose to Dose Reference","DS");
	registerTag(0x30080054, L"First Treatment Date","DA");
	registerTag(0x30080056, L"Most Recent Treatment Date","DA");
	registerTag(0x3008005A, L"Number of Fractions Delivered","IS");
	registerTag(0x30080060, L"Override Sequence","SQ");
	registerTag(0x30080062, L"Override Parameter Pointer","AT");
	registerTag(0x30080064, L"Measured Dose Reference Number","IS");
	registerTag(0x30080066, L"Override Reason","ST");
	registerTag(0x30080070, L"Calculated Dose Reference Sequence","SQ");
	registerTag(0x30080072, L"Calculated Dose Reference Number","IS");
	registerTag(0x30080074, L"Calculated Dose Reference Description","ST");
	registerTag(0x30080076, L"Calculated Dose Reference Dose Value","DS");
	registerTag(0x30080078, L"Start Meterset","DS");
	registerTag(0x3008007A, L"End Meterset","DS");
	registerTag(0x30080080, L"Referenced Measured Dose Reference Sequence","SQ");
	registerTag(0x30080082, L"Referenced Measured Dose Reference Number","IS");
	registerTag(0x30080090, L"Referenced Calculated Dose Reference Sequence","SQ");
	registerTag(0x30080092, L"Referenced Calculated Dose Reference Number","IS");
	registerTag(0x300800A0, L"Beam Limiting Device Leaf Pairs Sequence","SQ");
	registerTag(0x300800B0, L"Recorded Wedge Sequence","SQ");
	registerTag(0x300800C0, L"Recorded Compensator Sequence","SQ");
	registerTag(0x300800D0, L"Recorded Block Sequence","SQ");
	registerTag(0x300800E0, L"Treatment Summary Measured Dose Reference Sequence","SQ");
	registerTag(0x30080100, L"Recorded Source Sequence","SQ");
	registerTag(0x30080105, L"Source Serial Number","LO");
	registerTag(0x30080110, L"Treatment Session Application Setup Sequence","SQ");
	registerTag(0x30080116, L"Application Setup Check","CS");
	registerTag(0x30080120, L"Recorded Brachy Accessory Device Sequence","SQ");
	registerTag(0x30080122, L"Referenced Brachy Accessory Device Number","IS");
	registerTag(0x30080130, L"Recorded Channel Sequence","SQ");
	registerTag(0x30080132, L"Specified Channel Total Time","DS");
	registerTag(0x30080134, L"Delivered Channel Total Time","DS");
	registerTag(0x30080136, L"Specified Number of Pulses","IS");
	registerTag(0x30080138, L"Delivered Number of Pulses","IS");
	registerTag(0x3008013A, L"Specified Pulse Repetition Interval","DS");
	registerTag(0x3008013C, L"Delivered Pulse Repetition Interval","DS");
	registerTag(0x30080140, L"Recorded Source Applicator Sequence","SQ");
	registerTag(0x30080142, L"Referenced Source Applicator Number","IS");
	registerTag(0x30080150, L"Recorded Channel Shield Sequence","SQ");
	registerTag(0x30080152, L"Referenced Channel Shield Number","IS");
	registerTag(0x30080160, L"Brachy Control Point Delivered Sequence","SQ");
	registerTag(0x30080162, L"Safe Position Exit Date","DA");
	registerTag(0x30080164, L"Safe Position Exit Time","TM");
	registerTag(0x30080166, L"Safe Position Return Date","DA");
	registerTag(0x30080168, L"Safe Position Return Time","TM");
	registerTag(0x30080200, L"Current Treatment Status","CS");
	registerTag(0x30080202, L"Treatment Status Comment","ST");
	registerTag(0x30080220, L"Fraction Group Summary Sequence","SQ");
	registerTag(0x30080223, L"Referenced Fraction Number","IS");
	registerTag(0x30080224, L"Fraction Group Type","CS");
	registerTag(0x30080230, L"Beam Stopper Position","CS");
	registerTag(0x30080240, L"Fraction Status Summary Sequence","SQ");
	registerTag(0x30080250, L"Treatment Date","DA");
	registerTag(0x30080251, L"Treatment Time","TM");
	registerTag(0x300A0000, L"","UL");
	registerTag(0x300A0002, L"RT Plan Label","SH");
	registerTag(0x300A0003, L"RT Plan Name","LO");
	registerTag(0x300A0004, L"RT Plan Description","ST");
	registerTag(0x300A0006, L"RT Plan Date","DA");
	registerTag(0x300A0007, L"RT Plan Time","TM");
	registerTag(0x300A0009, L"Treatment Protocols","LO");
	registerTag(0x300A000A, L"Treatment Intent","CS");
	registerTag(0x300A000B, L"Treatment Sites","LO");
	registerTag(0x300A000C, L"RT Plan Geometry","CS");
	registerTag(0x300A000E, L"Prescription Description","ST");
	registerTag(0x300A0010, L"Dose Reference Sequence","SQ");
	registerTag(0x300A0012, L"Dose Reference Number","IS");
	registerTag(0x300A0013, L"Dose Reference UID","UI");
	registerTag(0x300A0014, L"Dose Reference Structure Type","CS");
	registerTag(0x300A0015, L"Nominal Beam Energy Unit","CS");
	registerTag(0x300A0016, L"Dose Reference Description","LO");
	registerTag(0x300A0018, L"Dose Reference Point Coordinates","DS");
	registerTag(0x300A001A, L"Nominal Prior Dose","DS");
	registerTag(0x300A0020, L"Dose Reference Type","CS");
	registerTag(0x300A0021, L"Constraint Weight","DS");
	registerTag(0x300A0022, L"Delivery Warning Dose","DS");
	registerTag(0x300A0023, L"Delivery Maximum Dose","DS");
	registerTag(0x300A0025, L"Target Minimum Dose","DS");
	registerTag(0x300A0026, L"Target Prescription Dose","DS");
	registerTag(0x300A0027, L"Target Maximum Dose","DS");
	registerTag(0x300A0028, L"Target Underdose Volume Fraction","DS");
	registerTag(0x300A002A, L"Organ at Risk Full-volume Dose","DS");
	registerTag(0x300A002B, L"Organ at Risk Limit Dose","DS");
	registerTag(0x300A002C, L"Organ at Risk Maximum Dose","DS");
	registerTag(0x300A002D, L"Organ at Risk Overdose Volume Fraction","DS");
	registerTag(0x300A0040, L"Tolerance Table Sequence","SQ");
	registerTag(0x300A0042, L"Tolerance Table Number","IS");
	registerTag(0x300A0043, L"Tolerance Table Label","SH");
	registerTag(0x300A0044, L"Gantry Angle Tolerance","DS");
	registerTag(0x300A0046, L"Beam Limiting Device Angle Tolerance","DS");
	registerTag(0x300A0048, L"Beam Limiting Device Tolerance Sequence","SQ");
	registerTag(0x300A004A, L"Beam Limiting Device Position Tolerance","DS");
	registerTag(0x300A004C, L"Patient Support Angle Tolerance","DS");
	registerTag(0x300A004E, L"Table Top Eccentric Angle Tolerance","DS");
	registerTag(0x300A004F, L"Table Top Pitch Angle Tolerance","FL");
	registerTag(0x300A0050, L"Table Top Roll Angle Tolerance","FL");
	registerTag(0x300A0051, L"Table Top Vertical Position Tolerance","DS");
	registerTag(0x300A0052, L"Table Top Longitudinal Position Tolerance","DS");
	registerTag(0x300A0053, L"Table Top Lateral Position Tolerance","DS");
	registerTag(0x300A0055, L"RT Plan Relationship","CS");
	registerTag(0x300A0070, L"Fraction Group Sequence","SQ");
	registerTag(0x300A0071, L"Fraction Group Number","IS");
	registerTag(0x300A0072, L"Fraction Group Description","LO");
	registerTag(0x300A0078, L"Number of Fractions Planned","IS");
	registerTag(0x300A0079, L"Number of Fraction Pattern Digits Per Day","IS");
	registerTag(0x300A007A, L"Repeat Fraction Cycle Length","IS");
	registerTag(0x300A007B, L"Fraction Pattern","LT");
	registerTag(0x300A0080, L"Number of Beams","IS");
	registerTag(0x300A0082, L"Beam Dose Specification Point","DS");
	registerTag(0x300A0084, L"Beam Dose","DS");
	registerTag(0x300A0086, L"Beam Meterset","DS");
	registerTag(0x300A0088, L"Beam Dose Point Depth","FL");
	registerTag(0x300A0089, L"Beam Dose Point Equivalent Depth","FL");
	registerTag(0x300A008A, L"Beam Dose Point SSD","FL");
	registerTag(0x300A00A0, L"Number of Brachy Application Setups","IS");
	registerTag(0x300A00A2, L"Brachy Application Setup Dose Specification Point","DS");
	registerTag(0x300A00A4, L"Brachy Application Setup Dose","DS");
	registerTag(0x300A00B0, L"Beam Sequence","SQ");
	registerTag(0x300A00B2, L"Treatment Machine Name","SH");
	registerTag(0x300A00B3, L"Primary Dosimeter Unit","CS");
	registerTag(0x300A00B4, L"Source-Axis Distance","DS");
	registerTag(0x300A00B6, L"Beam Limiting Device Sequence","SQ");
	registerTag(0x300A00B8, L"RT Beam Limiting Device Type","CS");
	registerTag(0x300A00BA, L"Source to Beam Limiting Device Distance","DS");
	registerTag(0x300A00BB, L"Isocenter to Beam Limiting Device Distance","FL");
	registerTag(0x300A00BC, L"Number of Leaf/Jaw Pairs","IS");
	registerTag(0x300A00BE, L"Leaf Position Boundaries","DS");
	registerTag(0x300A00C0, L"Beam Number","IS");
	registerTag(0x300A00C2, L"Beam Name","LO");
	registerTag(0x300A00C3, L"Beam Description","ST");
	registerTag(0x300A00C4, L"Beam Type","CS");
	registerTag(0x300A00C6, L"Radiation Type","CS");
	registerTag(0x300A00C7, L"High-Dose Technique Type","CS");
	registerTag(0x300A00C8, L"Reference Image Number","IS");
	registerTag(0x300A00CA, L"Planned Verification Image Sequence","SQ");
	registerTag(0x300A00CC, L"Imaging Device-Specific Acquisition Parameters","LO");
	registerTag(0x300A00CE, L"Treatment Delivery Type","CS");
	registerTag(0x300A00D0, L"Number of Wedges","IS");
	registerTag(0x300A00D1, L"Wedge Sequence","SQ");
	registerTag(0x300A00D2, L"Wedge Number","IS");
	registerTag(0x300A00D3, L"Wedge Type","CS");
	registerTag(0x300A00D4, L"Wedge","SH");
	registerTag(0x300A00D5, L"Wedge Angle","IS");
	registerTag(0x300A00D6, L"Wedge Factor","DS");
	registerTag(0x300A00D7, L"Total Wedge Tray Water-Equivalent Thickness","FL");
	registerTag(0x300A00D8, L"Wedge Orientation","DS");
	registerTag(0x300A00D9, L"Isocenter to Wedge Tray Distance","FL");
	registerTag(0x300A00DA, L"Source to Wedge Tray Distance","DS");
	registerTag(0x300A00DB, L"Wedge Thin Edge Position","DS");
	registerTag(0x300A00DC, L"Bolus ID","SH");
	registerTag(0x300A00DD, L"Bolus Description","ST");
	registerTag(0x300A00E0, L"Number of Compensators","IS");
	registerTag(0x300A00E1, L"Material","SH");
	registerTag(0x300A00E2, L"Total Compensator Tray Factor","DS");
	registerTag(0x300A00E3, L"Compensator Sequence","SQ");
	registerTag(0x300A00E4, L"Compensator Number","IS");
	registerTag(0x300A00E5, L"Compensator IS","SH");
	registerTag(0x300A00E6, L"Source to Compensator Tray Distance","DS");
	registerTag(0x300A00E7, L"Compensator Rows","IS");
	registerTag(0x300A00E8, L"Compensator Columns","IS");
	registerTag(0x300A00E9, L"Compensator Pixel Spacing","DS");
	registerTag(0x300A00EA, L"Compensator Position","DS");
	registerTag(0x300A00EB, L"Compensator Transmission Data","DS");
	registerTag(0x300A00EC, L"Compensator Thickness Data","DS");
	registerTag(0x300A00ED, L"Number of Boli","IS");
	registerTag(0x300A00EE, L"Compensator Type","CS");
	registerTag(0x300A00F0, L"Number of Blocks","IS");
	registerTag(0x300A00F2, L"Total Block Tray Factor","DS");
	registerTag(0x300A00F3, L"Total Block Tray Water-Equivalent Thickness","FL");
	registerTag(0x300A00F4, L"Block Sequence","SQ");
	registerTag(0x300A00F5, L"Block Tray ID","SH");
	registerTag(0x300A00F6, L"Source to Block Tray Distance","DS");
	registerTag(0x300A00F7, L"Isocenter to Block Tray Distance","FL");
	registerTag(0x300A00F8, L"Block Type","CS");
	registerTag(0x300A00FA, L"Block Divergence","CS");
	registerTag(0x300A00FB, L"Block Mounting Position","CS");
	registerTag(0x300A00FC, L"Block Number","IS");
	registerTag(0x300A00FE, L"Block Name","LO");
	registerTag(0x300A0100, L"Block Thickness","DS");
	registerTag(0x300A0102, L"Block Transmission","DS");
	registerTag(0x300A0104, L"Block Number of Points","IS");
	registerTag(0x300A0106, L"Block Data","DS");
	registerTag(0x300A0107, L"Applicator Sequence","SQ");
	registerTag(0x300A0108, L"Applicator ID","SH");
	registerTag(0x300A0109, L"Applicator Type","CS");
	registerTag(0x300A010A, L"Applicator Description","LO");
	registerTag(0x300A010C, L"Cumulative Dose Reference Coefficient","DS");
	registerTag(0x300A010E, L"Final Cumulative Meterset Weight","DS");
	registerTag(0x300A0110, L"Number of Control Points","IS");
	registerTag(0x300A0111, L"Control Point Sequence","SQ");
	registerTag(0x300A0112, L"Control Point Index","IS");
	registerTag(0x300A0114, L"Nominal Beam Energy","DS");
	registerTag(0x300A0115, L"Dose Rate Set","DS");
	registerTag(0x300A0116, L"Wedge Position Sequence","SQ");
	registerTag(0x300A0118, L"Wedge Position","CS");
	registerTag(0x300A011A, L"Beam Limiting Device Position Sequence","SQ");
	registerTag(0x300A011C, L"Leaf/Jaw Positions","DS");
	registerTag(0x300A011E, L"Gantry Angle","DS");
	registerTag(0x300A011F, L"Gantry Rotation Direction","CS");
	registerTag(0x300A0120, L"Beam Limiting Device Angle","DS");
	registerTag(0x300A0121, L"Beam Limiting Device Rotation Direction","CS");
	registerTag(0x300A0122, L"Patient Support Angle","DS");
	registerTag(0x300A0123, L"Patient Support Rotation Direction","CS");
	registerTag(0x300A0124, L"Table Top Eccentric Axis Distance","DS");
	registerTag(0x300A0125, L"Table Top Eccentric Angle","DS");
	registerTag(0x300A0126, L"Table Top Eccentric Rotation Direction","CS");
	registerTag(0x300A0128, L"Table Top Vertical Position","DS");
	registerTag(0x300A0129, L"Table Top Longitudinal Position","DS");
	registerTag(0x300A012A, L"Table Top Lateral Position","DS");
	registerTag(0x300A012C, L"Isocenter Position","DS");
	registerTag(0x300A012E, L"Surface Entry Point","DS");
	registerTag(0x300A0130, L"Source to Surface Distance","DS");
	registerTag(0x300A0134, L"Cumulative Meterset Weight","DS");
	registerTag(0x300A0140, L"Table Top Pitch Angle","FL");
	registerTag(0x300A0142, L"Table Top Pitch Rotation Direction","CS");
	registerTag(0x300A0144, L"Table Top Roll Angle","FL");
	registerTag(0x300A0146, L"Table Top Roll Rotation Direction","CS");
	registerTag(0x300A0148, L"Head Fixation Angle","FL");
	registerTag(0x300A014A, L"Gantry Pitch Angle","FL");
	registerTag(0x300A014C, L"Gantry Pitch Rotation Direction","CS");
	registerTag(0x300A014E, L"Gantry Pitch Angle Tolerance","FL");
	registerTag(0x300A0180, L"Patient Setup Sequence","SQ");
	registerTag(0x300A0182, L"Patient Setup Number","IS");
	registerTag(0x300A0183, L"Patient Setup Label","LO");
	registerTag(0x300A0184, L"Patient Additional Position","LO");
	registerTag(0x300A0190, L"Fixation Device Sequence","SQ");
	registerTag(0x300A0192, L"Fixation Device Type","CS");
	registerTag(0x300A0194, L"Fixation Device Label","SH");
	registerTag(0x300A0196, L"Fixation Device Description","ST");
	registerTag(0x300A0198, L"Fixation Device Position","SH");
	registerTag(0x300A0199, L"Fixation Device Pitch Angle","FL");
	registerTag(0x300A019A, L"Fixation Device Roll Angle","FL");
	registerTag(0x300A01A0, L"Shielding Device Sequence","SQ");
	registerTag(0x300A01A2, L"Shielding Device Type","CS");
	registerTag(0x300A01A4, L"Shielding Device Label","SH");
	registerTag(0x300A01A6, L"Shielding Device Description","ST");
	registerTag(0x300A01A8, L"Shielding Device Position","SH");
	registerTag(0x300A01B0, L"Setup Technique","CS");
	registerTag(0x300A01B2, L"Setup Technique Description","ST");
	registerTag(0x300A01B4, L"Setup Device Sequence","SQ");
	registerTag(0x300A01B6, L"Setup Device Type","CS");
	registerTag(0x300A01B8, L"Setup Device Label","SH");
	registerTag(0x300A01BA, L"Setup Device Description","ST");
	registerTag(0x300A01BC, L"Setup Device Parameter","DS");
	registerTag(0x300A01D0, L"Setup Reference Description","ST");
	registerTag(0x300A01D2, L"Table Top Vertical Setup Displacement","DS");
	registerTag(0x300A01D4, L"Table Top Longitudinal Setup Displacement","DS");
	registerTag(0x300A01D6, L"Table Top Lateral Setup Displacement","DS");
	registerTag(0x300A0200, L"Brachy Treatment Technique","CS");
	registerTag(0x300A0202, L"Brachy Treatment Type","CS");
	registerTag(0x300A0206, L"Treatment Machine Sequence","SQ");
	registerTag(0x300A0210, L"Source Sequence","SQ");
	registerTag(0x300A0212, L"Source Number","IS");
	registerTag(0x300A0214, L"Source Type","CS");
	registerTag(0x300A0216, L"Source Manufacturer","LO");
	registerTag(0x300A0218, L"Active Source Diameter","DS");
	registerTag(0x300A021A, L"Active Source Length","DS");
	registerTag(0x300A0222, L"Source Encapsulation Nominal Thickness","DS");
	registerTag(0x300A0224, L"Source Encapsulation Nominal Transmission","DS");
	registerTag(0x300A0226, L"Source Isotope Name","LO");
	registerTag(0x300A0228, L"Source Isotope Half Life","DS");
	registerTag(0x300A022A, L"Reference Air Kerma Rate","DS");
	registerTag(0x300A022B, L"Source Strength","DS");
	registerTag(0x300A022C, L"Air Kerma Rate Reference Date","DA");
	registerTag(0x300A022E, L"Air Kerma Rate Reference Time","TM");
	registerTag(0x300A0230, L"Application Setup Sequence","SQ");
	registerTag(0x300A0232, L"Application Setup Type","CS");
	registerTag(0x300A0234, L"Application Setup Number","IS");
	registerTag(0x300A0236, L"Application Setup Name","LO");
	registerTag(0x300A0238, L"Application Setup Manufacturer","LO");
	registerTag(0x300A0240, L"Template Number","IS");
	registerTag(0x300A0242, L"Template Type","SH");
	registerTag(0x300A0244, L"Template Name","LO");
	registerTag(0x300A0250, L"Total Reference Air Kerma","DS");
	registerTag(0x300A0260, L"Brachy Accessory Device Sequence","SQ");
	registerTag(0x300A0262, L"Brachy Accessory Device Number","IS");
	registerTag(0x300A0263, L"Brachy Accessory Device","SH");
	registerTag(0x300A0264, L"Brachy Accessory Device Type","CS");
	registerTag(0x300A0266, L"Brachy Accessory Device Name","LO");
	registerTag(0x300A026A, L"Brachy Accessory Device Nominal Thickness","DS");
	registerTag(0x300A026C, L"Brachy Accessory Device Nominal Transmission","DS");
	registerTag(0x300A0280, L"Channel Sequence","SQ");
	registerTag(0x300A0282, L"Channel Number","IS");
	registerTag(0x300A0284, L"Channel Length","DS");
	registerTag(0x300A0286, L"Channel Total Time","DS");
	registerTag(0x300A0288, L"Source Movement Type","CS");
	registerTag(0x300A028A, L"Number of Pulses","IS");
	registerTag(0x300A028C, L"Pulse Repetition Interval","DS");
	registerTag(0x300A0290, L"Source Applicator Number","DS");
	registerTag(0x300A0291, L"Source Applicator","SH");
	registerTag(0x300A0292, L"Source Applicator Type","CS");
	registerTag(0x300A0294, L"Source Applicator Name","LO");
	registerTag(0x300A0296, L"Source Applicator Length","DS");
	registerTag(0x300A0298, L"Source Applicator Manufacturer","LO");
	registerTag(0x300A029C, L"Source Applicator Wall Nominal Thickness","DS");
	registerTag(0x300A029E, L"Source Applicator Wall Nominal Transmission","DS");
	registerTag(0x300A02A0, L"Source Applicator Step Size","DS");
	registerTag(0x300A02A2, L"Transfer Tube Number","IS");
	registerTag(0x300A02A4, L"Transfer Tube Length","DS");
	registerTag(0x300A02B0, L"Channel Shield Sequence","SQ");
	registerTag(0x300A02B2, L"Channel Shield Number","IS");
	registerTag(0x300A02B3, L"Channel Shield","SH");
	registerTag(0x300A02B4, L"Channel Shield Name","LO");
	registerTag(0x300A02B8, L"Channel Shield Nominal Thickness","DS");
	registerTag(0x300A02BA, L"Channel Shield Nominal Transmission","DS");
	registerTag(0x300A02C8, L"Final Cumulative Time Weight","DS");
	registerTag(0x300A02D0, L"Brachy Control Point Sequence","SQ");
	registerTag(0x300A02D2, L"Control Point Relative Position","DS");
	registerTag(0x300A02D4, L"Control Point,Position","DS");
	registerTag(0x300A02D6, L"Cumulative Time Weight","DS");
	registerTag(0x300A02E0, L"Compensator Divergence","CS");
	registerTag(0x300A02E1, L"Compensator Mounting Position","CS");
	registerTag(0x300A02E2, L"Source to Compensator Distance","DS");
	registerTag(0x300A02E3, L"Total Compensator Tray Water-Equivalent Thickness","FL");
	registerTag(0x300A02E4, L"Isocenter to Compensator Tray Distance","FL");
	registerTag(0x300A02E5, L"Compensator Column Offset","FL");
	registerTag(0x300A02E6, L"Isocenter to Compensator Distances","FL");
	registerTag(0x300A02E7, L"Compensator Relative Stopping Power Ratio","FL");
	registerTag(0x300A02E8, L"Compensator Milling Tool Diameter","FL");
	registerTag(0x300A02EA, L"Ion Range Compensator Sequence","SQ");
	registerTag(0x300A02EB, L"Compensator Description","LT");
	registerTag(0x300A0302, L"Radiation Mass Number","IS");
	registerTag(0x300A0304, L"Radiation Atomic Number","IS");
	registerTag(0x300A0306, L"Radiation Charge State","SS");
	registerTag(0x300A0308, L"Scan Mode","CS");
	registerTag(0x300A030A, L"Virtual Source-Axis Distances","FL");
	registerTag(0x300A030C, L"Snout Sequence","SQ");
	registerTag(0x300A030D, L"Snout Position","FL");
	registerTag(0x300A030F, L"Snout ID","SH");
	registerTag(0x300A0312, L"Number of Range Shifters","IS");
	registerTag(0x300A0314, L"Range Shifter Sequence","SQ");
	registerTag(0x300A0316, L"Range Shifter Number","IS");
	registerTag(0x300A0318, L"Range Shifter ID","SH");
	registerTag(0x300A0320, L"Range Shifter Type","CS");
	registerTag(0x300A0322, L"Range Shifter Description","LO");
	registerTag(0x300A0330, L"Number of Lateral Spreading Devices","IS");
	registerTag(0x300A0332, L"Lateral Spreading Device Sequence","SQ");
	registerTag(0x300A0334, L"Lateral Spreading Device Number","IS");
	registerTag(0x300A0336, L"Lateral Spreading Device ID","SH");
	registerTag(0x300A0338, L"Lateral Spreading Device Type","CS");
	registerTag(0x300A033A, L"Lateral Spreading Device Description","LO");
	registerTag(0x300A033C, L"Lateral Spreading Device Water Equivalent Thickness","FL");
	registerTag(0x300A0340, L"Number of Range Modulators","IS");
	registerTag(0x300A0342, L"Range Modulator Sequence","SQ");
	registerTag(0x300A0344, L"Range Modulator Number","IS");
	registerTag(0x300A0346, L"Range Modulator ID","SH");
	registerTag(0x300A0348, L"Range Modulator Type","CS");
	registerTag(0x300A034A, L"Range Modulator Description","LO");
	registerTag(0x300A034C, L"Beam Current Modulation ID","SH");
	registerTag(0x300A0350, L"Patient Support Type","CS");
	registerTag(0x300A0352, L"Patient Support ID","SH");
	registerTag(0x300A0354, L"Patient Support Accessory Code","LO");
	registerTag(0x300A0356, L"Fixation Light Azimuthal Angle","FL");
	registerTag(0x300A0358, L"Fixation Light Polar Angle","FL");
	registerTag(0x300A035A, L"Meterset Rate","FL");
	registerTag(0x300A0360, L"Range Shifter Settings Sequence","SQ");
	registerTag(0x300A0362, L"Range Shifter Setting","LO");
	registerTag(0x300A0364, L"Isocenter to Range Shifter Distance","FL");
	registerTag(0x300A0366, L"Range Shifter Water Equivalent Thickness","FL");
	registerTag(0x300A0370, L"Lateral Spreading Device Settings Sequence","SQ");
	registerTag(0x300A0372, L"Lateral Spreading Device Setting","LO");
	registerTag(0x300A0374, L"Isocenter to Lateral Spreading Device Distance","FL");
	registerTag(0x300A0380, L"Range Modulator Settings Sequence","SQ");
	registerTag(0x300A0382, L"Range Modulator Gating Start Value","FL");
	registerTag(0x300A0384, L"Range Modulator Gating Stop Value","FL");
	registerTag(0x300A0386, L"Range Modulator Gating Start Water Equivalent Thickness","FL");
	registerTag(0x300A0388, L"Range Modulator Gating Stop Water Equivalent Thickness","FL");
	registerTag(0x300A038A, L"Isocenter to Range Modulator Distance","FL");
	registerTag(0x300A0390, L"Scan Spot Tune ID","SH");
	registerTag(0x300A0392, L"Number of Scan Spot Positions","IS");
	registerTag(0x300A0394, L"Scan Spot Position Map","FL");
	registerTag(0x300A0396, L"Scan Spot Meterset Weights","FL");
	registerTag(0x300A0398, L"Scanning Spot Size","FL");
	registerTag(0x300A039A, L"Number of Paintings","IS");
	registerTag(0x300A03A0, L"Ion Tolerance Table Sequence","SQ");
	registerTag(0x300A03A2, L"Ion Beam Sequence","SQ");
	registerTag(0x300A03A4, L"Ion Beam Limiting Device Sequence","SQ");
	registerTag(0x300A03A6, L"Ion Block Sequence","SQ");
	registerTag(0x300A03A8, L"Ion Control Point Sequence","SQ");
	registerTag(0x300A03AA, L"Ion Wedge Sequence","SQ");
	registerTag(0x300A03AC, L"Ion Wedge Position Sequence","SQ");
	registerTag(0x300A03A8, L"Ion Wedge Position Sequence","SQ");
	registerTag(0x300A0401, L"Referenced Setup Image Sequence","SQ");
	registerTag(0x300A0402, L"Setup Image Comment","ST");
	registerTag(0x300A0410, L"Motion Synchronization Sequence","SQ");
	registerTag(0x300A0412, L"Control Point Orientation","FL");
	registerTag(0x300A0420, L"General Accessory Sequence","SQ");
	registerTag(0x300A0421, L"General Accessory ID","SH");
	registerTag(0x300A0422, L"General Accessory Description","ST");
	registerTag(0x300A0423, L"General Accessory Type","CS");
	registerTag(0x300A0424, L"General Accessory Number","IS");
	registerTag(0x300C0000, L"","UL");
	registerTag(0x300C0002, L"Referenced RT Plan Sequence","SQ");
	registerTag(0x300C0004, L"Referenced Beam Sequence","SQ");
	registerTag(0x300C0006, L"Referenced Beam Number","IS");
	registerTag(0x300C0007, L"Referenced Reference Image Number","IS");
	registerTag(0x300C0008, L"Start Cumulative Meterset Weight","DS");
	registerTag(0x300C0009, L"End Cumulative Meterset Weight","DS");
	registerTag(0x300C000A, L"Referenced Brachy Application Setup Sequence","SQ");
	registerTag(0x300C000C, L"Referenced Brachy Application Setup Number","IS");
	registerTag(0x300C000E, L"Referenced Source Number","IS");
	registerTag(0x300C0020, L"Referenced Fraction Group Sequence","SQ");
	registerTag(0x300C0022, L"Referenced Fraction Group Number","IS");
	registerTag(0x300C0040, L"Referenced Verification Image Sequence","SQ");
	registerTag(0x300C0042, L"Referenced Reference Image Sequence","SQ");
	registerTag(0x300C0050, L"Referenced Dose Reference Sequence","SQ");
	registerTag(0x300C0051, L"Referenced Dose Reference Number","IS");
	registerTag(0x300C0055, L"Brachy Referenced Dose Reference Sequence","SQ");
	registerTag(0x300C0060, L"Referenced Structure Set Sequence","SQ");
	registerTag(0x300C006A, L"Referenced Patient Setup Number","IS");
	registerTag(0x300C0080, L"Referenced Dose Sequence","SQ");
	registerTag(0x300C00A0, L"Referenced Tolerance Table Number","IS");
	registerTag(0x300C00B0, L"Referenced Bolus Sequence","SQ");
	registerTag(0x300C00C0, L"Referenced Wedge Number","IS");
	registerTag(0x300C00D0, L"Referenced Compensator Number","IS");
	registerTag(0x300C00E0, L"Referenced Block Number","IS");
	registerTag(0x300C00F0, L"Referenced Control Point Index","IS");
	registerTag(0x300E0000, L"","UL");
	registerTag(0x300E0002, L"Approval Status","CS");
	registerTag(0x300E0004, L"Review Date","DA");
	registerTag(0x300E0005, L"Review Time","TM");
	registerTag(0x300E0008, L"Reviewer Name","PN");
	registerTag(0x40000000, L"Text","");
	registerTag(0x40000010, L"Arbitrary","");
	registerTag(0x40004000, L"Comments","");
	registerTag(0x40080000, L"","UL");
	registerTag(0x40080040, L"Results","SH");
	registerTag(0x40080042, L"Results Issuer","LO");
	registerTag(0x40080050, L"Referenced Interpretation Sequence","SQ");
	registerTag(0x40080100, L"Interpretation Recorded Date","DA");
	registerTag(0x40080101, L"Interpretation Recorded Time","TM");
	registerTag(0x40080102, L"Interpretation Recorder","PN");
	registerTag(0x40080103, L"Reference to Recorded Sound","LO");
	registerTag(0x40080108, L"Interpretation Transcription Date","DA");
	registerTag(0x40080109, L"Interpretation Transcription Time","TM");
	registerTag(0x4008010A, L"Interpretation Transcriber","PN");
	registerTag(0x4008010B, L"Interpretation Text","ST");
	registerTag(0x4008010C, L"Interpretation Author","PN");
	registerTag(0x40080111, L"Interpretation Approver Sequence","SQ");
	registerTag(0x40080112, L"Interpretation Approval Date","DA");
	registerTag(0x40080113, L"Interpretation Approval Time","TM");
	registerTag(0x40080114, L"Physician Approving Interpretation","PN");
	registerTag(0x40080115, L"Interpretation Diagnosis Description","LT");
	registerTag(0x40080117, L"Interpretation Diagnosis Code Sequence","SQ");
	registerTag(0x40080118, L"Results Distribution List Sequence","SQ");
	registerTag(0x40080119, L"Distribution Name","PN");
	registerTag(0x4008011A, L"Distribution Address","LO");
	registerTag(0x40080200, L"Interpretation","SH");
	registerTag(0x40080202, L"Interpretation Issuer","LO");
	registerTag(0x40080210, L"Interpretation Type","CS");
	registerTag(0x40080212, L"Interpretation Status","CS");
	registerTag(0x40080300, L"Impressions","ST");
	registerTag(0x40084000, L"Results Comments","ST");
	registerTag(0x4FFE0001, L"MAC Parameters Sequence","SQ");
	registerTag(0x54000000, L"Waveform data elements","UL");
	registerTag(0x54000100, L"Waveform Sequence","SQ");
	registerTag(0x54000110, L"Channel Minimum Value","OW");
	registerTag(0x54000112, L"Channel Maximum Value","OW");
	registerTag(0x54001004, L"Waveform Bits Allocated","US");
	registerTag(0x54001006, L"Waveform Sample Interpretation","CS");
	registerTag(0x5400100A, L"Waveform Padding Value","OW");
	registerTag(0x54001010, L"Waveform Data","OW");
	registerTag(0x7FE00000, L"Pixel data elements","UL");
	registerTag(0x7FE00010, L"Pixel Data","OW");
	registerTag(0xFFFAFFFA, L"Digital Signatures Sequence","OB");
	registerTag(0xFFFCFFFC, L"Data Set Trailing Padding","OB");
	registerTag(0xFFFEE000, L"Item","OB");
	registerTag(0xFFFEE00D, L"Item Delimitation Item","OB");
	registerTag(0xFFFEE0DD, L"Sequence Delimitation Item","OB");

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Register a tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomDictionary::registerTag(imbxUint32 tagId, const wchar_t* tagName, const char* tagType)
{
	PUNTOEXE_FUNCTION_START(L"dicomDictionary::registerTag");

	if(m_dicomDict.find(tagId) != m_dicomDict.end())
	{
		return;
	}
	imageDataDictionaryElement newElement;

	newElement.m_tagName = tagName;
	newElement.m_tagType = tagType;

	m_dicomDict[tagId] = newElement;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Register a VR
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomDictionary::registerVR(std::string vr, bool bLongLength, imbxUint32 wordSize, imbxUint32 maxLength)
{
	PUNTOEXE_FUNCTION_START(L"dicomDictionary::registerVR");

	if(m_vrDict.find(vr) != m_vrDict.end())
	{
		return;
	}
	validDataTypesStruct newElement;
	newElement.m_longLength = bLongLength;
	newElement.m_wordLength = wordSize;
	newElement.m_maxLength = maxLength;

	m_vrDict[vr] = newElement;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return an human readable name for the tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
std::wstring dicomDictionary::getTagName(imbxUint16 groupId, imbxUint16 tagId) const
{
	PUNTOEXE_FUNCTION_START(L"dicomDictionary::getTagName");

	imbxUint32 tagDWordId=(((imbxUint32)groupId)<<16) | (imbxUint32)tagId;

	tDicomDictionary::const_iterator findIterator = m_dicomDict.find(tagDWordId);
	if(findIterator == m_dicomDict.end())
	{
		return L"";
	}
	
	return findIterator->second.m_tagName;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return the default type for the specified tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
std::string dicomDictionary::getTagType(imbxUint16 groupId, imbxUint16 tagId) const
{
	PUNTOEXE_FUNCTION_START(L"dicomDictionary::getTagType");

	imbxUint32 tagDWordId=(((imbxUint32)groupId)<<16) | (imbxUint32)tagId;

	tDicomDictionary::const_iterator findIterator = m_dicomDict.find(tagDWordId);
	if(findIterator == m_dicomDict.end())
	{
		return "";
	}

	return findIterator->second.m_tagType;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return true if the specified data type is valid
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool dicomDictionary::isDataTypeValid(std::string dataType) const
{
	PUNTOEXE_FUNCTION_START(L"dicomDictionary::isDataTypeValid");

	tVRDictionary::const_iterator findIterator = m_vrDict.find(dataType);

	return (findIterator != m_vrDict.end());

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return true if the specified data type must use a 
//  long length descriptor
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool dicomDictionary::getLongLength(std::string dataType) const
{
	PUNTOEXE_FUNCTION_START(L"dicomDictionary::getLongLength");

	tVRDictionary::const_iterator findIterator = m_vrDict.find(dataType);

	if(findIterator == m_vrDict.end())
	{
		return false;
	}

	return findIterator->second.m_longLength;

	PUNTOEXE_FUNCTION_END();
	
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return the word size for the specified data type
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dicomDictionary::getWordSize(std::string dataType) const
{
	PUNTOEXE_FUNCTION_START(L"dicomDictionary::getWordSize");

	tVRDictionary::const_iterator findIterator = m_vrDict.find(dataType);

	if(findIterator == m_vrDict.end())
	{
		return 0;
	}

	return findIterator->second.m_wordLength;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return the max size in bytes for the specified data
//  type
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dicomDictionary::getMaxSize(std::string dataType) const
{
	PUNTOEXE_FUNCTION_START(L"dicomDictionary::getMaxSize");

	tVRDictionary::const_iterator findIterator = m_vrDict.find(dataType);

	if(findIterator == m_vrDict.end())
	{
		return 0;
	}

	return findIterator->second.m_maxLength;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return a pointer to the unique instance of
//  dicomDictionary
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
dicomDictionary* dicomDictionary::getDicomDictionary()
{
	static dicomDictionary m_imbxDicomDictionary;
	return &m_imbxDicomDictionary;
}


} // namespace imebra

} // namespace puntoexe
