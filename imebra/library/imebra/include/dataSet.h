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

/*! \file dataSet.h
    \brief Declaration of the class dataSet.

*/

#if !defined(imebraDataSet_93F684BF_0024_4bf3_89BA_D98E82A1F44C__INCLUDED_)
#define imebraDataSet_93F684BF_0024_4bf3_89BA_D98E82A1F44C__INCLUDED_

#include "dataCollection.h"
#include "dataGroup.h"
#include "../../base/include/exception.h"
#include "codec.h"

#include <vector>


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

//The following classes are used in this declaration file
class image;
class lut;
class waveform;

/// \addtogroup group_dataset Dicom data
/// \brief The Dicom dataset is represented by the
///         class dataSet.
///
/// @{



///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief A data set is a collection of groups of tags
///        (see dataGroup).
///
/// The dataSet is usually built from a dicom stream by
///  using the codec codecs::dicomCodec.
///
/// Also the tags with the data type SQ (sequence) contains
///  one or more embedded dataSets that can be retrieved
///  by using data::getDataSet().
///
/// If your application creates a new dataset then it can
///  set the default dataSet's charset by calling 
///  setCharsetsList(). See \ref imebra_unicode for
///  more information related to Imebra and the Dicom
///   charsets.
///
/// The dataSet and its components (all the dataGroup, 
///  and data) share a common lock object:
///  this means that a lock on one of the dataSet's
///  component will lock the entire dataSet and all
///  its components.
///
/// For an introduction to the dataSet, read 
///  \ref quick_tour_dataSet.
/// 
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataSet : public dataCollection<dataGroup>
{
public:
	// Costructor
	///////////////////////////////////////////////////////////
	dataSet(): dataCollection<dataGroup>(ptr<baseObject>(new baseObject)), m_itemOffset(0) {}

	///////////////////////////////////////////////////////////
	/// \name Get/set groups/tags
	///
	///////////////////////////////////////////////////////////
	//@{

	/// \brief Retrieve a tag object.
	///
	/// Tag object is represented by the \ref data class.
	///
	/// If the tag doesn't exist and the parameter bCreate is
	///  set to false, then the function returns a null
	///  pointer.
	/// If the tag doesn't exist and the parameter bCreate is
	///  set to true, then an empty tag will be created and
	///  inserted into the dataset.
	///
	/// @param groupId The group to which the tag belongs.
	/// @param order   If the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify which group
	///                 must be retrieved.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero.
	/// @param tagId   The id of the tag to retrieve.
	/// @param bCreate When bCreate is set to true and the
	///                 requested tag doesn't exist,
	///                 then a new one is created and inserted
	///                 into the dataset.
	///                When bCreate is set to false and the
	///                 requested tag doesn't exist, then
	///                 a null pointer is returned.
	/// @return        A pointer to the retrieved tag.
	///                If the requested tag doesn't exist then
	///                 the returned value depend on the value
	///                 of the bCreate parameter: when bCreate
	///                 is false then a value of zero is
	///                 returned, otherwise a pointer to the
	///                 just created tag is returned.
	///
	///////////////////////////////////////////////////////////
	ptr<data> getTag(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, bool bCreate=false);
	
	/// \brief Retrieve a group object.
	///
	/// A Group object is represented by the \ref dataGroup
	/// class.
	///
	/// If the group doesn't exist and the parameter bCreate
	///  is set to false, then the function returns a null
	///  pointer.
	/// If the group doesn't exist and the parameter bCreate
	///  is set to true, then an empty group will be created
	///  and inserted into the dataset.
	///
	/// @param groupId The group to retrieve.
	/// @param order   If the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify which group
	///                 must be retrieved.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero.
	/// @param bCreate When bCreate is set to true and the
	///                 requested group doesn't exist,
	///                 then a new one is created and inserted
	///                 into the dataset.
	///                When bCreate is set to false and the
	///                 requested group doesn't exist, then
	///                 a null pointer is returned.
	/// @return        A pointer to the retrieved group.
	///                The group should be released as soon as
	///                 possible using the function Release().
	///                If the requested group doesn't exist
	///                 then the returned value depend on the
	///                 value of the bCreate parameter: when
	///                 bCreate is false then a value of zero
	///                 is returned, otherwise a pointer to the
	///                 just created group is returned.
	///
	///////////////////////////////////////////////////////////
	ptr<dataGroup> getGroup(imbxUint16 groupId, imbxUint16 order, bool bCreate=false);
	
	/// \brief Insert the specified group into the dataset.
	///
	/// A Group object is represented by the \ref dataGroup
	///  class.
	///
	/// If a group with the same id and order is already
	///  present into the data set, then it is removed to
	///  leave space to the new group.
	///
	/// @param groupId The id of the group to insert into
	///                 the data set.
	/// @param order   If the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the group belongs.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero.
	/// @param pGroup  A pointer to the group to insert into
	///                 the data set.
	///
	///////////////////////////////////////////////////////////
	void setGroup(imbxUint16 groupId, imbxUint16 order, ptr<dataGroup> pGroup);

	//@}


	///////////////////////////////////////////////////////////
	/// \name Get/set the image
	///
	///////////////////////////////////////////////////////////
	//@{
	
	/// \brief Retrieve an image from the dataset.
	///
	/// The right codec will be automatically used to
	///  decode the image embedded into the dataset.
	/// If multiple frames are available, then the
	///  calling application can decide the frame to
	///  retrieve.
	///
	/// The function throw an exception if the requested
	///  image doesn't exist or the image's tag is
	///  corrupted.
	///
	/// The retrieved image should then be processed by the
	///  transforms::modalityVOILUT transform in order to 
	///  convert the pixels value to a meaningful space.
	/// Infact, the dicom image's pixel values saved by
	///  other application have a meaningful value only for
	///  the application that generated them, while the
	///  modality VOI/LUT transformation will convert those
	///  values to a more portable unit (e.g.: optical
	///  density).
	///
	/// Further transformations are applied by 
	///  the transforms::VOILUT transform, in order to 
	///  adjust the image's contrast for displaying purposes.
	///  
	/// @param frameNumber The frame number to retrieve.
	///                    The first frame's id is 0
	/// @return            A pointer to the retrieved
	///                     image
	///
	///////////////////////////////////////////////////////////
	ptr<image> getImage(imbxUint32 frameNumber);
	
	/// \brief Insert an image into the data set.
	///
	/// The specified transfer syntax will be used to choose
	///  the right codec for the image.
	///
	/// @param frameNumber The frame number where the image
	///                     must be stored.
	///                    The first frame's id is 0.
	/// @param pImage      A pointer to the image object to
	///                     be stored into the data set.
	/// @param transferSyntax the transfer syntax that
	///                     specifies the codec and the 
	///                     parameters to use for the encoding
	/// @param quality     an enumeration that set the 
	///                     compression quality
	///
	///////////////////////////////////////////////////////////
	void setImage(imbxUint32 frameNumber, ptr<image> pImage, std::wstring transferSyntax, codecs::codec::quality quality);

	/// \brief Get a frame's offset from the offset table.
	///
	/// @param frameNumber the number of the frame for which
	///                     the offset is requested
	/// @return the offset for the specified frame
	///
	///////////////////////////////////////////////////////////
	imbxUint32 getFrameOffset(imbxUint32 frameNumber);

	/// \brief Get the id of the buffer that starts at the
	///         specified offset.
	///
	/// @param offset   one offset retrieved from the frames
	///                  offset table: see getFrameOffset()
	/// @param pLengthToBuffer a pointer to a variable that
	///                  will store the total lenght of
	///                  the buffers that preceed the one
	///                  being returned (doesn't include
	///                  the tag descriptors)
	/// @return         the id of the buffer that starts at
	///                  the specified offset
	///
	///////////////////////////////////////////////////////////
	imbxUint32 getFrameBufferId(imbxUint32 offset, imbxUint32* pLengthToBuffer);

	/// \brief Retrieve the first and the last buffers used
	///         to store the image.
	///
	/// This function works only with the new Dicom3 streams,
	///  not with the old NEMA format.
	///
	/// This function is used by setImage() and getImage().
	///
	/// @param frameNumber the frame for which the buffers 
	///                     have to be retrieved
	/// @param pFirstBuffer a pointer to a variable that will
	///                     contain the id of the first buffer
	///                     used to store the image
	/// @param pEndBuffer  a pointer to a variable that will
	///                     contain the id of the first buffer
	///                     next to the last one used to store 
	///                     the image
	/// @return the total length of the buffers that contain
	///          the image
	///
	///////////////////////////////////////////////////////////
	imbxUint32 getFrameBufferIds(imbxUint32 frameNumber, imbxUint32* pFirstBuffer, imbxUint32* pEndBuffer);
	
	/// \brief Return the first buffer's id available where
	///         a new frame can be saved.
	///
	/// @return the id of the first buffer available to store
	///          a new frame
	///
	///////////////////////////////////////////////////////////
	imbxUint32 getFirstAvailFrameBufferId();

	//@}


	///////////////////////////////////////////////////////////
	/// \name Get/set a sequence item
	///
	///////////////////////////////////////////////////////////
	//@{

	/// \brief Retrieve a data set embedded into a sequence
	///        tag.
	///
	/// Sequence tags store several binary data which can be 
	///  individually parsed as a normal dicom file 
	///  (without the preamble of 128 bytes and the DICM 
	///   signature).
	///
	/// Using sequences, an application can store several
	///  nested dicom structures.
	///
	/// This function parse a single item of a sequence tag
	///  and return a data set object (represented by a 
	///  this class) which stores the retrieved
	///  tags.
	///
	/// If the requested tag's type is not a sequence or the
	///  requested item in the sequence is missed, then a null
	///  pointer will be returned.
	///
	/// @param groupId The group to which the sequence 
	///                 tag to be parsed belongs
	/// @param order   If the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify which group
	///                 must be retrieved.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   The id of the tag to parse
	/// @param itemId  The id of the tag's item to parse
	///                (zero based)
	/// @return        A pointer to the retrieved data set.
	///                If the requested group, tag or buffer
	///                 (sequence item) doesn't exist, or if
	///                 the tag's type is not a sequence (SQ),
	///                 then a null pointer is returned,
	///                 otherwise a pointer to the retrieved
	///                 dataset is returned
	///
	///////////////////////////////////////////////////////////
	ptr<dataSet> getSequenceItem(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 itemId);

	/// \brief Retrieve a LUT.
	///
	/// LUT are encoded into sequences.
	/// This function retrieve the sequence and build
	///  a \ref lut object describing the LUT.
	///
	/// @param groupId The group to which the LUT sequence 
	///                 belongs
	/// @param tagId   The id of the tag to containing the
	///                 LUT
	/// @param lutId   The id of the lut inside the tag (0
	///                 based)
	/// @return        A pointer to the retrieved LUT.
	///                If the requested group, tag or buffer
	///                 (sequence item) doesn't exist, or if
	///                 the tag's type is not a sequence (SQ),
	///                 then a null pointer is returned,
	///                 otherwise a pointer to the retrieved
	///                 LUT is returned
	///
	///////////////////////////////////////////////////////////
	ptr<lut> getLut(imbxUint16 groupId, imbxUint16 tagId, imbxUint32 lutId);

	/// \brief Retrieve a waveform from the dataSet.
	///
	/// Each waveforms is stored in a sequence item;
	/// the function retrieves the proper sequence item and
	///  connects it to the class waveform which can be used
	///  to retrieve the waveform data.
	///
	/// @param waveformId   the zero based index of the 
	///                      waveform to retrieve
	/// @return an object waveform that can be used to read
	///          the waveform data, or a null pointer if
	///          the requested waveform doesn't exist
	///
	///////////////////////////////////////////////////////////
	ptr<waveform> getWaveform(imbxUint32 waveformId);

	//@}


	///////////////////////////////////////////////////////////
	/// \name Get/set the tags' values
	///
	///////////////////////////////////////////////////////////
	//@{
public:	
	/// \brief Retrieve a tag's value as a signed long.
	///
	/// Read the value of the requested tag and return it as
	///  a signed long.
	///
	/// @param groupId The group to which the tag to be read
	///                 belongs
	/// @param order   If the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the group belongs
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   The id of the tag to retrieve
	/// @param elementNumber The element's number to retrieve.
	///                A buffer can store several elements:
	///                 this parameter specifies which element
	///                 must be retrieved.
	///                The first element's number is 0
	/// @return        The tag's content, as a signed long
	///
	///////////////////////////////////////////////////////////
	imbxInt32 getSignedLong(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 elementNumber);

	/// \brief Set a tag's value as a signed long.
	///
	/// If the specified tag doesn't exist, then a new one
	///  will be created and inserted into the dataset.
	///
	/// @param groupId The group to which the tag to be write
	///                 belongs
	/// @param order   If the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the group belongs.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   The id of the tag to set
	/// @param elementNumber The element's number to set.
	///                A buffer can store several elements:
	///                 this parameter specifies which element
	///                 must be set.
	///                The first element's number is 0
	/// @param newValue the value to be written into the tag
	/// @param defaultType if the specified tag doesn't exist
	///                 then the function will create a new
	///                 tag with the data type specified in
	///                 this parameter
	///
	///////////////////////////////////////////////////////////
	void setSignedLong(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 elementNumber, imbxInt32 newValue, std::string defaultType = "");

	/// \brief Retrieve a tag's value as an unsigned long.
	///
	/// Read the value of the requested tag and return it as
	///  an unsigned long.
	///
	/// @param groupId The group to which the tag to be read
	///                 belongs
	/// @param order   If the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the group belongs.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   The id of the tag to retrieve
	/// @param elementNumber The element's number to retrieve.
	///                A buffer can store several elements:
	///                 this parameter specifies which element
	///                 must be retrieved.
	///                The first element's number is 0
	/// @return        The tag's content, as an unsigned long
	///
	///////////////////////////////////////////////////////////
	imbxUint32 getUnsignedLong(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 elementNumber);

	/// \brief Set a tag's value as an unsigned long.
	///
	/// If the specified tag doesn't exist, then a new one
	///  will be created and inserted into the dataset.
	///
	/// @param groupId The group to which the tag to be write
	///                 belongs
	/// @param order   If the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the group belongs.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   The id of the tag to set
	/// @param elementNumber The element's number to set.
	///                A buffer can store several elements:
	///                 this parameter specifies which element
	///                 must be set.
	///                The first element's number is 0
	/// @param newValue the value to be written into the tag
	/// @param defaultType if the specified tag doesn't exist
	///                 then the function will create a new
	///                 tag with the data type specified in
	///                 this parameter
	///
	///////////////////////////////////////////////////////////
	void setUnsignedLong(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 elementNumber, imbxUint32 newValue, std::string defaultType = "");

	/// \brief Retrieve a tag's value as a double.
	///
	/// Read the value of the requested tag and return it as
	///  a double.
	///
	/// @param groupId The group to which the tag to be read
	///                 belongs
	/// @param order   If the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the group belongs.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   The id of the tag to retrieve
	/// @param elementNumber The element's number to retrieve.
	///                A buffer can store several elements:
	///                 this parameter specifies which element
	///                 must be retrieved.
	///                The first element's number is 0
	/// @return        The tag's content, as a double
	///
	///////////////////////////////////////////////////////////
	double getDouble(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 elementNumber);
	
	/// \brief Set a tag's value as a double.
	///
	/// If the specified tag doesn't exist, then a new one
	///  will be created and inserted into the dataset.
	///
	/// @param groupId The group to which the tag to be write
	///                 belongs
	/// @param order   If the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the group belongs.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   The id of the tag to set
	/// @param elementNumber The element's number to set.
	///                A buffer can store several elements:
	///                 this parameter specifies which element
	///                 must be set.
	///                The first element's number is 0
	/// @param newValue the value to be written into the tag
	/// @param defaultType if the specified tag doesn't exist
	///                 then the function will create a new
	///                 tag with the data type specified in
	///                 this parameter
	///
	///////////////////////////////////////////////////////////
	void setDouble(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 elementNumber, double newValue, std::string defaultType = "");

	/// \brief Retrieve a tag's value as a string.
	///        getUnicodeString() is preferred over this
	///         method.
	///
	/// Read the value of the requested tag and return it as
	///  a string.
	///
	/// @param groupId The group to which the tag to be read
	///                 belongs
	/// @param order   If the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the group belongs.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   The id of the tag to retrieve
	/// @param elementNumber The element's number to retrieve.
	///                A buffer can store several elements:
	///                 this parameter specifies which element
	///                 must be retrieved.
	///                The first element's number is 0
	/// @return        The tag's content, as a string
	///
	///////////////////////////////////////////////////////////
	std::string getString(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 elementNumber);

	/// \brief Retrieve a tag's value as an unicode string.
	///
	/// Read the value of the requested tag and return it as
	///  an unicode string.
	///
	/// @param groupId The group to which the tag to be read
	///                 belongs
	/// @param order   If the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the group belongs.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   The id of the tag to retrieve
	/// @param elementNumber The element's number to retrieve.
	///                A buffer can store several elements:
	///                 this parameter specifies which element
	///                 must be retrieved.
	///                The first element's number is 0
	/// @return        The tag's content, as an unicode string
	///
	///////////////////////////////////////////////////////////
	std::wstring getUnicodeString(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 elementNumber);

	/// \brief Set a tag's value as a string.
	///        setUnicodeString() is preferred over this
	///         method.
	///
	/// If the specified tag doesn't exist, then a new one
	///  will be created and inserted into the dataset.
	///
	/// @param groupId The group to which the tag to be write
	///                 belongs
	/// @param order   If the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the group belongs.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   The id of the tag to set
	/// @param elementNumber The element's number to set.
	///                A buffer can store several elements:
	///                 this parameter specifies which element
	///                 must be set.
	///                The first element's number is 0
	/// @param newString the value to be written into the tag
	/// @param defaultType if the specified tag doesn't exist
	///                 then the function will create a new
	///                 tag with the data type specified in
	///                 this parameter
	///
	///////////////////////////////////////////////////////////
	void setString(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 elementNumber, std::string newString, std::string defaultType = "");

	/// \brief Set a tag's value as an unicode string.
	///
	/// If the specified tag doesn't exist, then a new one
	///  will be created and inserted into the dataset.
	///
	/// @param groupId The group to which the tag to be write
	///                 belongs
	/// @param order   If the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the group belongs.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   The id of the tag to set
	/// @param elementNumber The element's number to set.
	///                A buffer can store several elements:
	///                 this parameter specifies which element
	///                 must be set.
	///                The first element's number is 0
	/// @param newString the value to be written into the tag
	/// @param defaultType if the specified tag doesn't exist
	///                 then the function will create a new
	///                 tag with the data type specified in
	///                 this parameter
	///
	///////////////////////////////////////////////////////////
	void setUnicodeString(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 elementNumber, std::wstring newString, std::string defaultType = "");

	//@}


	///////////////////////////////////////////////////////////
	/// \name Data handlers
	///
	///////////////////////////////////////////////////////////
	//@{
public:
	/// \brief Return the default data type for the specified
	///        tag's id.
	///
	/// The default data type is retrieved from an internal
	///  dictionary which stores the default properties of
	///  each dicom's tag.
	///
	/// @param groupId    The group to which the tag
	///                   belongs
	/// @param tagId      The id of the tag.
	/// @return           the tag's default type.
	///                   The returned string is a constant.
	///////////////////////////////////////////////////////////
	std::string getDefaultDataType(imbxUint16 groupId, imbxUint16 tagId);

	/// \brief Return the data type of a tag
	///
	/// @param groupId    The group to which the tag belongs
	/// @param order      When multiple groups with the same
	///                    it are present, then use this
	///                    parameter to specify which group
	///                    must be used. The first group as
	///                    an order of 0.
	/// @param tagId      The id of the tag for which the type
	///                    must be retrieved.
	/// @return           a string with the tag's type.
	///
	///////////////////////////////////////////////////////////
	std::string getDataType(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId);

	/// \brief Return a data handler for the specified tag's
	///         buffer.
	///
	/// The data handler allows the application to read, write
	///  and resize the tag's buffer.
	///
	/// A tag can store several buffers, then the application
	///  must specify the buffer's id it wants to deal with.
	///
	/// @param groupId the group to which the tag belongs
	/// @param order   if the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the tag belongs.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   the tag's id
	/// @param bufferId the buffer's id (zero based)
	/// @param bWrite  true if the application wants to write
	///                 into the buffer
	/// @param defaultType a string with the dicom data type 
	///                 to use if the buffer doesn't exist.
	///                If none is specified, then a default
	///                 data type will be used
	/// @return a pointer to the data handler.
	///
	///////////////////////////////////////////////////////////
	ptr<handlers::dataHandler> getDataHandler(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 bufferId, bool bWrite, std::string defaultType="");

	/// \brief Return a raw data handler for the specified 
	///         tag's buffer.
	///
	/// A raw data handler always sees the buffer as a 
	///  collection of bytes, no matter what the tag's data
	///  type is.
	///
	/// A tag can store several buffers, then the application
	///  must specify the buffer's id it wants to deal with.
	///
	/// @param groupId the group to which the tag belongs
	/// @param order   if the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the tag belongs.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   the tag's id
	/// @param bufferId the buffer's id (zero based)
	/// @param bWrite  true if the application wants to write
	///                 into the buffer
	/// @param defaultType a string with the dicom data type 
	///                 to use if the buffer doesn't exist.
	///                If none is specified, then a default
	///                 data type will be used
	/// @return a pointer to the data handler.
	///
	///////////////////////////////////////////////////////////
	ptr<handlers::dataHandlerRaw> getDataHandlerRaw(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 bufferId, bool bWrite, std::string defaultType="");

	/// \brief Return a streamReader connected to the specified
	///         tag's buffer's memory.
	///
	/// A tag can store several buffers: the application
	///  must specify the buffer's id it wants to deal with.
	///
	/// @param groupId the group to which the tag belongs
	/// @param order   if the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the tag belongs.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   the tag's id
	/// @param bufferId the buffer's id (zero based)
	/// @return a pointer to the streamReader
	///
	///////////////////////////////////////////////////////////
	ptr<streamReader> getStreamReader(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 bufferId);

	/// \brief Return a streamWriter connected to the specified
	///         tag's buffer's memory.
	///
	/// A tag can store several buffers, then the application
	///  must specify the buffer's id it wants to deal with.
	///
	/// @param groupId the group to which the tag belongs
	/// @param order   if the group is recurring in the file
	///                 (it appears several times), then use
	///                 this parameter to specify to which
	///                 group the tag belongs.
	///                This parameter is used to deal with
	///                 old DICOM files, since the new one
	///                 should use the sequence items to
	///                 achieve the same result.
	///                It should be set to zero
	/// @param tagId   the tag's id
	/// @param bufferId the buffer's id (zero based)
	/// @param dataType the datatype used to create the 
	///                 buffer if it doesn't exist already
	/// @return a pointer to the streamWriter
	///
	///////////////////////////////////////////////////////////
	ptr<streamWriter> getStreamWriter(imbxUint16 groupId, imbxUint16 order, imbxUint16 tagId, imbxUint32 bufferId, std::string dataType = "");

	//@}


	///////////////////////////////////////////////////////////
	/// \name Syncronize the charset tag (0008,0005)
	///
	///////////////////////////////////////////////////////////
	//@{
	
	/// \brief Collect all the charsets used in the dataSet's
	///         tags and then update the content of the tag
	///         0008,0005.
	///
	/// This function is called by the codecs before the dicom
	///  stream is saved, therefore the application doesn't
	///  need to call the function before saving the stream.
	///
	///////////////////////////////////////////////////////////
	void updateCharsetTag();

	/// \brief Update all the dataSet's tags with the charsets
	///         specified in the tag 0008,0005.
	///
	/// This function is called by the codecs after the stream
	///  has been loaded, therefore the application doesn't
	///  need to call the function after the stream has been
	///  loaded.
	///
	///////////////////////////////////////////////////////////
	void updateTagsCharset();

	//@}


	///////////////////////////////////////////////////////////
	/// \name Set/get the item offset.
	///
	///////////////////////////////////////////////////////////
	//@{
	
	/// \brief Called by codecs::dicomCodec when the dataset
	///         is written into a stream.
	///        Tells the dataSet the position at which it has
	///         been written into the stream
	///
	/// @param offset   the position at which the dataSet has
	///                  been written into the stream
	///
	///////////////////////////////////////////////////////////
	void setItemOffset(imbxUint32 offset);

	/// \brief Retrieve the offset at which the dataSet is
	///         located in the dicom stream.
	///
	/// @return the position at which the dataSet is located
	///          in the dicom stream
	///
	///////////////////////////////////////////////////////////
	imbxUint32 getItemOffset();

	//@}

protected:
	// Convert an image using the attributes specified in the
	//  the dataset
	///////////////////////////////////////////////////////////
	ptr<image> convertImageForDataSet(ptr<image> sourceImage);

	std::vector<imbxUint32> m_imagesPositions;

	// Position of the sequence item in the stream. Used to
	//  parse DICOMDIR items
	///////////////////////////////////////////////////////////
	imbxUint32 m_itemOffset;
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This is the base class for the exceptions thrown
///         by the dataSet.
/// 
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataSetException: public std::runtime_error
{
public:
	dataSetException(const std::string& message): std::runtime_error(message){}
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This exception is thrown when the application is
///         trying to store an image in the dataSet but
///         the dataSet already stores other images that
///         have different attributes.
///
/// The exception is usually thrown by dataSet::setImage().
/// 
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataSetExceptionDifferentFormat: public dataSetException
{
public:
	dataSetExceptionDifferentFormat(const std::string& message): dataSetException(message){}
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This exception is thrown when an unknown 
///         transfer syntax is being used while reading or
///         writing a stream.
/// 
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataSetExceptionUnknownTransferSyntax: public dataSetException
{
public:
	dataSetExceptionUnknownTransferSyntax(const std::string& message): dataSetException(message){}
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This exception is thrown when the application
///         is storing several images in the dataSet but
///         doesn't store them in the right order.
///
/// The application must store the images following the 
///  frame order, without skipping frames.
/// 
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataSetExceptionWrongFrame: public dataSetException
{
public:
	dataSetExceptionWrongFrame(const std::string& message): dataSetException(message){}
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This exception is thrown when the application
///         is trying to store an image in an old Dicom
///         format.
///
/// The application cannot store images in old Dicom 
///  formats (before Dicom3).
/// 
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataSetExceptionOldFormat: public dataSetException
{
public:
	dataSetExceptionOldFormat(const std::string& message): dataSetException(message){}
};

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This exception is thrown when the an image that
///         doesn't exist is requested.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataSetImageDoesntExist: public dataSetException
{
public:
	/// \brief Build a dataSetImageDoesntExist exception
	///
	/// @param message the message to store into the exception
	///
	///////////////////////////////////////////////////////////
	dataSetImageDoesntExist(const std::string& message): dataSetException(message){}
};

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This exception is thrown when the basic offset
///         table is corrupted.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataSetCorruptedOffsetTable: public dataSetException
{
public:
	/// \brief Build a dataSetImageDoesntExist exception
	///
	/// @param message the message to store into the exception
	///
	///////////////////////////////////////////////////////////
	dataSetCorruptedOffsetTable(const std::string& message): dataSetException(message){}
};

/// @}



} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraDataSet_93F684BF_0024_4bf3_89BA_D98E82A1F44C__INCLUDED_)
