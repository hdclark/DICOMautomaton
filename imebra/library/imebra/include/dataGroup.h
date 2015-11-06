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

/*! \file dataGroup.h
    \brief Declaration of the class used to store a single dicom group.

*/

#if !defined(imebraDataGroup_E2A96B14_8398_4a3c_BB3B_A850F27F88AC__INCLUDED_)
#define imebraDataGroup_E2A96B14_8398_4a3c_BB3B_A850F27F88AC__INCLUDED_

#include "dataCollection.h"
#include "dataHandler.h"



///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe::imebra
//
///////////////////////////////////////////////////////////
namespace puntoexe
{

	class streamReader;
	class streamWriter;

namespace imebra
{

/// \addtogroup group_dataset
///
/// @{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief Represents a Dicom group which stores a group 
///         of Dicom tags.
///
/// The Dicom tags are organized into groups: this class
///  stores all the tags that belong to the same group.
///
/// Groups and tags (represented by the class \ref data)
///  are identified by an ID.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataGroup : public dataCollection<data>
{
public:
	dataGroup(ptr<baseObject> externalLock): dataCollection<data>(externalLock) {}

	/// \brief Get the requested tag (class \ref data).
	///
	/// @param tagId the tag's id (without the group id).
	/// @param bCreate if true and the tag doesn't exist then
	///              a new tag will be created.
	/// @return a pointer to the tag object
	///
	///////////////////////////////////////////////////////////
	ptr<data> getTag(imbxUint16 tagId, bool bCreate=false);
	
	/// \brief Get a handlers::dataHandler object for the 
	///         requested tag's buffer.
	///
	/// A tag is represented by the class \ref data.
	///
	/// The data handler (handlers::dataHandler) allows the 
	///  application to read, write and resize the tag's 
	///  buffer.
	///
	/// A tag can store several buffers: the application
	///  must specify the buffer's id it wants to deal with.
	///
	/// @param tagId    the tag's id, without the group's id.
	/// @param bufferId the buffer's id (zero based). See
	///                  data::getDataHandler() for more 
	///                  informantion regarding this parameter
	/// @param bWrite   true if the application wants to write
	///                  into the buffer
	/// @param defaultType a string with the dicom data type
	///                  to use if the buffer doesn't exist.
	///                 If none is specified, then a default
	///                  data type will be used.
	///                 This parameter is ignored if bWrite is
	///                  set to false
	/// @return a pointer to the handlers::dataHandler
	///
	///////////////////////////////////////////////////////////
	ptr<handlers::dataHandler> getDataHandler(imbxUint16 tagId, imbxUint32 bufferId, bool bWrite, std::string defaultType="");

	/// \brief Get a handlers::dataHandlerRaw object for the 
	///         requested tag's buffer.
	///
	/// A raw data handler always sees the buffer as a 
	///  collection of bytes, no matter what its real data type
	///  is.
	///
	/// A tag is represented by the class \ref data.
	///
	/// The raw data handler (handlers::dataHandlerRaw) 
	///  allows the application to read, write and resize the 
	///  tag's buffer.
	/// The data handler works on a local copy of the data so
	///  the application doesn't have to worry about 
	///  multithreading accesses to the buffer's data.
	///
	/// A tag can store several buffers: the application
	///  must specify the buffer's id it wants to deal with.
	///
	/// @param tagId    the tag's id, without the group's id.
	/// @param bufferId the buffer's id (zero based). See
	///                  data::getDataHandlerRaw() for more 
	///                  informantion regarding this parameter
	/// @param bWrite   true if the application wants to write
	///                  into the buffer
	/// @param defaultType a string with the dicom data type
	///                  to use if the buffer doesn't exist.
	///                 If none is specified, then a default
	///                  data type will be used.
	///                 This parameter is ignored if bWrite is
	///                  set to false
	/// @return a pointer to the handlers::dataHandlerRaw
	///
	///////////////////////////////////////////////////////////
	ptr<handlers::dataHandlerRaw> getDataHandlerRaw(imbxUint16 tagId, imbxUint32 bufferId, bool bWrite, std::string defaultType="");

	/// \brief Get a streamReader connected to the requested
	///         tag's buffer.
	///
	/// A tag is represented by the class \ref data.
	///
	/// The streamReader allows the application to read the
	///  data from the tag's buffer.
	/// The stream works on a local copy of the data so
	///  the application doesn't have to worry about 
	///  multithreading accesses to the buffer's data.
	///
	/// A tag can store several buffers: the application
	///  must specify the buffer's id it wants to deal with.
	///
	/// @param tagId    the tag's id, without the group's id.
	/// @param bufferId the buffer's id (zero based). See
	///                  data::getDataHandler() for more 
	///                  informantion regarding this parameter
	/// @return a pointer to the streamReader
	///
	///////////////////////////////////////////////////////////
	ptr<streamReader> getStreamReader(imbxUint16 tagId, imbxUint32 bufferId);

	/// \brief Get a streamWriter connected to the requested
	///         tag's buffer.
	///
	/// A tag is represented by the class \ref data.
	///
	/// The streamWriter allows the application to write
	///  the data into the tag's buffer.
	/// The stream works on a local copy of the data so
	///  the application doesn't have to worry about 
	///  multithreading accesses to the buffer's data.
	///
	/// A tag can store several buffers: the application
	///  must specify the buffer's id it wants to deal with.
	///
	/// @param tagId    the tag's id, without the group's id.
	/// @param bufferId the buffer's id (zero based). See
	///                  data::getDataHandler() for more 
	///                  informantion regarding this parameter
	/// @param dataType the datatype used to create the 
	///                  buffer if it doesn't exist already
	/// @return a pointer to the streamWriter
	///
	///////////////////////////////////////////////////////////
	ptr<streamWriter> getStreamWriter(imbxUint16 tagId, imbxUint32 bufferId, std::string dataType = "");

	/// \brief Return the data type of the specified tag, in
	///         dicom format.
	///
	/// A dicom data type is composed by 2 uppercase chars.
	///
	/// If the specified tag doesn't exist, then the function
	///  returns an empty string.
	///
	/// @param  tagId the tag's id, without the group's id
	/// @return the tag's data type in dicom format, or an
	///          empty string if the tag doesn't exist in
	///          the group
	///
	///////////////////////////////////////////////////////////
	std::string getDataType(imbxUint16 tagId);

};

/// @}

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraDataGroup_E2A96B14_8398_4a3c_BB3B_A850F27F88AC__INCLUDED_)
