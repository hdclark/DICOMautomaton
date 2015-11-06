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

/*! \file dicomDir.h
    \brief Declaration of the classes that parse/create a DICOMDIR
	        structure (dicomDir and directoryRecord).

*/

#if !defined(imebraDicomDir_93F684BF_0024_4bf3_89BA_D98E82A1F44C__INCLUDED_)
#define imebraDicomDir_93F684BF_0024_4bf3_89BA_D98E82A1F44C__INCLUDED_

#include "../../base/include/baseObject.h"
#include <string>

///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe::imebra
//
///////////////////////////////////////////////////////////
namespace puntoexe
{

namespace imebra
{


class dataSet; // Used in this class
class dicomDir;

/// \addtogroup group_dicomdir Dicomdir
///
/// @{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief directoryRecord represents a single record in
///         a DICOMDIR structure (see dicomDir).
///
/// A new directoryRecord object cannot be constructed 
///  directly but only by calling dicomDir::getNewRecord() 
///  on the directory the new record will belong to.
///
/// 
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class directoryRecord: public baseObject
{
	friend class dicomDir;
public:

	/// \brief Specifies the item's type.
	///
	///////////////////////////////////////////////////////////
	enum tDirectoryRecordType
	{
		patient,
		study,
		series,
		image,
		overlay,
		modality_lut,
		voi_lut,
		curve,
		topic,
		visit,
		results,
		interpretation,
		study_component,
        stored_print,
		rt_dose,
		rt_structure_set,
		rt_plan,
		rt_treat_record,
		presentation,
		waveform,
		sr_document,
		key_object_doc,
		spectroscopy,
		raw_data,
		registration,
		fiducial,
		mrdr,
		endOfDirectoryRecordTypes
	};

public:
	/// \brief Returns the dataSet that contains the
	///         record's information.
	///
	/// The record's dataSet is embedded in a sequence in the
	///  DICOMDIR dataset.
	///
	/// @return the dataSet that contains the record's 
	///          information
	///
	///////////////////////////////////////////////////////////
	ptr<dataSet> getRecordDataSet();

	/// \brief Returns the next sibling record.
	///
	/// @return the next sibling record, or 0 if this is the
	///          last record
	///
	///////////////////////////////////////////////////////////
	ptr<directoryRecord> getNextRecord();

	/// \brief Returns the first child record.
	///
	/// @return the first child record, or 0 if the record 
	///          doesn't have any child
	///
	///////////////////////////////////////////////////////////
	ptr<directoryRecord> getFirstChildRecord();
	
	/// \brief Returns the referenced record, if any.
	///
	/// @return the referenced record, or 0 if the record
	///          doesn't reference any other record
	///
	///////////////////////////////////////////////////////////
	ptr<directoryRecord> getReferencedRecord();

	/// \brief Sets the next sibling record.
	///
	/// The new sibling record takes the place of the old
	///  next sibling record, if it was already set.
	///
	/// @param pNextRecord    the next sibling record
	///
	///////////////////////////////////////////////////////////
	void setNextRecord(ptr<directoryRecord> pNextRecord);

	/// \brief Set the first child record.
	///
	/// The new child record takes the place of the old child
	///  record, if it was already set.
	///
	/// @param pFirstChildRecord the first child record
	///
	///////////////////////////////////////////////////////////
	void setFirstChildRecord(ptr<directoryRecord> pFirstChildRecord);

	/// \brief Set the referenced record.
	///
	/// The new referenced record takes the place of the old 
	///  referenced record, if it was already set.
	///
	/// @param pReferencedRecord the referenced record
	///
	///////////////////////////////////////////////////////////
	void setReferencedRecord(ptr<directoryRecord> pReferencedRecord);
	
	/// \brief Get the full path to the  file referenced by
	///         the record.
	///
	/// Several calls to this function may have to be made to 
	///  obtain the full file path.
	///
	/// The full file path is formed by a list of folder and
	///  by the file name; for each call to this function
	///  the caller has to specify a zero based part number.
	///
	/// The last valid part contains the file name, while
	///  the parts before the last one contain the folder
	///  in which the file is stored; each folder is a child
	///  of the folder returned by the previous part.
	///
	/// The function returns an empty string if the
	///  specified part doesn't exist.
        ///
        /// See also setFilePart().
	/// 
	/// @param part the part to be returned, 0 based.
	/// @return the part's name (folder or file), or an
	///          empty string if the requested part doesn't
	///          exist
	///
	///////////////////////////////////////////////////////////
	std::wstring getFilePart(imbxUint32 part);

	/// \brief Set the full path to the  file referenced by
	///         the record.
	///
	/// Several calls to this function may have to be made to
	///  set the full file path.
	///
	/// The full file path is formed by a list of folder and
	///  by the file name; for each call to this function
	///  the caller has to specify a zero based part number.
	///
	/// The last valid part contains the file name, while
	///  the parts before the last one contain the folder
	///  in which the file is stored; each folder is a child
	///  of the folder set in the previous part.
	///
        /// For instance, the file /folder/file.dcm is set
        ///  with two calls to setFilePart():
        /// - setFilePart(0, "folder")
        /// - setFilePart(1, "file.dcm")
        ///
        /// See also getFilePart().
	///
	/// @param part the part to be set, 0 based.
        /// @param partName tha value to set for the part
	///
	///////////////////////////////////////////////////////////
	void setFilePart(imbxUint32 part, const std::wstring partName);

	/// \brief Returns the record's type.
	///
	/// This function calls getTypeString() and convert the
	///  result to an enumerated value.
	///
	/// @return the record's type
	///
	///////////////////////////////////////////////////////////
	tDirectoryRecordType getType();

	/// \brief Returns a string representing the record's type.
	///
	/// @return the record's type
	///
	///////////////////////////////////////////////////////////
	std::wstring getTypeString();

	/// \brief Sets the record's type.
	///
	/// @param recordType  the record's type
	///
	///////////////////////////////////////////////////////////
	void setType(tDirectoryRecordType recordType);

	/// \brief Sets the record's type.
	///
	/// @param recordType  the string representing the record's
	///                     type
	///
	///////////////////////////////////////////////////////////
	void setTypeString(std::wstring recordType);


private:
	/// \brief Constructor. A directoryRecord object must be
	///         connected to a dataSet object, which contains
	///         the record's information.
	///
	/// The dataSet object is an item of the sequence tag 
	///  0004,1220
	///
	/// When a directoryRecord is connected to an empty dataSet
	///  then the other constructor must be used, which allows
	///  to define the record type.
	///
	/// @param pDataSet  the dataSet that must be connected
	///                   to the directoryRecord
	///
	///////////////////////////////////////////////////////////
	directoryRecord(ptr<dataSet> pDataSet);

	void checkCircularReference(directoryRecord* pStartRecord);

	void updateOffsets();

	ptr<directoryRecord> m_pNextRecord;
	ptr<directoryRecord> m_pFirstChildRecord;
	ptr<directoryRecord> m_pReferencedRecord;

	ptr<dataSet> m_pDataSet;
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief A dicomDir object contains the information about
///         the tree structure of a DICOMDIR dataset.
///
/// The dicomDir class connects to the DICOMDIR dataset
///  specified in the constructor and detects the DICOMDIR
///  tree structure that it contains.
///
/// Each directory record in the DICOMDIR structure is
///  represented by a directoryRecord class.
///
/// The first root directoryRecord can be obtained with
///  a call to getFirstRootRecord().
///
/// Any change made to the dicomDir class is immediately
///  reflected into the connected dataset.
///
/// WARNING: all the directoryRecord allocated by this
///  class are released only when the dicomDir itself
///  is released, even if the directoryRecord are 
///  explicitly released by your application.\n
/// 
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dicomDir: public baseObject
{
public:
	/// \brief Initializes a dicomDir object and attach it to 
	///         a dataset.
	///
	/// If a dataSet is specified then the constructor parses
	///  it and build an in memory representation of the
	///  directory tree and its records. The first root record
	///  can be retrieved with a call to getFirstRootRecord().
	///
	/// @param pDataSet   the dataSet that contains or will 
	///                    contain the DICOMDIR structure. 
	///                   If a null pointer is passed, then a
	///                    new dataSet is created by this
	///                    constructor
	///
	///////////////////////////////////////////////////////////
	dicomDir(ptr<dataSet> pDataSet);

	/// \brief Returns the DICOMDIR dataset.
	///
	/// This is the same dataset specified in the constructor
	///  dicomDir::dicomDir() and returned by buildDataSet().
	///
	/// @return the dataset containing the DICOMDIR
	///          information
	///
	///////////////////////////////////////////////////////////
	ptr<dataSet> getDirectoryDataSet();

	/// \brief Creates a new directoryRecord and embeds its
	///         dataSet into the DICOMDIR sequence of items.
	///
	/// Once the new directoryRecord has been returned, the
	///  calling application should set the proper record's
	///  values and specify the relationship with other items
	///  by calling setFirstRootRecord() or 
	///  directoryRecord::setNextRecord() or 
	///  directoryRecord::setFirstChildRecord() or
	///  directoryRecord::setReferencedRecord().
	///
	/// @return a new directoryRecord object belonging to the
	///         DICOMDIR
	///
	///////////////////////////////////////////////////////////
	ptr<directoryRecord> getNewRecord();

	/// \brief Returns the first root record in the DICOMDIR.
	///
	/// Once the first root record has been retrieved, the
	///  application can retrieve sibling root records by
	///  calling directoryRecord::getNextRecord().
	///
	/// @return the first root record in the DICOMDIR.
	///
	///////////////////////////////////////////////////////////
	ptr<directoryRecord> getFirstRootRecord();

	/// \brief Sets the first root record in the DICOMDIR.
	///
	/// @param pFirstRootRecord a directoryRecord obtained with
	///                          a call to getNewRecord().
	///                         The directoryRecord will be the
	///                          first root record in the 
	///                          directory
	///////////////////////////////////////////////////////////
	void setFirstRootRecord(ptr<directoryRecord> pFirstRootRecord);

	/// \brief Updates the dataSet containing the DICOMDIR
	///         with the information contained in the directory
	///         records.
	///
	/// Before building the DICOMDIR dataSet remember to set
	///  the following values in the dataSet (call
	///  getDirectoryDataSet() to get the dataSet):
	/// - tag 0x2,0x3: Media Storage SOP instance UID
	/// - tag 0x2,0x12: Implementation class UID
	/// - tag 0x2,0x13: Implementation version name
	/// - tag 0x2,0x16: Source application Entity Title
	///
	/// Please note that if any data is added to the directory
	///  or any of the directoryItem objects after a call to
	///  buildDataSet() then the dataset has to be rebuilt once
	///  more with another call to buildDataSet().
	///
	/// buildDataSet() doesn't return a new dataSet but 
	///  modifies the dataSet specified in the constructor.
	///
	/// @return a pointer to the updated dataSet
	///
	///////////////////////////////////////////////////////////
	ptr<dataSet> buildDataSet();

protected:
	// Destructor.
	//
	///////////////////////////////////////////////////////////
	virtual ~dicomDir();

	ptr<dataSet> m_pDataSet;

	ptr<directoryRecord> m_pFirstRootRecord;

	typedef std::list<ptr<directoryRecord> > tRecordsList;
	tRecordsList m_recordsList;
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief Base class from which the exceptions thrown
///         by directoryRecord and dicomDir derive.
///
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dicomDirException: public std::runtime_error
{
public:
	/// \brief Build a dicomDirException exception.
	///
	/// @param message the message to store into the exception
	///
	///////////////////////////////////////////////////////////
	dicomDirException(const std::string& message): std::runtime_error(message){}
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This exception is thrown when a call to
///        dicomDir::setFirstRootRecord() or 
///        directoryRecord::setNextRecord() or 
///        directoryRecord::setFirstChildRecord() or
///        directoryRecord::setReferencedRecord() causes
///        a circular reference between directoryRecord
///        objects.
///
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dicomDirCircularReferenceException: public dicomDirException
{
public:
	/// \brief Build a dicomDirCircularReferenceException
	///         exception.
	///
	/// @param message the message to store into the exception
	///
	///////////////////////////////////////////////////////////
	dicomDirCircularReferenceException(const std::string& message): dicomDirException(message){}
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief Exception thrown when an unknown record type
///        is detected in directoryRecord::getType(),
///        directoryRecord::getTypeString(), 
///        directoryRecord::setType() or 
///        directoryRecord::setTypeString().
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dicomDirUnknownDirectoryRecordType: public dicomDirException
{
public:
	/// \brief Build a dicomDirUnknownDirectoryRecordType 
	///         exception.
	///
	/// @param message the message to store into the exception
	///
	///////////////////////////////////////////////////////////
	dicomDirUnknownDirectoryRecordType(const std::string& message): dicomDirException(message){}
};

/// @}

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraDicomDir_93F684BF_0024_4bf3_89BA_D98E82A1F44C__INCLUDED_)
