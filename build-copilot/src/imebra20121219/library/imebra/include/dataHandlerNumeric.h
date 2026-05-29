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

/*! \file dataHandlerNumeric.h
    \brief Declaration of the handler for the numeric tags.

*/

#if !defined(imebraDataHandlerNumeric_BD270581_5746_48d1_816E_64B700955A12__INCLUDED_)
#define imebraDataHandlerNumeric_BD270581_5746_48d1_816E_64B700955A12__INCLUDED_

#include <sstream>
#include <iomanip>

#include "../../base/include/exception.h"
#include "dataHandler.h"

#define HANDLER_CALL_TEMPLATE_FUNCTION(functionName, handlerPointer)\
{\
	puntoexe::imebra::handlers::dataHandlerNumericBase* pHandler(handlerPointer.get()); \
	if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<imbxUint8>)) \
{\
	functionName<imbxUint8> ((imbxUint8*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize());\
	}\
	else if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<imbxInt8>))\
{\
	functionName<imbxInt8> ((imbxInt8*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize());\
	}\
	else if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<imbxUint16>))\
{\
	functionName<imbxUint16> ((imbxUint16*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize());\
	}\
	else if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<imbxInt16>))\
{\
	functionName<imbxInt16> ((imbxInt16*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize());\
	}\
	else if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<imbxUint32>))\
{\
	functionName<imbxUint32> ((imbxUint32*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize());\
	}\
	else if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<imbxInt32>))\
{\
	functionName<imbxInt32> ((imbxInt32*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize());\
	}\
	else if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<float>))\
{\
	functionName<float> ((float*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize());\
	}\
	else if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<double>))\
{\
	functionName<double> ((double*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize());\
	}\
	else\
{\
	throw std::runtime_error("Data type not valid");\
	}\
	}

#define HANDLER_CALL_TEMPLATE_FUNCTION_WITH_PARAMS(functionName, handlerPointer, ...)\
{\
	puntoexe::imebra::handlers::dataHandlerNumericBase* pHandler(handlerPointer.get()); \
	if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<imbxUint8>)) \
{\
	functionName<imbxUint8> ((imbxUint8*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize(), __VA_ARGS__);\
	}\
	else if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<imbxInt8>))\
{\
	functionName<imbxInt8> ((imbxInt8*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize(), __VA_ARGS__);\
	}\
	else if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<imbxUint16>))\
{\
	functionName<imbxUint16> ((imbxUint16*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize(), __VA_ARGS__);\
	}\
	else if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<imbxInt16>))\
{\
	functionName<imbxInt16> ((imbxInt16*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize(), __VA_ARGS__);\
	}\
	else if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<imbxUint32>))\
{\
	functionName<imbxUint32> ((imbxUint32*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize(), __VA_ARGS__);\
	}\
	else if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<imbxInt32>))\
{\
	functionName<imbxInt32> ((imbxInt32*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize(), __VA_ARGS__);\
	}\
	else if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<float>))\
{\
	functionName<float> ((float*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize(), __VA_ARGS__);\
	}\
	else if(typeid(*pHandler) == typeid(puntoexe::imebra::handlers::dataHandlerNumeric<double>))\
{\
	functionName<double> ((double*)handlerPointer->getMemoryBuffer(), handlerPointer->getSize(), __VA_ARGS__);\
	}\
	else\
{\
	throw std::runtime_error("Data type not valid");\
	}\
	}

///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe::imebra
//
///////////////////////////////////////////////////////////
namespace puntoexe
{

namespace imebra
{

namespace handlers
{

/// \brief This is the base class for the %data handlers
///         that manage numeric values.
///
///////////////////////////////////////////////////////////
class dataHandlerNumericBase: public dataHandler
{
    friend class buffer;

public:
	imbxUint8* getMemoryBuffer() const
	{
		return m_pMemoryString;
	}

	size_t getMemorySize() const
	{
		return m_memorySize;
	}

	/// \brief Returns the memory object that stores the data
	///         managed by the handler.
	///
	/// @return the memory object that stores the data managed
	///          by the handler
	///
	///////////////////////////////////////////////////////////
	ptr<memory> getMemory()
	{
		return m_memory;
	}

	// Set the buffer's size, in data elements
	///////////////////////////////////////////////////////////
	void setSize(const imbxUint32 elementsNumber) override
	{
		PUNTOEXE_FUNCTION_START(L"dataHandlerNumeric::setSize");

		m_memory->resize(elementsNumber * getUnitSize());
		m_pMemoryString = m_memory->data();
		m_memorySize = m_memory->size();

		PUNTOEXE_FUNCTION_END();
	}

	// Parse the tag's buffer and extract its content
	///////////////////////////////////////////////////////////
	void parseBuffer(const ptr<memory>& memoryBuffer) override
	{
		PUNTOEXE_FUNCTION_START(L"dataHandlerNumeric::parseBuffer");

		m_memory = memoryBuffer;
		m_pMemoryString = m_memory->data();
		m_memorySize = m_memory->size();

		PUNTOEXE_FUNCTION_END();
	}

	// Rebuild the tag's buffer
	///////////////////////////////////////////////////////////
	void buildBuffer(const ptr<memory>& memoryBuffer) override
	{
		PUNTOEXE_FUNCTION_START(L"dataHandlerNumeric::buildBuffer");

		memoryBuffer->transfer(m_memory);

		PUNTOEXE_FUNCTION_END();
	}

	virtual void copyFrom(ptr<dataHandlerNumericBase> pSource) = 0;

	virtual void copyFrom(imbxUint8* pMemory, size_t memorySize) = 0;
	virtual void copyFrom(imbxInt8* pMemory, size_t memorySize) = 0;
	virtual void copyFrom(imbxUint16* pMemory, size_t memorySize) = 0;
	virtual void copyFrom(imbxInt16* pMemory, size_t memorySize) = 0;
	virtual void copyFrom(imbxUint32* pMemory, size_t memorySize) = 0;
	virtual void copyFrom(imbxInt32* pMemory, size_t memorySize) = 0;
	virtual void copyFrom(float* pMemory, size_t memorySize) = 0;
	virtual void copyFrom(double* pMemory, size_t memorySize) = 0;

	virtual void copyTo(imbxUint8* pMemory, size_t memorySize) = 0;
	virtual void copyTo(imbxInt8* pMemory, size_t memorySize) = 0;
	virtual void copyTo(imbxUint16* pMemory, size_t memorySize) = 0;
	virtual void copyTo(imbxInt16* pMemory, size_t memorySize) = 0;
	virtual void copyTo(imbxUint32* pMemory, size_t memorySize) = 0;
	virtual void copyTo(imbxInt32* pMemory, size_t memorySize) = 0;
	virtual void copyTo(float* pMemory, size_t memorySize) = 0;
	virtual void copyTo(double* pMemory, size_t memorySize) = 0;

	virtual void copyFromInt32Interleaved(const imbxInt32* pSource,
										  imbxUint32 sourceReplicateX,
										  imbxUint32 sourceReplicateY,
										  imbxUint32 destStartCol,
										  imbxUint32 destStartRow,
										  imbxUint32 destEndCol,
										  imbxUint32 destEndRow,
										  imbxUint32 destStartChannel,
										  imbxUint32 destWidth,
										  imbxUint32 destHeight,
										  imbxUint32 destNumChannels) = 0;

	virtual void copyToInt32Interleaved(imbxInt32* pDest,
										imbxUint32 destSubSampleX,
										imbxUint32 destSubSampleY,
										imbxUint32 sourceStartCol,
										imbxUint32 sourceStartRow,
										imbxUint32 sourceEndCol,
										imbxUint32 sourceEndRow,
										imbxUint32 sourceStartChannel,
										imbxUint32 sourceWidth,
										imbxUint32 sourceHeight,
										imbxUint32 sourceNumChannels) const = 0;

	/// \brief Returns truen if the buffer's elements are
	///         signed, false otherwise.
	///
	/// @return true if the buffer's elements are signed,
	///          or false otherwise
	///
	///////////////////////////////////////////////////////////
	virtual bool isSigned() const = 0;

	bool pointerIsValid(const imbxUint32 index) const override
	{
		return index < getSize();
	}

protected:
	// Memory buffer
	///////////////////////////////////////////////////////////
	imbxUint8* m_pMemoryString;
	size_t m_memorySize;
	ptr<memory> m_memory;
};

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This data handler accesses to the numeric data
///         stored in a puntoexe::imebra::buffer class.
///
/// A special definition of this class
///  (puntoexe::imebra::handlers::imageHandler) is used
///  to access to the images' raw pixels.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
template<class dataHandlerType>
class dataHandlerNumeric : public dataHandlerNumericBase
{
public:
	/// \brief Provides a direct access to the data managed by
	///         the handler.
	///
	/// @param nSubscript  the index of the numeric value that
	///                     you want to access to
	/// @return a reference to the requested value
	///
	///////////////////////////////////////////////////////////
	dataHandlerType& operator[](int nSubscript)
	{
		return ((dataHandlerType*)m_pMemoryString)[nSubscript];
	}

	/// \brief Provides a direct access to the data managed by
	///         the handler.
	///
	/// @param nSubscript  the index of the numeric value that
	///                     you want to access to
	/// @return a reference to the requested value
	///
	///////////////////////////////////////////////////////////
	dataHandlerType& at(int nSubscript)
	{
		return ((dataHandlerType*)m_pMemoryString)[nSubscript];
	}

	// Returns the size of an element managed by the
	//  handler.
	///////////////////////////////////////////////////////////
	imbxUint32 getUnitSize() const override
	{
		return sizeof(dataHandlerType);
	}

	bool isSigned() const override
	{
		dataHandlerType firstValue((dataHandlerType) -1);
		dataHandlerType secondValue((dataHandlerType) 0);
		return firstValue < secondValue;
	}

	// Retrieve the data element as a signed long
	///////////////////////////////////////////////////////////
	imbxInt32 getSignedLong(const imbxUint32 index) const override
	{
		return (imbxInt32) (((dataHandlerType*)m_pMemoryString)[index]);
	}

	// Retrieve the data element an unsigned long
	///////////////////////////////////////////////////////////
	imbxUint32 getUnsignedLong(const imbxUint32 index) const override
	{
		return (imbxUint32) (((dataHandlerType*)m_pMemoryString)[index]);
	}

	// Retrieve the data element as a double
	///////////////////////////////////////////////////////////
	double getDouble(const imbxUint32 index) const override
	{
		return (double) (((dataHandlerType*)m_pMemoryString)[index]);
	}

	// Retrieve the data element as a string
	///////////////////////////////////////////////////////////
	std::string getString(const imbxUint32 index) const override
	{
		PUNTOEXE_FUNCTION_START(L"dataHandlerNumeric::getString");

		std::ostringstream convStream;
		convStream << std::fixed << getDouble(index);
		return convStream.str();

		PUNTOEXE_FUNCTION_END();
	}

	// Retrieve the data element as a unicode string
	///////////////////////////////////////////////////////////
	std::wstring getUnicodeString(const imbxUint32 index) const override
	{
		PUNTOEXE_FUNCTION_START(L"dataHandlerNumeric::getUnicodeString");

		std::wostringstream convStream;
		convStream << std::fixed << getDouble(index);
		return convStream.str();

		PUNTOEXE_FUNCTION_END();
	}

	// Retrieve the buffer's size in elements
	///////////////////////////////////////////////////////////
	imbxUint32 getSize() const override
	{
		PUNTOEXE_FUNCTION_START(L"dataHandlerNumeric::getSize");

		return m_memorySize/getUnitSize();

		PUNTOEXE_FUNCTION_END();
	}

	// Set the data element as a signed long
	///////////////////////////////////////////////////////////
	void setSignedLong(const imbxUint32 index, const imbxInt32 value) override
	{
		((dataHandlerType*)m_pMemoryString)[index] = (dataHandlerType)value;
	}

	// Set the data element as an unsigned long
	///////////////////////////////////////////////////////////
	void setUnsignedLong(const imbxUint32 index, const imbxUint32 value) override
	{
		((dataHandlerType*)m_pMemoryString)[index] = (dataHandlerType)value;
	}

	// Set the data element as a double
	///////////////////////////////////////////////////////////
	void setDouble(const imbxUint32 index, const double value) override
	{
		((dataHandlerType*)m_pMemoryString)[index] = (dataHandlerType)value;
	}

	// Set the data element as a string
	///////////////////////////////////////////////////////////
	void setString(const imbxUint32 index, const std::string& value) override
	{
		PUNTOEXE_FUNCTION_START(L"dataHandlerNumeric::setString");

		std::istringstream convStream(value);
		double tempValue;
		convStream >> tempValue;
		setDouble(index, tempValue);

		PUNTOEXE_FUNCTION_END();
	}

	// Set the data element as an unicode string
	///////////////////////////////////////////////////////////
	void setUnicodeString(const imbxUint32 index, const std::wstring& value) override
	{
		PUNTOEXE_FUNCTION_START(L"dataHandlerNumeric::setUnicodeString");

		std::wistringstream convStream(value);
		double tempValue;
		convStream >> tempValue;
		setDouble(index, tempValue);

		PUNTOEXE_FUNCTION_END();
	}

	/// \brief Copy the values from a memory location to
	///         the buffer managed by the handler
	///
	/// @param pSource a pointer to the first byte of the
	///         memory area to copy
	/// @param sourceSize the number of values to copy
	///
	///////////////////////////////////////////////////////////
	template<class sourceHandlerType>
	void copyFromMemory(sourceHandlerType* pSource, size_t sourceSize)
	{
		setSize(sourceSize);
		if(getSize() < sourceSize)
		{
			sourceSize = getSize();
		}
		dataHandlerType* pDest((dataHandlerType*)m_pMemoryString);
		while(sourceSize-- != 0)
		{
			*(pDest++) = (dataHandlerType)*(pSource++);
		}
	}

	// Copy the data from another handler
	///////////////////////////////////////////////////////////
	void copyFrom(ptr<dataHandlerNumericBase> pSource) override
	{
		PUNTOEXE_FUNCTION_START(L"dataHandlerNumeric::copyFrom");

		HANDLER_CALL_TEMPLATE_FUNCTION(copyFromMemory, pSource);

		PUNTOEXE_FUNCTION_END();

	}

	void copyFrom(imbxUint8* pMemory, size_t memorySize) override
	{
		copyFromMemory(pMemory, memorySize);
	}
	void copyFrom(imbxInt8* pMemory, size_t memorySize) override
	{
		copyFromMemory(pMemory, memorySize);
	}
	void copyFrom(imbxUint16* pMemory, size_t memorySize) override
	{
		copyFromMemory(pMemory, memorySize);
	}
	void copyFrom(imbxInt16* pMemory, size_t memorySize) override
	{
		copyFromMemory(pMemory, memorySize);
	}
	void copyFrom(imbxUint32* pMemory, size_t memorySize) override
	{
		copyFromMemory(pMemory, memorySize);
	}
	void copyFrom(imbxInt32* pMemory, size_t memorySize) override
	{
		copyFromMemory(pMemory, memorySize);
	}
	void copyFrom(float* pMemory, size_t memorySize) override
	{
		copyFromMemory(pMemory, memorySize);
	}
	void copyFrom(double* pMemory, size_t memorySize) override
	{
		copyFromMemory(pMemory, memorySize);
	}


	template<class destHandlerType>
	void copyToMemory(destHandlerType* pDestination, size_t destSize)
	{
		if(getSize() < destSize)
		{
			destSize = getSize();
		}
		dataHandlerType* pSource((dataHandlerType*)m_pMemoryString);
		while(destSize-- != 0)
		{
			*(pDestination++) = (destHandlerType)*(pSource++);
		}
	}

	void copyTo(imbxUint8* pMemory, size_t memorySize) override
	{
		copyToMemory(pMemory, memorySize);
	}
	void copyTo(imbxInt8* pMemory, size_t memorySize) override
	{
		copyToMemory(pMemory, memorySize);
	}
	void copyTo(imbxUint16* pMemory, size_t memorySize) override
	{
		copyToMemory(pMemory, memorySize);
	}
	void copyTo(imbxInt16* pMemory, size_t memorySize) override
	{
		copyToMemory(pMemory, memorySize);
	}
	void copyTo(imbxUint32* pMemory, size_t memorySize) override
	{
		copyToMemory(pMemory, memorySize);
	}
	void copyTo(imbxInt32* pMemory, size_t memorySize) override
	{
		copyToMemory(pMemory, memorySize);
	}
	void copyTo(float* pMemory, size_t memorySize) override
	{
		copyToMemory(pMemory, memorySize);
	}
	void copyTo(double* pMemory, size_t memorySize) override
	{
		copyToMemory(pMemory, memorySize);
	}


	template<int subsampleX>
	inline void copyFromInt32Interleaved(
		const imbxInt32* pSource,
		imbxUint32 sourceReplicateY,
		imbxUint32 destStartCol,
		imbxUint32 destStartRow,
		imbxUint32 destEndCol,
		imbxUint32 destEndRow,
		imbxUint32 destStartChannel,
		imbxUint32 destWidth,
		imbxUint32 destHeight,
		imbxUint32 destNumChannels)
	{
		dataHandlerType *pDestRowScan = &(((dataHandlerType*)m_pMemoryString)[(destStartRow*destWidth+destStartCol)*destNumChannels+destStartChannel]);
		const imbxInt32* pSourceRowScan = pSource;

		imbxUint32 replicateYCount = sourceReplicateY;
		imbxUint32 replicateYIncrease = (destEndCol - destStartCol) / subsampleX;

		dataHandlerType *pDestColScan;
		const imbxInt32* pSourceColScan;
		const imbxInt32* pSourceEndColScan;

		if(destHeight < destEndRow)
		{
			destEndRow = destHeight;
		}
		if(destWidth < destEndCol)
		{
			destEndCol = destWidth;
		}

		imbxUint32 numColumns(destEndCol - destStartCol);

		imbxUint32 horizontalCopyOperations = numColumns / subsampleX;
		imbxUint32 horizontalFinalCopyDest = numColumns - horizontalCopyOperations * subsampleX;

		dataHandlerType copyValue;
		imbxInt32 scanDest;

		imbxUint32 scanHorizontalFinalCopyDest;

		imbxUint32 destRowScanIncrease(destWidth * destNumChannels);


		for(imbxUint32 numYCopies(destEndRow - destStartRow); numYCopies != 0; --numYCopies)
		{
			pDestColScan = pDestRowScan;
			pSourceColScan = pSourceRowScan;
			pSourceEndColScan = pSourceColScan + horizontalCopyOperations;

			if(subsampleX == 1)
			{
				while(pSourceColScan != pSourceEndColScan)
				{
					*pDestColScan = (dataHandlerType)(*(pSourceColScan++));
					pDestColScan += destNumChannels;
				}
			}
			else
			{
				while(pSourceColScan != pSourceEndColScan)
				{
					copyValue = (dataHandlerType)(*(pSourceColScan++));
					for(scanDest = subsampleX; scanDest != 0; --scanDest)
					{
						*pDestColScan = copyValue;
						pDestColScan += destNumChannels;
					}
				}

				for(scanHorizontalFinalCopyDest = horizontalFinalCopyDest; scanHorizontalFinalCopyDest != 0; --scanHorizontalFinalCopyDest)
				{
					*pDestColScan = (dataHandlerType) *pSourceColScan;
					pDestColScan += destNumChannels;
				}

			}

			pDestRowScan += destRowScanIncrease;
			if(--replicateYCount == 0)
			{
				replicateYCount = sourceReplicateY;
				pSourceRowScan += replicateYIncrease;
			}
		}
	}


	/// \brief Copy the content of an array of imbxInt32 values
	///         into the buffer controlled by the handler,
	///         considering that the source buffer could
	///         have subsampled data.
	///
	/// The source buffer is supposed to have the information
	///  related to a single channel.
	/// @param pSource      a pointer to the source array of
	///                      imbxInt32 values
	/// @param sourceReplicateX the horizontal subsamplig
	///                      factor of the source buffer
	///                      (1=not subsampled, 2=subsampled)
	/// @param sourceReplicateY the vertical subsamplig
	///                      factor of the source buffer
	///                      (1=not subsampled, 2=subsampled)
	/// @param destStartCol the horizontal coordinate of the
	///                      top left corner of the destination
	///                      rectangle
	/// @param destStartRow the vertical coordinate of the
	///                      top left corner of the destination
	///                      rectangle
	/// @param destEndCol   the horizontal coordinate of the
	///                      bottom right corner of the
	///                      destination rectangle
	/// @param destEndRow   the vertical coordinate of the
	///                      bottom right corner of the
	///                      destination rectangle
	/// @param destStartChannel the destination channel
	/// @param destWidth    the destination buffer's width in
	///                      pixels
	/// @param destHeight   the destination buffer's height in
	///                      pixels
	/// @param destNumChannels the number of channels in the
	///                      destination buffer
	///
	///////////////////////////////////////////////////////////
	void copyFromInt32Interleaved(const imbxInt32* pSource,
										  imbxUint32 sourceReplicateX,
										  imbxUint32 sourceReplicateY,
										  imbxUint32 destStartCol,
										  imbxUint32 destStartRow,
										  imbxUint32 destEndCol,
										  imbxUint32 destEndRow,
										  imbxUint32 destStartChannel,
										  imbxUint32 destWidth,
										  imbxUint32 destHeight,
										  imbxUint32 destNumChannels) override
	{
		if(destStartCol >= destWidth || destStartRow >= destHeight)
		{
			return;
		}

		switch(sourceReplicateX)
		{
		case 1:
			copyFromInt32Interleaved< 1 > (
						pSource,
						sourceReplicateY,
						destStartCol,
						destStartRow,
						destEndCol,
						destEndRow,
						destStartChannel,
						destWidth,
						destHeight,
						destNumChannels);
			break;
		case 2:
			copyFromInt32Interleaved< 2 > (
						pSource,
						sourceReplicateY,
						destStartCol,
						destStartRow,
						destEndCol,
						destEndRow,
						destStartChannel,
						destWidth,
						destHeight,
						destNumChannels);
			break;
		default:
			copyFromInt32Interleaved< 4 > (
						pSource,
						sourceReplicateY,
						destStartCol,
						destStartRow,
						destEndCol,
						destEndRow,
						destStartChannel,
						destWidth,
						destHeight,
						destNumChannels);
			break;
		}
	}


	/// \brief Copy the buffer controlled by the handler into
	///         an array of imbxInt32 values, considering that
	///         the destination buffer could be subsampled
	///
	/// The destination buffer is supposed to have the
	///  information related to a single channel.
	/// @param pDest        a pointer to the destination array
	///                      of imbxInt32 values
	/// @param destSubSampleX the horizontal subsamplig
	///                      factor of the destination buffer
	///                      (1=not subsampled, 2=subsampled)
	/// @param destSubSampleY the vertical subsamplig
	///                      factor of the destination buffer
	///                      (1=not subsampled, 2=subsampled)
	/// @param sourceStartCol the horizontal coordinate of the
	///                      top left corner of the source
	///                      rectangle
	/// @param sourceStartRow the vertical coordinate of the
	///                      top left corner of the source
	///                      rectangle
	/// @param sourceEndCol   the horizontal coordinate of the
	///                      bottom right corner of the
	///                      source rectangle
	/// @param sourceEndRow   the vertical coordinate of the
	///                      bottom right corner of the
	///                      source rectangle
	/// @param sourceStartChannel the source channel to be
	///                      copied
	/// @param sourceWidth  the source buffer's width in
	///                      pixels
	/// @param sourceHeight the source buffer's height in
	///                      pixels
	/// @param sourceNumChannels the number of channels in the
	///                      source buffer
	///
	///////////////////////////////////////////////////////////
	void copyToInt32Interleaved(imbxInt32* pDest,
										imbxUint32 destSubSampleX,
										imbxUint32 destSubSampleY,
										imbxUint32 sourceStartCol,
										imbxUint32 sourceStartRow,
										imbxUint32 sourceEndCol,
										imbxUint32 sourceEndRow,
										imbxUint32 sourceStartChannel,
										imbxUint32 sourceWidth,
										imbxUint32 sourceHeight,
										imbxUint32 sourceNumChannels) const override
	{
		if(sourceStartCol >= sourceWidth || sourceStartRow >= sourceHeight)
		{
			return;
		}
		dataHandlerType *pSourceRowScan = &(((dataHandlerType*)m_pMemoryString)[(sourceStartRow*sourceWidth+sourceStartCol)*sourceNumChannels+sourceStartChannel]);
		imbxInt32* pDestRowScan = pDest;

		imbxUint32 subSampleXCount;
		imbxUint32 subSampleYCount = destSubSampleY;
		imbxUint32 subSampleYIncrease = (sourceEndCol - sourceStartCol) / destSubSampleX;

		dataHandlerType *pSourceColScan;
		imbxInt32* pDestColScan;

		imbxInt32 lastValue = (imbxInt32)*pSourceRowScan;

		for(imbxUint32 scanRow = sourceStartRow; scanRow < sourceEndRow; ++scanRow)
		{
			pSourceColScan = pSourceRowScan;
			pDestColScan = pDestRowScan;
			subSampleXCount = destSubSampleX;

			for(imbxUint32 scanCol = sourceStartCol; scanCol < sourceEndCol; ++scanCol)
			{
				if(scanCol < sourceWidth)
				{
					lastValue = (imbxInt32)*pSourceColScan;
					pSourceColScan += sourceNumChannels;
				}
				*pDestColScan += (imbxInt32)lastValue;
				if(--subSampleXCount == 0)
				{
					subSampleXCount = destSubSampleX;
					++pDestColScan;
				}
			}
			if(scanRow < sourceHeight - 1)
			{
				pSourceRowScan += sourceWidth * sourceNumChannels;
			}
			if(--subSampleYCount == 0)
			{
				subSampleYCount = destSubSampleY;
				pDestRowScan += subSampleYIncrease;
			}
		}

		imbxInt32 rightShift = 0;
		if(destSubSampleX == 2)
		{
			++rightShift;
		}
		if(destSubSampleY == 2)
		{
			++rightShift;
		}
		if(rightShift == 0)
		{
			return;
		}
		for(imbxInt32* scanDivide = pDest; scanDivide < pDestRowScan; ++scanDivide)
		{
			*scanDivide >>= rightShift;
		}

	}


};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief A dataHandlerRaw always "sees" the data as a
///         collection of bytes, no matter what the Dicom
///         data type is.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataHandlerRaw: public dataHandlerNumeric<imbxUint8>
{
};

} // namespace handlers

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraDataHandlerNumeric_BD270581_5746_48d1_816E_64B700955A12__INCLUDED_)
