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

/*! \file dicomCodec.cpp
    \brief Implementation of the class dicomCodec.

*/

#include <list>
#include <vector>
#include <string.h>
#include "../../base/include/exception.h"
#include "../../base/include/streamReader.h"
#include "../../base/include/streamWriter.h"
#include "../../base/include/memory.h"
#include "../include/dicomCodec.h"
#include "../include/dataSet.h"
#include "../include/dicomDict.h"
#include "../include/image.h"
#include "../include/colorTransformsFactory.h"


namespace puntoexe
{

namespace imebra
{

namespace codecs
{

static registerCodec m_registerCodec(ptr<dicomCodec>(new dicomCodec));

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// dicomCodec
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
// Create another DICOM codec
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
ptr<codec> dicomCodec::createCodec()
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::createCodec");

	return ptr<codec>(new dicomCodec);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Write a Dicom stream
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::writeStream(ptr<streamWriter> pStream, ptr<dataSet> pDataSet)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::writeStream");

	// Retrieve the transfer syntax
	///////////////////////////////////////////////////////////
	std::wstring transferSyntax=pDataSet->getUnicodeString(0x0002, 0, 0x0010, 0);

	// Adjust the flags
	///////////////////////////////////////////////////////////
	bool bExplicitDataType = (transferSyntax != L"1.2.840.10008.1.2");        // Implicit VR little endian

	// Explicit VR big endian
	///////////////////////////////////////////////////////////
	streamController::tByteOrdering endianType = (transferSyntax == L"1.2.840.10008.1.2.2") ? streamController::highByteEndian : streamController::lowByteEndian;

	// Write the dicom header
	///////////////////////////////////////////////////////////
	imbxUint8 zeroBuffer[128];
	::memset(zeroBuffer, 0L, 128L);
	pStream->write(zeroBuffer, 128);

	// Write the DICM signature
	///////////////////////////////////////////////////////////
	pStream->write((imbxUint8*)"DICM", 4);

	// Build the stream
	///////////////////////////////////////////////////////////
	buildStream(pStream, pDataSet, bExplicitDataType, endianType);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Build a dicom stream, without header and DICM signature
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::buildStream(ptr<streamWriter> pStream, ptr<dataSet> pDataSet, bool bExplicitDataType, streamController::tByteOrdering endianType)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::buildStream");

	for(
		ptr<dataCollectionIterator<dataGroup> > groupsIterator = pDataSet->getDataIterator();
		groupsIterator->isValid();
		groupsIterator->incIterator())
	{
		ptr<dataGroup> pGroup = groupsIterator->getData();
		imbxUint16 groupId = groupsIterator->getId();
		writeGroup(pStream, pGroup, groupId, bExplicitDataType, endianType);
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Write a single data group
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::writeGroup(ptr<streamWriter> pDestStream, ptr<dataGroup> pGroup, imbxUint16 groupId, bool bExplicitDataType, streamController::tByteOrdering endianType)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::writeGroup");

	if(groupId == 2)
	{
		bExplicitDataType = true;
		endianType = streamController::lowByteEndian;
	}

	// Calculate the group's length
	///////////////////////////////////////////////////////////
	imbxUint32 groupLength = getGroupLength(pGroup, bExplicitDataType);

	// Write the group's length
	///////////////////////////////////////////////////////////
	static char lengthDataType[] = "UL";

	imbxUint16 adjustedGroupId = groupId;
	pDestStream->adjustEndian((imbxUint8*)&adjustedGroupId, 2, endianType);

	imbxUint16 tagId = 0;
	pDestStream->write((imbxUint8*)&adjustedGroupId, 2);
	pDestStream->write((imbxUint8*)&tagId, 2);

	if(bExplicitDataType)
	{
		pDestStream->write((imbxUint8*)&lengthDataType, 2);
		imbxUint16 tagLengthWord = 4;
		pDestStream->adjustEndian((imbxUint8*)&tagLengthWord, 2, endianType);
		pDestStream->write((imbxUint8*)&tagLengthWord, 2);
	}
	else
	{
		imbxUint16 tagLengthDword = 4;
		pDestStream->adjustEndian((imbxUint8*)&tagLengthDword, 4, endianType);
		pDestStream->write((imbxUint8*)&tagLengthDword, 4);
	}

	pDestStream->adjustEndian((imbxUint8*)&groupLength, 4, endianType);
	pDestStream->write((imbxUint8*)&groupLength, 4);

	// Write all the tags
	///////////////////////////////////////////////////////////
	for(
		ptr<dataCollectionIterator<data> > pIterator = pGroup->getDataIterator();
		pIterator->isValid();
		pIterator->incIterator())
	{
		imbxUint16 tagId = pIterator->getId();
		if(tagId == 0)
		{
			continue;
		}
		ptr<data> pData = pIterator->getData();
		pDestStream->write((imbxUint8*)&adjustedGroupId, 2);
		writeTag(pDestStream, pData, tagId, bExplicitDataType, endianType);
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Write a single tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::writeTag(ptr<streamWriter> pDestStream, ptr<data> pData, imbxUint16 tagId, bool bExplicitDataType, streamController::tByteOrdering endianType)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::writeTag");

	// Calculate the tag's length
	///////////////////////////////////////////////////////////
	bool bSequence;
	imbxUint32 tagHeader;
	imbxUint32 tagLength = getTagLength(pData, bExplicitDataType, &tagHeader, &bSequence);

	// Prepare the identifiers for the sequence (adjust the
	//  endian)
	///////////////////////////////////////////////////////////
	imbxUint16 sequenceItemGroup = 0xfffe;
	imbxUint16 sequenceItemDelimiter = 0xe000;
	imbxUint16 sequenceItemEnd = 0xe0dd;
	pDestStream->adjustEndian((imbxUint8*)&sequenceItemGroup, 2, endianType);
	pDestStream->adjustEndian((imbxUint8*)&sequenceItemDelimiter, 2, endianType);
	pDestStream->adjustEndian((imbxUint8*)&sequenceItemEnd, 2, endianType);

	// Check the data type
	///////////////////////////////////////////////////////////
	std::string dataType = pData->getDataType();
	if(!(dicomDictionary::getDicomDictionary()->isDataTypeValid(dataType)))
	{
		if(pData->getDataSet(0) != 0)
		{
			dataType = "SQ";
		}
		else
		{
			dataType = "OB";
		}
	}

	// Adjust the tag id endian and write it
	///////////////////////////////////////////////////////////
	imbxUint16 adjustedTagId = tagId;
	pDestStream->adjustEndian((imbxUint8*)&adjustedTagId, 2, endianType);
	pDestStream->write((imbxUint8*)&adjustedTagId, 2);

	// Write the data type if it is explicit
	///////////////////////////////////////////////////////////
	if(bExplicitDataType)
	{
		pDestStream->write((imbxUint8*)(dataType.c_str()), 2);

		imbxUint16 tagLengthWord = (imbxUint16)tagLength;

		if(bSequence | dicomDictionary::getDicomDictionary()->getLongLength(dataType))
		{
			imbxUint32 tagLengthDWord = bSequence ? 0xffffffff : tagLength;
			tagLengthWord = 0;
			pDestStream->adjustEndian((imbxUint8*)&tagLengthDWord, 4, endianType);
			pDestStream->write((imbxUint8*)&tagLengthWord, 2);
			pDestStream->write((imbxUint8*)&tagLengthDWord, 4);
		}
		else
		{
			pDestStream->adjustEndian((imbxUint8*)&tagLengthWord, 2, endianType);
			pDestStream->write((imbxUint8*)&tagLengthWord, 2);
		}
	}
	else
	{
		imbxUint32 tagLengthDword = bSequence ? 0xffffffff : tagLength;
		pDestStream->adjustEndian((imbxUint8*)&tagLengthDword, 4, endianType);
		pDestStream->write((imbxUint8*)&tagLengthDword, 4);
	}

	// Write all the buffers or datasets
	///////////////////////////////////////////////////////////
	for(imbxUint32 scanBuffers = 0; ; ++scanBuffers)
	{
		ptr<handlers::dataHandlerRaw> pDataHandlerRaw = pData->getDataHandlerRaw(scanBuffers, false, "");
		if(pDataHandlerRaw != 0)
		{
			imbxUint32 wordSize = dicomDictionary::getDicomDictionary()->getWordSize(dataType);
			imbxUint32 bufferSize = pDataHandlerRaw->getSize();

			// write the sequence item header
			///////////////////////////////////////////////////////////
			if(bSequence)
			{
				pDestStream->write((imbxUint8*)&sequenceItemGroup, 2);
				pDestStream->write((imbxUint8*)&sequenceItemDelimiter, 2);
				imbxUint32 sequenceItemLength = bufferSize;
				pDestStream->adjustEndian((imbxUint8*)&sequenceItemLength, 4, endianType);
				pDestStream->write((imbxUint8*)&sequenceItemLength, 4);
			}

			if(bufferSize == 0)
			{
				continue;
			}

			// Adjust the buffer's endian
			///////////////////////////////////////////////////////////
			if(wordSize > 1)
			{
				std::unique_ptr<imbxUint8> pTempBuffer(new imbxUint8[bufferSize]);
				::memcpy(pTempBuffer.get(), pDataHandlerRaw->getMemoryBuffer(), pDataHandlerRaw->getSize());
				streamController::adjustEndian(pTempBuffer.get(), wordSize, endianType, bufferSize / wordSize);
				pDestStream->write(pTempBuffer.get(), bufferSize);
				continue;
			}

			pDestStream->write((imbxUint8*)pDataHandlerRaw->getMemoryBuffer(), bufferSize);
			continue;
		}

		// Write a nested dataset
		///////////////////////////////////////////////////////////
		ptr<dataSet> pDataSet = pData->getDataSet(scanBuffers);
		if(pDataSet == 0)
		{
			break;
		}

		// Remember the position at which the item has been written
		///////////////////////////////////////////////////////////
		pDataSet->setItemOffset(pDestStream->getControlledStreamPosition());

		// write the sequence item header
		///////////////////////////////////////////////////////////
		pDestStream->write((imbxUint8*)&sequenceItemGroup, 2);
		pDestStream->write((imbxUint8*)&sequenceItemDelimiter, 2);
		imbxUint32 sequenceItemLength = getDataSetLength(pDataSet, bExplicitDataType);
		pDestStream->adjustEndian((imbxUint8*)&sequenceItemLength, 4, endianType);
		pDestStream->write((imbxUint8*)&sequenceItemLength, 4);

		// write the dataset
		///////////////////////////////////////////////////////////
		buildStream(pDestStream, pDataSet, bExplicitDataType, endianType);
	}

	// write the sequence item end marker
	///////////////////////////////////////////////////////////
	if(bSequence)
	{
		pDestStream->write((imbxUint8*)&sequenceItemGroup, 2);
		pDestStream->write((imbxUint8*)&sequenceItemEnd, 2);
		imbxUint32 sequenceItemLength = 0;
		pDestStream->write((imbxUint8*)&sequenceItemLength, 4);
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Calculate the tag's length
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dicomCodec::getTagLength(ptr<data> pData, bool bExplicitDataType, imbxUint32* pHeaderLength, bool *pbSequence)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::getTagLength");

	std::string dataType = pData->getDataType();
	*pbSequence = (dataType == "SQ");
	imbxUint32 numberOfElements = 0;
	imbxUint32 totalLength = 0;
	for(imbxUint32 scanBuffers = 0; ; ++scanBuffers, ++numberOfElements)
	{
		ptr<dataSet> pDataSet = pData->getDataSet(scanBuffers);
		if(pDataSet != 0)
		{
			totalLength += getDataSetLength(pDataSet, bExplicitDataType);
			totalLength += 8; // item tag and item length
			*pbSequence = true;
			continue;
		}
		if(!pData->bufferExists(scanBuffers))
		{
			break;
		}
		totalLength += pData->getBufferSize(scanBuffers);
	}

	(*pbSequence) |= (numberOfElements > 1);

	// Find the tag type
	bool bLongLength = dicomDictionary::getDicomDictionary()->getLongLength(dataType);

	*pHeaderLength = 8;
	if((bLongLength || (*pbSequence)) && bExplicitDataType)
	{
		(*pHeaderLength) +=4;
	}

	if(*pbSequence)
	{
		totalLength += (numberOfElements+1) * 8;
	}

	return totalLength;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Calculate the group's length
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dicomCodec::getGroupLength(ptr<dataGroup> pDataGroup, bool bExplicitDataType)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::getGroupLength");

	ptr<dataCollectionIterator<data> > pIterator = pDataGroup->getDataIterator();

	imbxUint32 totalLength = 0;

	for(;pIterator->isValid(); pIterator->incIterator())
	{
		if(pIterator->getId() == 0)
		{
			continue;
		}

		imbxUint32 tagHeaderLength;
		bool bSequence;
		totalLength += getTagLength(pIterator->getData(), bExplicitDataType, &tagHeaderLength, &bSequence);
		totalLength += tagHeaderLength;
	}

	return totalLength;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Calculate the dataset's length
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dicomCodec::getDataSetLength(ptr<dataSet> pDataSet, bool bExplicitDataType)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::getDataSetLength");

	ptr<dataCollectionIterator<dataGroup> > pIterator = pDataSet->getDataIterator();

	imbxUint32 totalLength = 0;

	while(pIterator->isValid())
	{
		totalLength += getGroupLength(pIterator->getData(), bExplicitDataType);
		totalLength += 4; // Add space for the tag 0
		if(bExplicitDataType) // Add space for the data type
		{
			totalLength += 2;
		}
		totalLength += 2; // Add space for the tag's length
		totalLength += 4; // Add space for the group's length

		pIterator->incIterator();
	}

	return totalLength;

	PUNTOEXE_FUNCTION_END();
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read a DICOM stream and fill the dataset with the
//  DICOM's content
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::readStream(ptr<streamReader> pStream, ptr<dataSet> pDataSet, imbxUint32 maxSizeBufferLoad /* = 0xffffffff */)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::readStream");

	// Save the starting position
	///////////////////////////////////////////////////////////
	imbxUint32 position=pStream->position();

	// This flag signals a failure
	///////////////////////////////////////////////////////////
	bool bFailed=false;

	// Read the old dicom signature (NEMA)
	///////////////////////////////////////////////////////////
	imbxUint8 oldDicomSignature[8];

	try
	{
		pStream->read(oldDicomSignature, 8);
	}
	catch(streamExceptionEOF&)
	{
		PUNTOEXE_THROW(codecExceptionWrongFormat, "detected a wrong format");
	}

	// Skip the first 128 bytes (8 already skipped)
	///////////////////////////////////////////////////////////
	pStream->seek(120, true);

	// Read the DICOM signature (DICM)
	///////////////////////////////////////////////////////////
	imbxUint8 dicomSignature[4];
	pStream->read(dicomSignature, 4);
	// Check the DICM signature
	///////////////////////////////////////////////////////////
	const char* checkSignature="DICM";
	if(::memcmp(dicomSignature, checkSignature, 4) != 0)
	{
		bFailed=true;
	}

	bool bExplicitDataType = true;
	streamController::tByteOrdering endianType=streamController::lowByteEndian;
	if(bFailed)
	{
		// Tags 0x8 and 0x2 are accepted in the begin of the file
		///////////////////////////////////////////////////////////
		if(
			(oldDicomSignature[0]!=0x8 && oldDicomSignature[0]!=0x2) ||
			oldDicomSignature[1]!=0x0 ||
			oldDicomSignature[3]!=0x0)
		{
			PUNTOEXE_THROW(codecExceptionWrongFormat, "detected a wrong format (checked old NEMA signature)");
		}

		// Go back to the beginning of the file
		///////////////////////////////////////////////////////////
		pStream->seek((imbxInt32)position);

		// Set "explicit data type" to true if a valid data type
                //  is found
		///////////////////////////////////////////////////////////
                std::string firstDataType;
                firstDataType.push_back(oldDicomSignature[4]);
                firstDataType.push_back(oldDicomSignature[5]);
                bExplicitDataType = dicomDictionary::getDicomDictionary()->isDataTypeValid(firstDataType);
	}

	// Signature OK. Now scan all the tags.
	///////////////////////////////////////////////////////////
	parseStream(pStream, pDataSet, bExplicitDataType, endianType, maxSizeBufferLoad);

	PUNTOEXE_FUNCTION_END();
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Parse a Dicom stream
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::parseStream(ptr<streamReader> pStream,
							 ptr<dataSet> pDataSet,
							 bool bExplicitDataType,
							 streamController::tByteOrdering endianType,
							 imbxUint32 maxSizeBufferLoad /* = 0xffffffff */,
							 imbxUint32 subItemLength /* = 0xffffffff */,
							 imbxUint32* pReadSubItemLength /* = 0 */,
							 imbxUint32 depth)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::parseStream");

	if(depth > IMEBRA_DATASET_MAX_DEPTH)
	{
		PUNTOEXE_THROW(dicomCodecExceptionDepthLimitReached, "Depth for embedded dataset reached");
	}

	imbxUint16 tagId;
	imbxUint16 tagSubId;
	imbxUint16 tagLengthWord;
	imbxUint32 tagLengthDWord;

	// Used to calculate the group order
	///////////////////////////////////////////////////////////
	imbxUint16 order = 0;
	imbxUint16 lastGroupId = 0;
	imbxUint16 lastTagId = 0;

	imbxUint32 tempReadSubItemLength = 0; // used when the last parameter is not defined
	char       tagType[3];
	bool       bStopped = false;
	bool       bFirstTag = (pReadSubItemLength == 0);
	bool       bCheckTransferSyntax = bFirstTag;
	short      wordSize;

	tagType[2] = 0;

	if(pReadSubItemLength == 0)
	{
		pReadSubItemLength = &tempReadSubItemLength;
	}
	*pReadSubItemLength = 0;

	///////////////////////////////////////////////////////////
	//
	// Read all the tags
	//
	///////////////////////////////////////////////////////////
	while(!bStopped && !pStream->endReached() && (*pReadSubItemLength < subItemLength))
	{
		// Get the tag's ID
		///////////////////////////////////////////////////////////
		pStream->read((imbxUint8*)&tagId, sizeof(tagId));
		pStream->adjustEndian((imbxUint8*)&tagId, sizeof(tagId), endianType);
		(*pReadSubItemLength) += sizeof(tagId);

		// Check for EOF
		///////////////////////////////////////////////////////////
		if(pStream->endReached())
		{
			break;
		}

		// Check the byte order
		///////////////////////////////////////////////////////////
		if(bFirstTag && tagId==0x0200)
		{
			// Reverse the last adjust
			pStream->adjustEndian((imbxUint8*)&tagId, sizeof(tagId), endianType);

			// Fix the byte adjustment
			endianType=streamController::highByteEndian;

			// Redo the byte adjustment
			pStream->adjustEndian((imbxUint8*)&tagId, sizeof(tagId), endianType);
		}

		// If this tag's id is not 0x0002, then load the
		//  transfer syntax and set the byte endian.
		///////////////////////////////////////////////////////////
		if(tagId!=0x0002 && bCheckTransferSyntax)
		{
			// Reverse the last adjust
			pStream->adjustEndian((imbxUint8*)&tagId, sizeof(tagId), endianType);

			std::wstring transferSyntax=pDataSet->getUnicodeString(0x0002, 0x0, 0x0010, 0x0);

			if(transferSyntax == L"1.2.840.10008.1.2.2")
				endianType=streamController::highByteEndian;
			if(transferSyntax == L"1.2.840.10008.1.2")
				bExplicitDataType=false;

			bCheckTransferSyntax=false;

			// Redo the byte adjustment
			pStream->adjustEndian((imbxUint8*)&tagId, sizeof(tagId), endianType);
		}

		// The first tag's ID has been read
		///////////////////////////////////////////////////////////
		bFirstTag=false;

		// Set the word's length to the default value
		///////////////////////////////////////////////////////////
		wordSize = 1;

		// Get the tag's sub ID
		///////////////////////////////////////////////////////////
		pStream->read((imbxUint8*)&tagSubId, sizeof(tagSubId));
		pStream->adjustEndian((imbxUint8*)&tagSubId, sizeof(tagSubId), endianType);
		(*pReadSubItemLength) += sizeof(tagSubId);

		// Check for the end of the dataset
		///////////////////////////////////////////////////////////
		if(tagId==0xfffe && tagSubId==0xe00d)
		{
			// skip the tag's length and exit
			imbxUint32 dummyDWord;
			pStream->read((imbxUint8*)&dummyDWord, 4);
			break;
		}

		//
		// Explicit data type
		//
		///////////////////////////////////////////////////////////
		if(bExplicitDataType && tagId!=0xfffe)
		{
			// Get the tag's type
			///////////////////////////////////////////////////////////
			pStream->read((imbxUint8*)tagType, 2);
			(*pReadSubItemLength) += 2;

			// Get the tag's length
			///////////////////////////////////////////////////////////
			pStream->read((imbxUint8*)&tagLengthWord, sizeof(tagLengthWord));
			pStream->adjustEndian((imbxUint8*)&tagLengthWord, sizeof(tagLengthWord), endianType);
			(*pReadSubItemLength) += sizeof(tagLengthWord);

			// The data type is valid
			///////////////////////////////////////////////////////////
			if(dicomDictionary::getDicomDictionary()->isDataTypeValid(tagType))
			{
				tagLengthDWord=(imbxUint32)tagLengthWord;
				wordSize = (short)dicomDictionary::getDicomDictionary()->getWordSize(tagType);
				if(dicomDictionary::getDicomDictionary()->getLongLength(tagType))
				{
					pStream->read((imbxUint8*)&tagLengthDWord, sizeof(tagLengthDWord));
					pStream->adjustEndian((imbxUint8*)&tagLengthDWord, sizeof(tagLengthDWord), endianType);
					(*pReadSubItemLength) += sizeof(tagLengthDWord);
				}
			}

			// The data type is not valid. Switch to implicit data type
			///////////////////////////////////////////////////////////
			else
			{
				if(endianType == streamController::lowByteEndian)
					tagLengthDWord=(((imbxUint32)tagLengthWord)<<16) | ((imbxUint32)tagType[0]) | (((imbxUint32)tagType[1])<<8);
				else
					tagLengthDWord=(imbxUint32)tagLengthWord | (((imbxUint32)tagType[0])<<24) | (((imbxUint32)tagType[1])<<16);
			}


		} // End of the explicit data type read block


		///////////////////////////////////////////////////////////
		//
		// Implicit data type
		//
		///////////////////////////////////////////////////////////
		else
		{
			// Get the tag's length
			///////////////////////////////////////////////////////////
			pStream->read((imbxUint8*)&tagLengthDWord, sizeof(tagLengthDWord));
			pStream->adjustEndian((imbxUint8*)&tagLengthDWord, sizeof(tagLengthDWord), endianType);
			(*pReadSubItemLength) += sizeof(tagLengthDWord);
		} // End of the implicit data type read block


		///////////////////////////////////////////////////////////
		//
		// Find the default data type and the tag's word's size
		//
		///////////////////////////////////////////////////////////
		if((!bExplicitDataType || tagId==0xfffe))
		{
			// Group length. Data type is always UL
			///////////////////////////////////////////////////////////
			if(tagSubId == 0)
			{
				tagType[0]='U';
				tagType[1]='L';
			}
			else
			{
				tagType[0]=tagType[1] = 0;
				std::string defaultType=pDataSet->getDefaultDataType(tagId, tagSubId);
				if(defaultType.length()==2L)
				{
					tagType[0]=defaultType[0];
					tagType[1]=defaultType[1];

					wordSize = (short)dicomDictionary::getDicomDictionary()->getWordSize(tagType);
				}
			}
		}

		// Check for the end of a sequence
		///////////////////////////////////////////////////////////
		if(tagId==0xfffe && tagSubId==0xe0dd)
		{
			break;
		}

		///////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////
		//
		//
		// Read the tag's buffer
		//
		//
		///////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////

		///////////////////////////////////////////////////////////
		//
		// Adjust the order when multiple groups with the same
		//  id are present.
		//
		///////////////////////////////////////////////////////////
		if(tagId<=lastGroupId && tagSubId<=lastTagId)
		{
			++order;
		}
		else
		{
			if(tagId>lastGroupId)
			{
				order = 0;
			}
		}
		lastGroupId=tagId;
		lastTagId=tagSubId;

		if(tagLengthDWord != 0xffffffff && ::memcmp(tagType, "SQ", 2) != 0)
		{
			(*pReadSubItemLength) += readTag(pStream, pDataSet, tagLengthDWord, tagId, order, tagSubId, tagType, endianType, wordSize, 0, maxSizeBufferLoad);
			continue;
		}

		///////////////////////////////////////////////////////////
		//
		// We are within an undefined-length tag or a sequence
		//
		///////////////////////////////////////////////////////////

		// Parse all the sequence's items
		///////////////////////////////////////////////////////////
		imbxUint16 subItemGroupId;
		imbxUint16 subItemTagId;
		imbxUint32 sequenceItemLength;
		imbxUint32 bufferId = 0;
		while(tagLengthDWord && !pStream->endReached())
		{
			// Remember the item's position (used by DICOMDIR
			//  structures)
			///////////////////////////////////////////////////////////
			imbxUint32 itemOffset(pStream->getControlledStreamPosition());

			// Read the sequence item's group
			///////////////////////////////////////////////////////////
			pStream->read((imbxUint8*)&subItemGroupId, sizeof(subItemGroupId));
			pStream->adjustEndian((imbxUint8*)&subItemGroupId, sizeof(subItemGroupId), endianType);
			(*pReadSubItemLength) += sizeof(subItemGroupId);

			// Read the sequence item's id
			///////////////////////////////////////////////////////////
			pStream->read((imbxUint8*)&subItemTagId, sizeof(subItemTagId));
			pStream->adjustEndian((imbxUint8*)&subItemTagId, sizeof(subItemTagId), endianType);
			(*pReadSubItemLength) += sizeof(subItemTagId);

			// Read the sequence item's length
			///////////////////////////////////////////////////////////
			pStream->read((imbxUint8*)&sequenceItemLength, sizeof(sequenceItemLength));
			pStream->adjustEndian((imbxUint8*)&sequenceItemLength, sizeof(sequenceItemLength), endianType);
			(*pReadSubItemLength) += sizeof(sequenceItemLength);

			if(tagLengthDWord!=0xffffffff)
			{
				tagLengthDWord-=8;
			}

			// check for the end of the undefined length sequence
			///////////////////////////////////////////////////////////
			if(subItemGroupId==0xfffe && subItemTagId==0xe0dd)
			{
				break;
			}

			///////////////////////////////////////////////////////////
			// Parse a sub element
			///////////////////////////////////////////////////////////
			if((sequenceItemLength == 0xffffffff) || (::memcmp(tagType, "SQ", 2) == 0))
			{
				ptr<dataSet> sequenceDataSet(new dataSet);
				sequenceDataSet->setItemOffset(itemOffset);
				imbxUint32 effectiveLength(0);
				parseStream(pStream, sequenceDataSet, bExplicitDataType, endianType, maxSizeBufferLoad, sequenceItemLength, &effectiveLength, depth + 1);
				(*pReadSubItemLength) += effectiveLength;
				if(tagLengthDWord!=0xffffffff)
					tagLengthDWord-=effectiveLength;
				ptr<data> sequenceTag=pDataSet->getTag(tagId, 0x0, tagSubId, true);
				sequenceTag->setDataSet(bufferId, sequenceDataSet);
				++bufferId;

				continue;
			}

			///////////////////////////////////////////////////////////
			// Read a buffer's element
			///////////////////////////////////////////////////////////
			sequenceItemLength=readTag(pStream, pDataSet, sequenceItemLength, tagId, order, tagSubId, tagType, endianType, wordSize, bufferId++, maxSizeBufferLoad);
			(*pReadSubItemLength) += sequenceItemLength;
			if(tagLengthDWord!=0xffffffff)
			{
				tagLengthDWord -= sequenceItemLength;
			}
		}

	} // End of the tags-read block

	PUNTOEXE_FUNCTION_END();

}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get a DICOM raw or RLE image from a dicom structure
//
//
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
ptr<image> dicomCodec::getImage(ptr<dataSet> pData, ptr<streamReader> pStream, std::string dataType)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::getImage");

	streamReader* pSourceStream = pStream.get();

	// Check for RLE compression
	///////////////////////////////////////////////////////////
	std::wstring transferSyntax = pData->getUnicodeString(0x0002, 0x0, 0x0010, 0x0);
	bool bRleCompressed = (transferSyntax == L"1.2.840.10008.1.2.5");

	// Check for color space and subsampled channels
	///////////////////////////////////////////////////////////
	std::wstring colorSpace=pData->getUnicodeString(0x0028, 0x0, 0x0004, 0x0);

	// Retrieve the number of planes
	///////////////////////////////////////////////////////////
	imbxUint8 channelsNumber=(imbxUint8)pData->getUnsignedLong(0x0028, 0x0, 0x0002, 0x0);

	// Adjust the colorspace and the channels number for old
	//  NEMA files that don't specify those data
	///////////////////////////////////////////////////////////
	if(colorSpace.empty() && (channelsNumber == 0 || channelsNumber == 1))
	{
		colorSpace = L"MONOCHROME2";
		channelsNumber = 1;
	}

	if(colorSpace.empty() && channelsNumber == 3)
	{
		colorSpace = L"RGB";
	}

	// Retrieve the image's size
	///////////////////////////////////////////////////////////
	imbxUint32 imageSizeX=pData->getUnsignedLong(0x0028, 0x0, 0x0011, 0x0);
	imbxUint32 imageSizeY=pData->getUnsignedLong(0x0028, 0x0, 0x0010, 0x0);
	if((imageSizeX == 0) || (imageSizeY == 0))
	{
		PUNTOEXE_THROW(codecExceptionCorruptedFile, "The size tags are not available");
	}

	// Check for interleaved planes.
	///////////////////////////////////////////////////////////
	bool bInterleaved(pData->getUnsignedLong(0x0028, 0x0, 0x0006, 0x0)==0x0);

	// Check for 2's complement
	///////////////////////////////////////////////////////////
	bool b2Complement=pData->getUnsignedLong(0x0028, 0x0, 0x0103, 0x0)!=0x0;

	// Retrieve the allocated/stored/high bits
	///////////////////////////////////////////////////////////
	imbxUint8 allocatedBits=(imbxUint8)pData->getUnsignedLong(0x0028, 0x0, 0x0100, 0x0);
	imbxUint8 storedBits=(imbxUint8)pData->getUnsignedLong(0x0028, 0x0, 0x0101, 0x0);
	imbxUint8 highBit=(imbxUint8)pData->getUnsignedLong(0x0028, 0x0, 0x0102, 0x0);
	if(highBit<storedBits-1)
		highBit=storedBits-1;


	// If the chrominance channels are subsampled, then find
	//  the right image's size
	///////////////////////////////////////////////////////////
	bool bSubSampledY=channelsNumber>0x1 && transforms::colorTransforms::colorTransformsFactory::isSubsampledY(colorSpace);
	bool bSubSampledX=channelsNumber>0x1 && transforms::colorTransforms::colorTransformsFactory::isSubsampledX(colorSpace);

	// Create an image
	///////////////////////////////////////////////////////////
	image::bitDepth depth;
	if(b2Complement)
	{
		if(highBit >= 16)
		{
			depth = image::depthS32;
		}
		else if(highBit >= 8)
		{
			depth = image::depthS16;
		}
		else
		{
			depth = image::depthS8;
		}
	}
	else
	{
		if(highBit >= 16)
		{
			depth = image::depthU32;
		}
		else if(highBit >= 8)
		{
			depth = image::depthU16;
		}
		else
		{
			depth = image::depthU8;
		}
	}

	ptr<image> pImage(new image);
	ptr<handlers::dataHandlerNumericBase> handler = pImage->create(imageSizeX, imageSizeY, depth, colorSpace, highBit);
	imbxUint32 tempChannelsNumber = pImage->getChannelsNumber();

	if(handler == 0 || tempChannelsNumber != channelsNumber)
	{
		PUNTOEXE_THROW(codecExceptionCorruptedFile, "Cannot allocate the image's buffer");
	}

	// Allocate the dicom channels
	///////////////////////////////////////////////////////////
	allocChannels(channelsNumber, imageSizeX, imageSizeY, bSubSampledX, bSubSampledY);

	imbxUint32 mask( (imbxUint32)0x1 << highBit );
	mask <<= 1;
	--mask;
	mask-=((imbxUint32)0x1<<(highBit+1-storedBits))-1;

	//
	// The image is not compressed
	//
	///////////////////////////////////////////////////////////
	if(!bRleCompressed)
	{
		imbxUint8 wordSizeBytes= (dataType=="OW") ? 2 : 1;

		// The planes are interleaved
		///////////////////////////////////////////////////////////
		if(bInterleaved && channelsNumber != 1)
		{
			readUncompressedInterleaved(
				channelsNumber,
				bSubSampledX,
				bSubSampledY,
				pSourceStream,
				wordSizeBytes,
				allocatedBits,
				mask);
		}
		else
		{
			readUncompressedNotInterleaved(
				channelsNumber,
				pSourceStream,
				wordSizeBytes,
				allocatedBits,
				mask);
		}
	}

	//
	// The image is RLE compressed
	//
	///////////////////////////////////////////////////////////
	else
	{
		if(bSubSampledX || bSubSampledY)
		{
			PUNTOEXE_THROW(codecExceptionCorruptedFile, "Cannot read subsampled RLE images");
		}

		readRLECompressed(imageSizeX, imageSizeY, channelsNumber, pSourceStream, allocatedBits, mask, bInterleaved);

	} // ...End of RLE decoding

	// Adjust b2complement buffers
	///////////////////////////////////////////////////////////
	if(b2Complement)
	{
		imbxInt32 checkSign = (imbxInt32)0x1<<highBit;
		imbxInt32 orMask = ((imbxInt32)-1)<<highBit;

		for(size_t adjChannels = 0; adjChannels < m_channels.size(); ++adjChannels)
		{
			imbxInt32* pAdjBuffer = m_channels[adjChannels]->m_pBuffer;
			imbxUint32 adjSize = m_channels[adjChannels]->m_bufferSize;
			while(adjSize != 0)
			{
				if(*pAdjBuffer & checkSign)
				{
					*pAdjBuffer |= orMask;
				}
				++pAdjBuffer;
				--adjSize;
			}
		}
	}


	// Copy the dicom channels into the image
	///////////////////////////////////////////////////////////
	imbxUint32 maxSamplingFactorX = bSubSampledX ? 2 : 1;
	imbxUint32 maxSamplingFactorY = bSubSampledY ? 2 : 1;
	for(imbxUint32 copyChannels = 0; copyChannels < channelsNumber; ++copyChannels)
	{
		ptrChannel dicomChannel = m_channels[copyChannels];
		handler->copyFromInt32Interleaved(
			dicomChannel->m_pBuffer,
			maxSamplingFactorX /dicomChannel->m_samplingFactorX,
			maxSamplingFactorY /dicomChannel->m_samplingFactorY,
			0, 0,
			dicomChannel->m_sizeX * maxSamplingFactorX / dicomChannel->m_samplingFactorX,
			dicomChannel->m_sizeY * maxSamplingFactorY / dicomChannel->m_samplingFactorY,
			copyChannels,
			imageSizeX,
			imageSizeY,
			channelsNumber);
	}

	// Return OK
	///////////////////////////////////////////////////////////
	return pImage;

	PUNTOEXE_FUNCTION_END();

}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Allocate the channels used to read/write an image
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::allocChannels(imbxUint32 channelsNumber, imbxUint32 sizeX, imbxUint32 sizeY, bool bSubSampledX, bool bSubSampledY)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::allocChannels");

	if(bSubSampledX && (sizeX & 0x1) != 0)
	{
		++sizeX;
	}

	if(bSubSampledY && (sizeY & 0x1) != 0)
	{
		++sizeY;
	}

	m_channels.resize(channelsNumber);
	for(imbxUint32 channelNum = 0; channelNum < channelsNumber; ++channelNum)
	{
		ptrChannel newChannel(new channel);
		imbxUint32 channelSizeX = sizeX;
		imbxUint32 channelSizeY = sizeY;
		imbxUint32 samplingFactorX = 1;
		imbxUint32 samplingFactorY = 1;
		if(channelNum != 0)
		{
			if(bSubSampledX)
			{
				channelSizeX >>= 1;
			}
			if(bSubSampledY)
			{
				channelSizeY >>= 1;
			}
		}
		else
		{
			if(bSubSampledX)
			{
				++samplingFactorX;
			}
			if(bSubSampledY)
			{
				++samplingFactorY;
			}
		}
		newChannel->allocate(channelSizeX, channelSizeY);

		if(channelNum == 0)
		newChannel->m_samplingFactorX = samplingFactorX;
		newChannel->m_samplingFactorY = samplingFactorY;

		m_channels[channelNum] = newChannel;
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read an uncompressed interleaved image
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::readUncompressedInterleaved(
	imbxUint32 channelsNumber,
	bool bSubSampledX,
	bool bSubSampledY,
	streamReader* pSourceStream,
	imbxUint8 wordSizeBytes,
	imbxUint8 allocatedBits,
	imbxUint32 mask
	)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::readUncompressedInterleaved");

	imbxUint8  bitPointer=0x0;

	std::unique_ptr<imbxInt32*> autoPtrChannelsMemory(new imbxInt32*[m_channels.size()]);
	imbxInt32** channelsMemory = autoPtrChannelsMemory.get();
	for(size_t copyChannelsPntr = 0; copyChannelsPntr < m_channels.size(); ++copyChannelsPntr)
	{
		channelsMemory[copyChannelsPntr] = m_channels[copyChannelsPntr]->m_pBuffer;
	}

	// No subsampling here
	///////////////////////////////////////////////////////////
	if(!bSubSampledX && !bSubSampledY)
	{
		imbxUint8 readBuffer[4];
		for(imbxUint32 totalSize = m_channels[0]->m_bufferSize; totalSize != 0; --totalSize)
		{
			for(imbxUint32 scanChannels = 0; scanChannels != channelsNumber; ++scanChannels)
			{
                            readPixel(pSourceStream, channelsMemory[scanChannels]++, 1, &bitPointer, readBuffer, (allocatedBits >> 3), wordSizeBytes, allocatedBits, mask);
			}
		}
		return;
	}

        imbxUint32 numValuesPerBlock(channelsNumber);
        if(bSubSampledX)
        {
            ++numValuesPerBlock;
        }
        if(bSubSampledY)
        {
            numValuesPerBlock += 2;
        }
        std::unique_ptr<imbxInt32> readBlockValuesAutoPtr(new imbxInt32[numValuesPerBlock]);

        // Read the subsampled channels.
	// Find the number of blocks to read
	///////////////////////////////////////////////////////////
	imbxUint32 adjSizeX = m_channels[0]->m_sizeX;
	imbxUint32 adjSizeY = m_channels[0]->m_sizeY;

	imbxUint32 maxSamplingFactorX = bSubSampledX ? 2 : 1;
	imbxUint32 maxSamplingFactorY = bSubSampledY ? 2 : 1;

	ptr<memory> readBuffer(memoryPool::getMemoryPool()->getMemory(numValuesPerBlock * ((7+allocatedBits) >> 3)));

	// Read all the blocks
	///////////////////////////////////////////////////////////
	for(
		imbxUint32 numBlocks = (adjSizeX / maxSamplingFactorX) * (adjSizeY / maxSamplingFactorY);
		numBlocks != 0;
		--numBlocks)
	{
        imbxInt32* readBlockValuesPtr(readBlockValuesAutoPtr.get());
		readPixel(pSourceStream, readBlockValuesPtr, numValuesPerBlock, &bitPointer, readBuffer->data(), readBuffer->size(), wordSizeBytes, allocatedBits, mask);

		// Read channel 0 (not subsampled)
		///////////////////////////////////////////////////////////
		*(channelsMemory[0]++) = *readBlockValuesPtr++;
		if(bSubSampledX)
		{
			*(channelsMemory[0]++) = *readBlockValuesPtr++;
		}
		if(bSubSampledY)
		{
			*(channelsMemory[0]+adjSizeX-2) = *readBlockValuesPtr++;
			*(channelsMemory[0]+adjSizeX-1) = *readBlockValuesPtr++;
		}
		// Read channels 1... (subsampled)
		///////////////////////////////////////////////////////////
		for(imbxUint32 scanSubSampled = 1; scanSubSampled < channelsNumber; ++scanSubSampled)
		{
			*(channelsMemory[scanSubSampled]++) = *readBlockValuesPtr++;
		}
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Write an uncompressed interleaved image
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::writeUncompressedInterleaved(
	imbxUint32 channelsNumber,
	bool bSubSampledX,
	bool bSubSampledY,
	streamWriter* pDestStream,
	imbxUint8 wordSizeBytes,
	imbxUint8 allocatedBits,
	imbxUint32 mask
	)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::writeUncompressedInterleaved");

	imbxUint8  bitPointer=0x0;

	std::unique_ptr<imbxInt32*> autoPtrChannelsMemory(new imbxInt32*[m_channels.size()]);
	imbxInt32** channelsMemory = autoPtrChannelsMemory.get();
	for(size_t copyChannelsPntr = 0; copyChannelsPntr < m_channels.size(); ++copyChannelsPntr)
	{
		channelsMemory[copyChannelsPntr] = m_channels[copyChannelsPntr]->m_pBuffer;
	}

	// No subsampling here
	///////////////////////////////////////////////////////////
	if(!bSubSampledX && !bSubSampledY)
	{
		for(imbxUint32 totalSize = m_channels[0]->m_bufferSize; totalSize != 0; --totalSize)
		{
			for(imbxUint32 scanChannels = 0; scanChannels < channelsNumber; ++scanChannels)
			{
				writePixel(pDestStream, *(channelsMemory[scanChannels]++), &bitPointer, wordSizeBytes, allocatedBits, mask);
			}
		}
		flushUnwrittenPixels(pDestStream, &bitPointer, wordSizeBytes);
		return;
	}

	// Write the subsampled channels.
	// Find the number of blocks to write
	///////////////////////////////////////////////////////////
	imbxUint32 adjSizeX = m_channels[0]->m_sizeX;
	imbxUint32 adjSizeY = m_channels[0]->m_sizeY;

	imbxUint32 maxSamplingFactorX = bSubSampledX ? 2 : 1;
	imbxUint32 maxSamplingFactorY = bSubSampledY ? 2 : 1;

	// Write all the blocks
	///////////////////////////////////////////////////////////
	for(
		imbxUint32 numBlocks = (adjSizeX / maxSamplingFactorX) * (adjSizeY / maxSamplingFactorY);
		numBlocks != 0;
		--numBlocks)
	{
		// Write channel 0 (not subsampled)
		///////////////////////////////////////////////////////////
		writePixel(pDestStream, *(channelsMemory[0]++), &bitPointer, wordSizeBytes, allocatedBits, mask);
		if(bSubSampledX)
		{
			writePixel(pDestStream, *(channelsMemory[0]++), &bitPointer, wordSizeBytes, allocatedBits, mask);
		}
		if(bSubSampledY)
		{
			writePixel(pDestStream, *(channelsMemory[0]+adjSizeX-2), &bitPointer, wordSizeBytes, allocatedBits, mask);
			writePixel(pDestStream, *(channelsMemory[0]+adjSizeX-1), &bitPointer, wordSizeBytes, allocatedBits, mask);
		}
		// Write channels 1... (subsampled)
		///////////////////////////////////////////////////////////
		for(imbxUint32 scanSubSampled = 1; scanSubSampled < channelsNumber; ++scanSubSampled)
		{
			writePixel(pDestStream, *(channelsMemory[scanSubSampled]++), &bitPointer, wordSizeBytes, allocatedBits, mask);
		}
	}

	flushUnwrittenPixels(pDestStream, &bitPointer, wordSizeBytes);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read an uncompressed not interleaved image
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::readUncompressedNotInterleaved(
	imbxUint32 channelsNumber,
	streamReader* pSourceStream,
	imbxUint8 wordSizeBytes,
	imbxUint8 allocatedBits,
	imbxUint32 mask
	)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::readUncompressedNotInterleaved");

	imbxUint8  bitPointer=0x0;

	ptr<memory> readBuffer;
	imbxUint32 lastBufferSize(0);

	// Read all the pixels
	///////////////////////////////////////////////////////////
	for(imbxUint32 channel = 0; channel < channelsNumber; ++channel)
	{
		if(m_channels[channel]->m_bufferSize != lastBufferSize)
		{
			lastBufferSize = m_channels[channel]->m_bufferSize;
			readBuffer = memoryPool::getMemoryPool()->getMemory(lastBufferSize * ((7+allocatedBits) >> 3));
		}
		imbxInt32* pMemoryDest = m_channels[channel]->m_pBuffer;
		readPixel(pSourceStream, pMemoryDest, m_channels[channel]->m_bufferSize, &bitPointer, readBuffer->data(), readBuffer->size(), wordSizeBytes, allocatedBits, mask);
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Write an uncompressed not interleaved image
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::writeUncompressedNotInterleaved(
	imbxUint32 channelsNumber,
	streamWriter* pDestStream,
	imbxUint8 wordSizeBytes,
	imbxUint8 allocatedBits,
	imbxUint32 mask
	)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::writeUncompressedNotInterleaved");

	imbxUint8  bitPointer=0x0;

	// Write all the pixels
	///////////////////////////////////////////////////////////
	for(imbxUint32 channel = 0; channel < channelsNumber; ++channel)
	{
		imbxInt32* pMemoryDest = m_channels[channel]->m_pBuffer;
		for(imbxUint32 scanPixels = m_channels[channel]->m_bufferSize; scanPixels != 0; --scanPixels)
		{
			writePixel(pDestStream, *pMemoryDest++, &bitPointer, wordSizeBytes, allocatedBits, mask);
		}
	}
	flushUnwrittenPixels(pDestStream, &bitPointer, wordSizeBytes);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Write a RLE compressed image
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::writeRLECompressed(
	imbxUint32 imageSizeX,
	imbxUint32 imageSizeY,
	imbxUint32 channelsNumber,
	streamWriter* pDestStream,
	imbxUint8 allocatedBits,
	imbxUint32 mask
	)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::writeRLECompressed");

	imbxUint32 segmentsOffset[16];
	::memset(segmentsOffset, 0, sizeof(segmentsOffset));

	// The first phase fills the segmentsOffset pointers, the
	//  second phase writes to the stream.
	///////////////////////////////////////////////////////////
	for(int phase = 0; phase < 2; ++phase)
	{
		if(phase == 1)
		{
			pDestStream->adjustEndian((imbxUint8*)segmentsOffset, 4, streamController::lowByteEndian, sizeof(segmentsOffset) / sizeof(segmentsOffset[0]));
			pDestStream->write((imbxUint8*)segmentsOffset, sizeof(segmentsOffset));
		}

		imbxUint32 segmentNumber = 0;
		imbxUint32 offset = 64;
		imbxUint8 command;

		for(imbxUint32 scanChannels = 0; scanChannels < channelsNumber; ++scanChannels)
		{
			std::unique_ptr<imbxUint8> rowBytes(new imbxUint8[imageSizeX]);
			imbxUint8* pRowBytes = rowBytes.get();

			for(imbxInt32 rightShift = ((allocatedBits + 7) & 0xfffffff8) -8; rightShift >= 0; rightShift -= 8)
			{
				imbxInt32* pPixel = m_channels[scanChannels]->m_pBuffer;

				if(phase == 0)
				{
					segmentsOffset[++segmentNumber] = offset;
					segmentsOffset[0] = segmentNumber;
				}
				else
				{
					offset = segmentsOffset[++segmentNumber];
				}

				for(imbxUint32 scanY = imageSizeY; scanY != 0; --scanY)
				{
					imbxUint8* rowBytesPointer = pRowBytes;

					for(imbxUint32 scanX = imageSizeX; scanX != 0; --scanX)
					{
						*(rowBytesPointer++) = (imbxUint8)((*pPixel & mask) >> rightShift);
						++pPixel;
					}

					for(imbxUint32 scanBytes = 0; scanBytes < imageSizeX; /* left empty */)
					{
						// Find the next start of consecutive bytes with the
						//  same value
						imbxUint32 startRun = scanBytes;
						imbxUint32 runLength = 0;
						while(startRun < imageSizeX)
						{
							imbxUint32 analyzeRun = startRun + 1;
							imbxUint8 runByte = pRowBytes[startRun];
							while(analyzeRun < imageSizeX && pRowBytes[analyzeRun] == runByte)
							{
								++analyzeRun;
							}
							if(analyzeRun - startRun > 3)
							{
								runLength = analyzeRun - startRun;
								break;
							}
							startRun = analyzeRun;
						}

						while(scanBytes < startRun)
						{
							imbxUint32 writeBytes = startRun - scanBytes;
							if(writeBytes > 0x00000080)
							{
								writeBytes = 0x00000080;
							}

							offset += 1 + startRun - scanBytes;
							if(phase == 1)
							{
								command = (imbxUint8)writeBytes;
								--command;
								pDestStream->write(&command, 1);
								pDestStream->write(&(pRowBytes[scanBytes]), startRun - scanBytes);
							}

							scanBytes += writeBytes;
						}

						// Write a run length
						if(startRun >= imageSizeX)
						{
							continue;
						}
						if(runLength > 0x00000080)
						{
							runLength = 0x00000080;
						}

						offset += 2;
						if(phase == 1)
						{
							command = 0xff - (imbxUint8)(runLength - 2);
							pDestStream->write(&command, 1);
							pDestStream->write(&(pRowBytes[scanBytes]), 1);
						}

						scanBytes += runLength;

					} // for(imbxUint32 scanBytes = 0; scanBytes < imageSizeX; )

				} // for(imbxUint32 scanY = imageSizeY; scanY != 0; --scanY)

				if(offset & 0x00000001)
				{
					++offset;
					if(phase == 1)
					{
						imbxUint8 command = 0x80;
						pDestStream->write(&command, 1);
					}
				}

			} // for(imbxInt32 rightShift = ((allocatedBits + 7) & 0xfffffff8) -8; rightShift >= 0; rightShift -= 8)

		} // for(int scanChannels = 0; scanChannels < channelsNumber; ++scanChannels)

	} // for(int phase = 0; phase < 2; ++phase)

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read a RLE compressed image
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::readRLECompressed(
	imbxUint32 imageSizeX,
	imbxUint32 imageSizeY,
	imbxUint32 channelsNumber,
	streamReader* pSourceStream,
	imbxUint8 allocatedBits,
	imbxUint32 mask,
	bool /*bInterleaved*/)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::readRLECompressed");

	// Copy the RLE header into the segmentsOffset array
	//  and adjust the byte endian to the machine architecture
	///////////////////////////////////////////////////////////
	imbxUint32 segmentsOffset[16];
	::memset(segmentsOffset, 0, sizeof(segmentsOffset));
	pSourceStream->read((imbxUint8*)segmentsOffset, 64);
    pSourceStream->adjustEndian((imbxUint8*)segmentsOffset, 4, streamController::lowByteEndian, sizeof(segmentsOffset) / sizeof(segmentsOffset[0]));

	//
	// Scan all the RLE segments
	//
	///////////////////////////////////////////////////////////
	imbxUint32 loopsNumber = channelsNumber;
	imbxUint32 loopSize = imageSizeX * imageSizeY;

	imbxUint32 currentSegmentOffset = sizeof(segmentsOffset);
	imbxUint8 segmentNumber = 0;
	for(imbxUint32 channel = 0; channel<loopsNumber; ++channel)
	{
		for(imbxInt32 leftShift = ((allocatedBits + 7) & 0xfffffff8) -8; leftShift >= 0; leftShift -= 8)
		{
			// Prepare to scan all the RLE segment
			///////////////////////////////////////////////////////////
			imbxUint32 segmentOffset=segmentsOffset[++segmentNumber]; // Get the offset
			pSourceStream->seek(segmentOffset - currentSegmentOffset, true);
			currentSegmentOffset = segmentOffset;

			imbxUint8  rleByte = 0;         // RLE code
			imbxUint8  copyBytes = 0;       // Number of bytes to copy
			imbxUint8  runByte = 0;         // Byte to use in run-lengths
			imbxUint8  runLength = 0;       // Number of bytes with the same information (runByte)
			imbxUint8  copyBytesBuffer[0x81];

			imbxInt32* pChannelMemory = m_channels[channel]->m_pBuffer;
			imbxUint32 channelSize = loopSize;
			imbxUint8* pScanCopyBytes;

			// Read the RLE segment
			///////////////////////////////////////////////////////////
			pSourceStream->read(&rleByte, 1);
			++currentSegmentOffset;
			while(channelSize != 0)
			{
				if(rleByte==0x80)
				{
				    pSourceStream->read(&rleByte, 1);
					++currentSegmentOffset;
					continue;
				}

				// Copy the specified number of bytes
				///////////////////////////////////////////////////////////
				if(rleByte<0x80)
				{
					copyBytes = ++rleByte;
					if(copyBytes < channelSize)
					{
					    pSourceStream->read(copyBytesBuffer, copyBytes + 1);
						currentSegmentOffset += copyBytes + 1;
						rleByte = copyBytesBuffer[copyBytes];
					}
					else
					{
					    pSourceStream->read(copyBytesBuffer, copyBytes);
						currentSegmentOffset += copyBytes;
					}
					pScanCopyBytes = copyBytesBuffer;
					while(copyBytes-- && channelSize != 0)
					{
						*pChannelMemory |= ((*pScanCopyBytes++) << leftShift) & mask;
						++pChannelMemory;
						--channelSize;
					}
					continue;
				}

				// Copy the same byte several times
				///////////////////////////////////////////////////////////
				runLength = (imbxUint8)0x1-rleByte;
				if(runLength < channelSize)
				{
				    pSourceStream->read(copyBytesBuffer, 2);
					currentSegmentOffset += 2;
					runByte = copyBytesBuffer[0];
					rleByte = copyBytesBuffer[1];
				}
				else
				{
				    pSourceStream->read(&runByte, 1);
					++currentSegmentOffset;
				}
				while(runLength-- && channelSize != 0)
				{
					*pChannelMemory |= (runByte << leftShift) & mask;
					++pChannelMemory;
					--channelSize;
				}

			} // ...End of the segment scanning loop

		} // ...End of the leftshift calculation

	} // ...Channels scanning loop

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read a single component from a DICOM raw image
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::readPixel(
					streamReader* pSourceStream,
					imbxInt32* pDest,
					imbxUint32 numPixels,
					imbxUint8* pBitPointer,
					imbxUint8* pReadBuffer,
					const imbxUint32 readBufferSize,
					const imbxUint8 wordSizeBytes,
					const imbxUint8 allocatedBits,
					const imbxUint32 mask)
{
	if(allocatedBits == 8 || allocatedBits == 16 || allocatedBits == 32)
	{
		pSourceStream->read(pReadBuffer, numPixels * (allocatedBits >> 3));
		if(allocatedBits == 8)
		{
			imbxUint8* pSource(pReadBuffer);
			while(numPixels-- != 0)
			{
				*pDest++ = (imbxUint32)(*pSource++) & mask;
			}
			return;
		}
		pSourceStream->adjustEndian(pReadBuffer, allocatedBits >> 3, streamController::lowByteEndian, numPixels);
		if(allocatedBits == 16)
		{
			imbxUint16* pSource((imbxUint16*)(pReadBuffer));
			while(numPixels-- != 0)
			{
				*pDest++ = (imbxUint32)(*pSource++) & mask;
			}
			return;
		}
		imbxUint32* pSource((imbxUint32*)(pReadBuffer));
		while(numPixels-- != 0)
		{
			*pDest++ = (*pSource++) & mask;
		}
		return;

	}


        while(numPixels-- != 0)
        {
            *pDest = 0;
            for(imbxUint8 bitsToRead = allocatedBits; bitsToRead != 0;)
            {
                if(*pBitPointer == 0)
                {
                    if(wordSizeBytes==0x2)
                    {
                        pSourceStream->read((imbxUint8*)&m_ioWord, sizeof(m_ioWord));
                        *pBitPointer = 16;
                    }
                    else
                    {
                        pSourceStream->read(&m_ioByte, 1);
                        m_ioWord = (imbxUint16)m_ioByte;
                        *pBitPointer = 8;
                    }
                }

                if(*pBitPointer <= bitsToRead)
                {
                    *pDest |= m_ioWord << (allocatedBits - bitsToRead);
                    bitsToRead -= *pBitPointer;
                    *pBitPointer = 0;
                    continue;
                }

                *pDest |= (m_ioWord & (((imbxUint16)1<<bitsToRead) - 1)) << (allocatedBits - bitsToRead);
                m_ioWord >>= bitsToRead;
                *pBitPointer -= bitsToRead;
                bitsToRead = 0;
            }
            *pDest++ &= mask;
        }
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read a single component from a DICOM raw image
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::writePixel(
					streamWriter* pDestStream,
					imbxInt32 pixelValue,
					imbxUint8*  pBitPointer,
					imbxUint8 wordSizeBytes,
					imbxUint8 allocatedBits,
					imbxUint32 mask)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::writePixel");

	pixelValue &= mask;

	if(allocatedBits == 8)
	{
		m_ioByte = (imbxUint8)pixelValue;
		pDestStream->write(&m_ioByte, sizeof(m_ioByte));
		return;
	}

	if(allocatedBits == 16)
	{
		m_ioWord = (imbxUint16)pixelValue;
		if(wordSizeBytes == 1)
		{
			pDestStream->adjustEndian((imbxUint8*)&m_ioWord, 2, streamController::lowByteEndian);
		}
		pDestStream->write((imbxUint8*)&m_ioWord, sizeof(m_ioWord));
		return;
	}

	if(allocatedBits == 32)
	{
		m_ioDWord = (imbxUint32)pixelValue;
		if(wordSizeBytes == 1)
		{
			pDestStream->adjustEndian((imbxUint8*)&m_ioDWord, 4, streamController::lowByteEndian);
		}
		pDestStream->write((imbxUint8*)&m_ioDWord, sizeof(m_ioDWord));
		return;
	}

	imbxUint8 maxBits = (imbxUint8)(wordSizeBytes << 3);

	for(imbxUint8 writeBits = allocatedBits; writeBits != 0;)
	{
		imbxUint8 freeBits = maxBits - *pBitPointer;
		if(freeBits == maxBits)
		{
			m_ioWord = 0;
		}
		if( freeBits <= writeBits )
		{
			m_ioWord |= (pixelValue & (((imbxInt32)1 << freeBits) -1 ))<< (*pBitPointer);
			*pBitPointer = maxBits;
			writeBits -= freeBits;
			pixelValue >>= freeBits;
		}
		else
		{
			m_ioWord |= (pixelValue & (((imbxInt32)1 << writeBits) -1 ))<< (*pBitPointer);
			(*pBitPointer) += writeBits;
			writeBits = 0;
		}

		if(*pBitPointer == maxBits)
		{
			if(wordSizeBytes == 2)
			{
				pDestStream->write((imbxUint8*)&m_ioWord, 2);
			}
			else
			{
				m_ioByte = (imbxUint8)m_ioWord;
				pDestStream->write(&m_ioByte, 1);
			}
			*pBitPointer = 0;
		}
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Used by the writing routines to commit the unwritten
//  bits
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::flushUnwrittenPixels(streamWriter* pDestStream, imbxUint8* pBitPointer, imbxUint8 wordSizeBytes)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::flushUnwrittenPixels");

	if(*pBitPointer == 0)
	{
		return;
	}
	if(wordSizeBytes == 2)
	{
		pDestStream->write((imbxUint8*)&m_ioWord, 2);
	}
	else if(wordSizeBytes == 4)
	{
		pDestStream->write((imbxUint8*)&m_ioDWord, 4);
	}
	else
	{
		m_ioByte = (imbxUint8)m_ioWord;
		pDestStream->write(&m_ioByte, 1);
	}
	*pBitPointer = 0;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Insert an image into a Dicom structure
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dicomCodec::setImage(
		ptr<streamWriter> pDestStream,
		ptr<image> pImage,
		std::wstring transferSyntax,
		quality /*imageQuality*/,
		std::string dataType,
		imbxUint8 allocatedBits,
		bool bSubSampledX,
		bool bSubSampledY,
		bool bInterleaved,
		bool /*b2Complement*/)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::setImage");

	// First calculate the attributes we want to use.
	// Return an exception if they are different from the
	//  old ones and bDontChangeAttributes is true
	///////////////////////////////////////////////////////////
	imbxUint32 imageWidth, imageHeight;
	pImage->getSize(&imageWidth, &imageHeight);

	std::wstring colorSpace = pImage->getColorSpace();
	imbxUint32 highBit = pImage->getHighBit();
	bool bRleCompressed = (transferSyntax == L"1.2.840.10008.1.2.5");

	imbxUint32 rowSize, channelPixelSize, channelsNumber;
	ptr<handlers::dataHandlerNumericBase> imageHandler = pImage->getDataHandler(false, &rowSize, &channelPixelSize, &channelsNumber);

	// Copy the image into the dicom channels
	///////////////////////////////////////////////////////////
	allocChannels(channelsNumber, imageWidth, imageHeight, bSubSampledX, bSubSampledY);
	imbxUint32 maxSamplingFactorX = bSubSampledX ? 2 : 1;
	imbxUint32 maxSamplingFactorY = bSubSampledY ? 2 : 1;
	for(imbxUint32 copyChannels = 0; copyChannels < channelsNumber; ++copyChannels)
	{
		ptrChannel dicomChannel = m_channels[copyChannels];
		imageHandler->copyToInt32Interleaved(
			dicomChannel->m_pBuffer,
			maxSamplingFactorX /dicomChannel->m_samplingFactorX,
			maxSamplingFactorY /dicomChannel->m_samplingFactorY,
			0, 0,
			dicomChannel->m_sizeX * maxSamplingFactorX / dicomChannel->m_samplingFactorX,
			dicomChannel->m_sizeY * maxSamplingFactorY / dicomChannel->m_samplingFactorY,
			copyChannels,
			imageWidth,
			imageHeight,
			channelsNumber);
	}

	imbxUint32 mask = ((imbxUint32)1 << (highBit + 1)) - 1;

	if(bRleCompressed)
	{
		writeRLECompressed(
			imageWidth,
			imageHeight,
			channelsNumber,
			pDestStream.get(),
			allocatedBits,
			mask);
		return;
	}

	imbxUint8 wordSizeBytes = ((dataType == "OW") || (dataType == "SS") || (dataType == "US")) ? 2 : 1;

	if(bInterleaved || channelsNumber == 1)
	{
		writeUncompressedInterleaved(
			channelsNumber,
			bSubSampledX, bSubSampledY,
			pDestStream.get(),
			wordSizeBytes,
			allocatedBits,
			mask);
		return;
	}

	writeUncompressedNotInterleaved(
		channelsNumber,
		pDestStream.get(),
		wordSizeBytes,
		allocatedBits,
		mask);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Returns true if the codec can handle the transfer
//  syntax
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool dicomCodec::canHandleTransferSyntax(std::wstring transferSyntax)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::canHandleTransferSyntax");

	return(
		transferSyntax == L"" ||
		transferSyntax == L"1.2.840.10008.1.2" ||      // Implicit VR little endian
		transferSyntax == L"1.2.840.10008.1.2.1" ||    // Explicit VR little endian
		// transferSyntax==L"1.2.840.10008.1.2.1.99" || // Deflated explicit VR little endian
		transferSyntax == L"1.2.840.10008.1.2.2" ||    // Explicit VR big endian
		transferSyntax == L"1.2.840.10008.1.2.5");     // RLE compression

	PUNTOEXE_FUNCTION_END();
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
//
// Returns true if the transfer syntax has to be
//  encapsulated
//
//
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
bool dicomCodec::encapsulated(std::wstring transferSyntax)
{
	PUNTOEXE_FUNCTION_START(L"jpegCodec::canHandleTransferSyntax");

	if(!canHandleTransferSyntax(transferSyntax))
	{
		PUNTOEXE_THROW(codecExceptionWrongTransferSyntax, "Cannot handle the transfer syntax");
	}
	return (transferSyntax == L"1.2.840.10008.1.2.5");

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return the highest bit number that can be handled
//  by the codec
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dicomCodec::getMaxHighBit(std::string /* transferSyntax */)
{
	return 15;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Suggest the number of allocated bits
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dicomCodec::suggestAllocatedBits(std::wstring transferSyntax, imbxUint32 highBit)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::suggestAllocatedBits");

	if(transferSyntax == L"1.2.840.10008.1.2.5")
	{
		return (highBit + 8) & 0xfffffff8;
	}

	return highBit + 1;

	PUNTOEXE_FUNCTION_END();

}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read a single tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dicomCodec::readTag(
	ptr<streamReader> pStream,
	ptr<dataSet> pDataSet,
	imbxUint32 tagLengthDWord,
	imbxUint16 tagId,
	imbxUint16 order,
	imbxUint16 tagSubId,
	std::string tagType,
	streamController::tByteOrdering endianType,
	short wordSize,
	imbxUint32 bufferId,
	imbxUint32 maxSizeBufferLoad /* = 0xffffffff */
	)
{
	PUNTOEXE_FUNCTION_START(L"dicomCodec::readTag");

	// If the tag's size is bigger than the maximum loadable
	//  size then just specify in which file it resides
	///////////////////////////////////////////////////////////
	if(tagLengthDWord > maxSizeBufferLoad)
	{
		imbxUint32 bufferPosition(pStream->position());
		imbxUint32 streamPosition(pStream->getControlledStreamPosition());
		pStream->seek(tagLengthDWord, true);
		imbxUint32 bufferLength(pStream->position() - bufferPosition);

		if(bufferLength != tagLengthDWord)
		{
			PUNTOEXE_THROW(codecExceptionCorruptedFile, "dicomCodec::readTag detected a corrupted tag");
		}

		ptr<dataGroup> writeGroup(pDataSet->getGroup(tagId, order, true));
		ptr<data>      writeData (writeGroup->getTag(tagSubId, true));
		ptr<buffer> newBuffer(
			new buffer(
				writeData,
				tagType,
				pStream->getControlledStream(),
				streamPosition,
				bufferLength,
				wordSize,
				endianType));

		writeData->setBuffer(bufferId, newBuffer);

		return bufferLength;
	}

	// Allocate the tag's buffer
	///////////////////////////////////////////////////////////
	ptr<handlers::dataHandlerRaw> handler(pDataSet->getDataHandlerRaw(tagId, order, tagSubId, bufferId, true, tagType));

	// Do nothing if the tag's size is 0
	///////////////////////////////////////////////////////////
	if(tagLengthDWord == 0)
	{
		return 0;
	}

	// In order to deal with damaged tags asking for an
	//  incredible amount of memory, this function reads the
	//  tag using a lot of small buffers (32768 bytes max)
	//  and then the tag's buffer is rebuilt at the end of the
	//  function.
	// This method saves a lot of time if a huge amount of
	//  memory is asked by a damaged tag, since only the amount
	//  of memory actually stored in the source file is
	//  allocated
	///////////////////////////////////////////////////////////

	// If the buffer size is bigger than the following const
	//  variable, then read the buffer in small chunks
	///////////////////////////////////////////////////////////
	static const imbxUint32 smallBuffersSize(32768);

	if(tagLengthDWord <= smallBuffersSize) // Read in one go
	{
		handler->setSize(tagLengthDWord);
		pStream->read(handler->getMemoryBuffer(), tagLengthDWord);
	}
	else // Read in small chunks
	{
		std::list<std::vector<imbxUint8> > buffers;

		// Used to keep track of the read bytes
		///////////////////////////////////////////////////////////
		imbxUint32 remainingBytes(tagLengthDWord);

		// Fill all the small buffers
		///////////////////////////////////////////////////////////
		while(remainingBytes != 0)
		{
			// Calculate the small buffer's size and allocate it
			///////////////////////////////////////////////////////////
			imbxUint32 thisBufferSize( (remainingBytes > smallBuffersSize) ? smallBuffersSize : remainingBytes);
			buffers.push_back(std::vector<imbxUint8>());
			buffers.back().resize(thisBufferSize);

			// Fill the buffer
			///////////////////////////////////////////////////////////
			pStream->read(&buffers.back()[0], thisBufferSize);

			// Decrease the number of the remaining bytes
			///////////////////////////////////////////////////////////
			remainingBytes -= thisBufferSize;
		}

		// Copy the small buffers into the tag object
		///////////////////////////////////////////////////////////
		handler->setSize(tagLengthDWord);
		imbxUint8* pHandlerBuffer(handler->getMemoryBuffer());

		// Scan all the small buffers and copy their content into
		//  the final buffer
		///////////////////////////////////////////////////////////
		std::list<std::vector<imbxUint8> >::iterator smallBuffersIterator;
		remainingBytes = tagLengthDWord;
		for(smallBuffersIterator=buffers.begin(); smallBuffersIterator != buffers.end(); ++smallBuffersIterator)
		{
			imbxUint32 copySize=(remainingBytes>smallBuffersSize) ? smallBuffersSize : remainingBytes;
			::memcpy(pHandlerBuffer, &(*smallBuffersIterator)[0], copySize);
			pHandlerBuffer += copySize;
			remainingBytes -= copySize;
		}
	} // end of reading from stream

	// All the bytes have been read, now rebuild the tag's
	//  buffer. Don't rebuild the tag if it is 0xfffc,0xfffc
	//  (end of the stream)
	///////////////////////////////////////////////////////////
	if(tagId == 0xfffc && tagSubId == 0xfffc)
	{
		return (imbxUint32)tagLengthDWord;
	}

	// Adjust the buffer's byte endian
	///////////////////////////////////////////////////////////
	if(wordSize != 0)
	{
		pStream->adjustEndian(handler->getMemoryBuffer(), wordSize, endianType, tagLengthDWord / wordSize);
	}

	// Return the tag's length in bytes
	///////////////////////////////////////////////////////////
	return (imbxUint32)tagLengthDWord;

	PUNTOEXE_FUNCTION_END();
}

} // namespace codecs

} // namespace imebra

} // namespace puntoexe

