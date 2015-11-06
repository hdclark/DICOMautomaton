/*

Imebra 2011 build 2011-09-18_22-24-41

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

/*! \file dicomDict.h
    \brief Declaration of the class dicomDict.

*/


#if !defined(imebraDicomDict_CC44A2C5_2B8C_42c1_9704_3F9C582643B9__INCLUDED_)
#define imebraDicomDict_CC44A2C5_2B8C_42c1_9704_3F9C582643B9__INCLUDED_

#include "../../base/include/baseObject.h"
#include <map>


///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe::imebra
//
///////////////////////////////////////////////////////////
namespace puntoexe
{

namespace imebra
{

/// \addtogroup group_dictionary Dicom dictionary
///
/// @{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief The Dicom Dictionary.
///
/// This class can be used to retrieve the tags' default
///  data types and descriptions.
///
/// An instance of this class is automatically allocated
///  by the library: your application should use the
///  static function getDicomDictionary() in order to
///  get the only valid instance of this class.
/// 
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dicomDictionary
{
	struct imageDataDictionaryElement
	{
		std::wstring m_tagName;
		std::string m_tagType;
	};

	struct validDataTypesStruct
	{
		bool  m_longLength;       // true if the tag has a 4 bytes length descriptor
		imbxUint32 m_wordLength;       // Word's length, used for byte reversing in hi/lo endian conversion
		imbxUint32 m_maxLength;        // The maximum length for the tag. An exception will be trown while reading a tag which exceedes this size 
	};

public:
	dicomDictionary();

	void registerTag(imbxUint32 tagId, const wchar_t* tagName, const char* tagType);
	void registerVR(std::string vr, bool bLongLength, imbxUint32 wordSize, imbxUint32 maxLength);

	/// \brief Retrieve a tag's description.
	///
	/// @param groupId   The group which the tag belongs to
	/// @param tagId     The tag's id
	/// @return          The tag's description
	///
	///////////////////////////////////////////////////////////
	std::wstring getTagName(imbxUint16 groupId, imbxUint16 tagId) const;

	/// \brief Retrieve a tag's default data type.
	///
	/// @param groupId   The group which the tag belongs to
	/// @param tagId     The tag's id
	/// @return          The tag's data type
	///
	///////////////////////////////////////////////////////////
	std::string getTagType(imbxUint16 groupId, imbxUint16 tagId) const;

	/// \brief Retrieve the only valid instance of this class.
	///
	/// @return a pointer to the dicom dictionary
	///
	///////////////////////////////////////////////////////////
	static dicomDictionary* getDicomDictionary();

	/// \brief Return true if the specified string represents
	///         a valid dicom data type.
	///
	/// @param dataType the string to be checked
	/// @return         true if the specified string is a valid
	///                  dicom data type
	///
	///////////////////////////////////////////////////////////
	bool isDataTypeValid(std::string dataType) const;

	/// \brief Return true if the tag's length in the dicom 
	///         stream must be written using a DWORD
	///
	/// @param dataType the data type for which the information
	///                  is required
	/// @return         true if the specified data type's 
	///                  length must be written using a DWORD
	///
	///////////////////////////////////////////////////////////
	bool getLongLength(std::string dataType) const ;
	
	/// \brief Return the size of the data type's elements
	///
	/// @param dataType the data type for which the information
	///                  is required
	/// @return the size of a single element
	///
	///////////////////////////////////////////////////////////
	imbxUint32 getWordSize(std::string dataType) const;
	
	/// \brief Return the maximum size of the tags with
	///         the specified data type.
	///
	/// @param dataType the data type for which the information
	///                  is required
	/// @return         the maximum tag's size in bytes 
	///
	///////////////////////////////////////////////////////////
	imbxUint32 getMaxSize(std::string dataType) const;

protected:
	typedef std::map<imbxUint32, imageDataDictionaryElement> tDicomDictionary;
	tDicomDictionary m_dicomDict;

	typedef std::map<std::string, validDataTypesStruct> tVRDictionary;
	tVRDictionary m_vrDict;

};

/// @}

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraDicomDict_CC44A2C5_2B8C_42c1_9704_3F9C582643B9__INCLUDED_)
