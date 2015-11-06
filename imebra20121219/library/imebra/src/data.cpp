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

/*! \file data.cpp
    \brief Implementation of the data class.

*/

#include "../../base/include/exception.h"
#include "../../base/include/streamReader.h"
#include "../../base/include/streamWriter.h"
#include "../include/data.h"
#include "../include/dataSet.h"
#include "../include/buffer.h"
#include "../include/dataHandler.h"
#include "../include/dataHandlerNumeric.h"
#include <iostream>

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
// data
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
// Set a buffer
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void data::setBuffer(imbxUint32 bufferId, const ptr<buffer>& newBuffer)
{
	PUNTOEXE_FUNCTION_START(L"data::setBuffer");

	lockObject lockAccess(this);

	// Assign the new buffer
	///////////////////////////////////////////////////////////
	m_buffers[bufferId] = newBuffer;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Remove a buffer
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void data::deleteBuffer(imbxUint32 bufferId)
{
	PUNTOEXE_FUNCTION_START(L"data::deleteBuffer");

	// Lock the object
	///////////////////////////////////////////////////////////
	lockObject lockAccess(this);

	// Remove the buffer
	///////////////////////////////////////////////////////////
	tBuffersMap::iterator findBuffer = m_buffers.find(bufferId);
	if(findBuffer != m_buffers.end())
	{
		m_buffers.erase(findBuffer);
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return the buffer's data type
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
std::string data::getDataType()
{
	PUNTOEXE_FUNCTION_START(L"data::getDataType");

	// Lock the object
	///////////////////////////////////////////////////////////
	lockObject lockAccess(this);

	tBuffersMap::iterator findBuffer = m_buffers.find(0);
	if(findBuffer != m_buffers.end())
	{
		return findBuffer->second->getDataType();
	}
	
	return "";

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return the number of buffers in the tag
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 data::getBuffersCount()
{
	PUNTOEXE_FUNCTION_START(L"data::getBuffersCount");

	// Lock the object
	///////////////////////////////////////////////////////////
	lockObject lockAccess(this);

	// Returns the number of buffers
	///////////////////////////////////////////////////////////
	return m_buffers.size();

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return true if the specified buffer exists
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool data::bufferExists(imbxUint32 bufferId)
{
	PUNTOEXE_FUNCTION_START(L"data::bufferExists");

	// Lock the object
	///////////////////////////////////////////////////////////
	lockObject lockAccess(this);

	// Retrieve the buffer
	///////////////////////////////////////////////////////////
	tBuffersMap::iterator findBuffer = m_buffers.find(bufferId);
	return (findBuffer != m_buffers.end());

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return the size of a buffer
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 data::getBufferSize(imbxUint32 bufferId)
{
	PUNTOEXE_FUNCTION_START(L"data::getBufferSize");

	// Lock the object
	///////////////////////////////////////////////////////////
	lockObject lockAccess(this);

	// Retrieve the buffer
	///////////////////////////////////////////////////////////
	tBuffersMap::iterator findBuffer = m_buffers.find(bufferId);
	if(findBuffer == m_buffers.end())
	{
		return 0;
	}

	// Retrieve the buffer's size
	///////////////////////////////////////////////////////////
	return findBuffer->second->getBufferSizeBytes();

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get an handler (normal or raw) for the buffer
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
ptr<handlers::dataHandler> data::getDataHandler(imbxUint32 bufferId, bool bWrite, std::string defaultType)
{
	PUNTOEXE_FUNCTION_START(L"data::getDataHandler");

	// Lock the object
	///////////////////////////////////////////////////////////
	lockObject lockAccess(this);

	// Retrieve the buffer
	///////////////////////////////////////////////////////////
	ptr<buffer> pTempBuffer;
	tBuffersMap::iterator findBuffer = m_buffers.find(bufferId);
	if(findBuffer != m_buffers.end())
	{
		pTempBuffer = findBuffer->second;
	}

	// If a buffer already exists, then use the already defined
	//  datatype
	///////////////////////////////////////////////////////////
	if( !m_buffers.empty() && !m_buffers.begin()->second->getDataType().empty() )
	{
		defaultType = m_buffers.begin()->second->getDataType();
	}

	// If the buffer doesn't exist, then create a new one
	///////////////////////////////////////////////////////////
	if(pTempBuffer == 0 && bWrite)
	{
		pTempBuffer = new buffer(this, defaultType);
		pTempBuffer->setCharsetsList(&m_charsetsList);
		m_buffers[bufferId]=pTempBuffer;
	}

	// Retrieve the data handler
	///////////////////////////////////////////////////////////
	if(pTempBuffer == 0)
	{
		ptr<handlers::dataHandler> emptyDataHandler;
		return emptyDataHandler;
	}
	
	return pTempBuffer->getDataHandler(bWrite);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get a raw data handler
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
ptr<handlers::dataHandlerRaw> data::getDataHandlerRaw(imbxUint32 bufferId, bool bWrite, std::string defaultType)
{
	PUNTOEXE_FUNCTION_START(L"data::getDataHandlerRaw");

	// Lock the object
	///////////////////////////////////////////////////////////
	lockObject lockAccess(this);

	// Retrieve the buffer
	///////////////////////////////////////////////////////////
	ptr<buffer> pTempBuffer;
	tBuffersMap::iterator findBuffer = m_buffers.find(bufferId);
	if(findBuffer != m_buffers.end() )
	{
		pTempBuffer = findBuffer->second;
	}

	// If a buffer already exists, then use the already defined
	//  datatype
	///////////////////////////////////////////////////////////
	if( !m_buffers.empty() )
	{
		defaultType = (m_buffers.begin())->second->getDataType();
	}

	// If the buffer doesn't exist, then create a new one
	///////////////////////////////////////////////////////////
	if( pTempBuffer == 0 && bWrite )
	{
		pTempBuffer = new buffer(this, defaultType);
		pTempBuffer->setCharsetsList(&m_charsetsList);
		m_buffers[bufferId]=pTempBuffer;
	}

	// Retrieve the data handler
	///////////////////////////////////////////////////////////
	if( pTempBuffer == 0 )
	{
		ptr<handlers::dataHandlerRaw> emptyDataHandler;
		return emptyDataHandler;
	}

	return pTempBuffer->getDataHandlerRaw(bWrite);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get a stream reader that works on the buffer's data
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
ptr<streamReader> data::getStreamReader(imbxUint32 bufferId)
{
	PUNTOEXE_FUNCTION_START(L"data::getStreamReader");

	// Lock the object
	///////////////////////////////////////////////////////////
	lockObject lockAccess(this);

	// Retrieve the buffer
	///////////////////////////////////////////////////////////
	ptr<buffer> pTempBuffer;
	tBuffersMap::iterator findBuffer = m_buffers.find(bufferId);
	if(findBuffer != m_buffers.end())
	{
		return findBuffer->second->getStreamReader();
	}

	ptr<streamReader> emptyStream;
	return emptyStream;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get a stream writer that works on the buffer's data
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
ptr<streamWriter> data::getStreamWriter(imbxUint32 bufferId, std::string dataType /* = "" */)
{
	PUNTOEXE_FUNCTION_START(L"data::getStream");

	// Lock the object
	///////////////////////////////////////////////////////////
	lockObject lockAccess(this);

	// Retrieve the buffer
	///////////////////////////////////////////////////////////
	ptr<buffer> pTempBuffer;
	tBuffersMap::iterator findBuffer = m_buffers.find(bufferId);
	if(findBuffer != m_buffers.end())
	{
		pTempBuffer = findBuffer->second;
	}

	// If a buffer already exists, then use the already defined
	//  datatype
	///////////////////////////////////////////////////////////
	if( !m_buffers.empty() )
	{
		dataType = (m_buffers.begin())->second->getDataType();
	}

	// If the buffer doesn't exist, then create a new one
	///////////////////////////////////////////////////////////
	if(pTempBuffer == 0)
	{
		pTempBuffer = new buffer(this, dataType);
		m_buffers[bufferId]=pTempBuffer;
	}

	// Retrieve the data handler
	///////////////////////////////////////////////////////////
	if(pTempBuffer == 0)
	{
		ptr<streamWriter> emptyStream;
		return emptyStream;
	}
	
	return pTempBuffer->getStreamWriter();

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve an embedded data set.
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
ptr<dataSet> data::getDataSet(imbxUint32 dataSetId)
{
	PUNTOEXE_FUNCTION_START(L"data::getDataSet");

	// Lock the object
	///////////////////////////////////////////////////////////
	lockObject lockAccess(this);

	// Retrieve the buffer
	///////////////////////////////////////////////////////////
	if(m_embeddedDataSets.size() <= dataSetId)
	{
		return 0;
	}

	return m_embeddedDataSets[dataSetId];

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set a data set
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void data::setDataSet(imbxUint32 dataSetId, ptr<dataSet> pDataSet)
{
	PUNTOEXE_FUNCTION_START(L"data::setDataSet");

	// Lock the object
	///////////////////////////////////////////////////////////
	lockObject lockAccess(this);

	if(dataSetId >= m_embeddedDataSets.size())
	{
		m_embeddedDataSets.resize(dataSetId + 1);
	}
	m_embeddedDataSets[dataSetId] = pDataSet;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Append a data set
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void data::appendDataSet(ptr<dataSet> pDataSet)
{
	PUNTOEXE_FUNCTION_START(L"data::appendDataSet");

	// Lock the object
	///////////////////////////////////////////////////////////
	lockObject lockAccess(this);

	m_embeddedDataSets.push_back(pDataSet);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Define the charset to use in the buffers and embedded
//  datasets
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void data::setCharsetsList(charsetsList::tCharsetsList* pCharsetsList)
{
	PUNTOEXE_FUNCTION_START(L"data::setCharsetsList");

	lockObject lockAccess(this);

	m_charsetsList.clear();
	charsetsList::updateCharsets(pCharsetsList, &m_charsetsList);

	for(tEmbeddedDatasetsMap::iterator scanEmbeddedDataSets = m_embeddedDataSets.begin(); scanEmbeddedDataSets != m_embeddedDataSets.end(); ++scanEmbeddedDataSets)
	{
		(*scanEmbeddedDataSets)->setCharsetsList(pCharsetsList);
	}

	for(tBuffersMap::iterator scanBuffers = m_buffers.begin(); scanBuffers != m_buffers.end(); ++scanBuffers)
	{
		scanBuffers->second->setCharsetsList(pCharsetsList);
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get the charset used by the buffers and the embedded
//  datasets
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void data::getCharsetsList(charsetsList::tCharsetsList* pCharsetsList)
{
	PUNTOEXE_FUNCTION_START(L"data::getCharsetsList");

	lockObject lockAccess(this);
	
	m_charsetsList.clear();

	for(tEmbeddedDatasetsMap::iterator scanEmbeddedDataSets = m_embeddedDataSets.begin(); scanEmbeddedDataSets != m_embeddedDataSets.end(); ++scanEmbeddedDataSets)
	{
		charsetsList::tCharsetsList charsets;
		(*scanEmbeddedDataSets)->getCharsetsList(&charsets);
		charsetsList::updateCharsets(&charsets, &m_charsetsList);
	}

	for(tBuffersMap::iterator scanBuffers = m_buffers.begin(); scanBuffers != m_buffers.end(); ++scanBuffers)
	{
		charsetsList::tCharsetsList charsets;
		scanBuffers->second->getCharsetsList(&charsets);
		charsetsList::updateCharsets(&charsets, &m_charsetsList);
	}

	charsetsList::copyCharsets(&m_charsetsList, pCharsetsList);

	PUNTOEXE_FUNCTION_END();
}


} // namespace imebra

} // namespace puntoexe
