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

/*! \file codecFactory.cpp
    \brief Implementation of the codecFactory class.

*/

#include "../include/codecFactory.h"

#include "../../base/include/exception.h"
#include "../../base/include/streamReader.h"
#include "../include/codec.h"


namespace puntoexe
{

namespace imebra
{

namespace codecs
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Force the creation of the codec factory before main()
//  starts.
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static codecFactory::forceCodecFactoryCreation forceCreation;


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Register a codec
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void codecFactory::registerCodec(ptr<codec> pCodec)
{
	PUNTOEXE_FUNCTION_START(L"codecFactory::registerCodec");

	if(pCodec == 0)
	{
		return;
	}

	lockObject lockAccess(this);

	m_codecsList.push_back(pCodec);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve a codec that can handle the specified
//  transfer syntax
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
ptr<codec> codecFactory::getCodec(std::wstring transferSyntax)
{
	PUNTOEXE_FUNCTION_START(L"codecFactory::getCodec");

	ptr<codecFactory> pFactory(getCodecFactory());
	lockObject lockAccess(pFactory.get());

	for(std::list<ptr<codec> >::iterator scanCodecs=pFactory->m_codecsList.begin(); scanCodecs!=pFactory->m_codecsList.end(); ++scanCodecs)
	{
		if((*scanCodecs)->canHandleTransferSyntax(transferSyntax))
		{
			return (*scanCodecs)->createCodec();
		}
	}

	ptr<codec> emptyCodec;
	return emptyCodec;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve the only instance of the codec factory
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
ptr<codecFactory> codecFactory::getCodecFactory()
{
	static ptr<codecFactory> m_codecFactory(new codecFactory);
	return m_codecFactory;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Load the data from the specified stream and build a
//  dicomSet structure
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
ptr<dataSet> codecFactory::load(ptr<streamReader> pStream, imbxUint32 maxSizeBufferLoad /* = 0xffffffff */)
{
	PUNTOEXE_FUNCTION_START(L"codecFactory::load");

	// Copy the list of codecs in a local list so we don't have
	//  to lock the object for a long time
	///////////////////////////////////////////////////////////
	std::list<ptr<codec> > localCodecsList;
	ptr<codecFactory> pFactory(getCodecFactory());
	{
		lockObject lockAccess(pFactory.get());
		for(std::list<ptr<codec> >::iterator scanCodecs=pFactory->m_codecsList.begin(); scanCodecs!=pFactory->m_codecsList.end(); ++scanCodecs)
		{
			ptr<codec> copyCodec((*scanCodecs)->createCodec());
			localCodecsList.push_back(copyCodec);
		}
	}

	ptr<dataSet> pDataSet;
	for(std::list<ptr<codec> >::iterator scanCodecs=localCodecsList.begin(); scanCodecs != localCodecsList.end() && pDataSet == 0; ++scanCodecs)
	{
		try
		{
			return (*scanCodecs)->read(pStream, maxSizeBufferLoad);
		}
		catch(codecExceptionWrongFormat& /* e */)
		{
			exceptionsManager::getMessage(); // Reset the messages stack
			continue;
		}
	}

	if(pDataSet == 0)
	{
		PUNTOEXE_THROW(codecExceptionWrongFormat, "none of the codecs recognized the file format");
	}

	return pDataSet;

	PUNTOEXE_FUNCTION_END();
}

} // namespace codecs

} // namespace imebra

} // namespace puntoexe

