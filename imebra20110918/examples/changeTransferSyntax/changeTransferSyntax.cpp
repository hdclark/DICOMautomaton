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

#include <iostream>


#include "../../library/imebra/include/imebra.h"
#include <sstream>

#ifdef PUNTOEXE_WINDOWS
#include <process.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#endif

#include <memory>
#include <list>

using namespace puntoexe;
using namespace puntoexe::imebra;

int findArgument(const char* argument, int argc, char* argv[])
{
	for(int scanArg(0); scanArg != argc; ++scanArg)
	{
		if(std::string(argv[scanArg]) == argument)
		{
			return scanArg;
		}
	}
	return -1;
}

void CopyGroups(ptr<dataSet> source, ptr<dataSet> destination)
{
	for(ptr<dataCollectionIterator<dataGroup> > scanGroups(source->getDataIterator()); scanGroups->isValid(); scanGroups->incIterator())
	{
		ptr<dataGroup> sourceGroup(scanGroups->getData());
		ptr<dataGroup> newGroup(destination->getGroup(scanGroups->getId(), scanGroups->getOrder(), true));

		for(ptr<dataCollectionIterator<imebra::data> > scanTags(sourceGroup->getDataIterator()); scanTags->isValid(); scanTags->incIterator())
		{
			if(!newGroup->getDataType(scanTags->getId()).empty())
			{
				continue;
			}

			ptr<data> sourceTag(scanTags->getData());
			ptr<data> destTag(newGroup->getTag(scanTags->getId(), true));

			for(imbxUint32 itemId(0); ; ++itemId)
			{
				ptr<dataSet> sourceSequenceItem(sourceTag->getDataSet(itemId));
				if(sourceSequenceItem != 0)
				{
					ptr<dataSet> destSequenceItem(new dataSet);
					destTag->setDataSet(itemId, destSequenceItem);
					CopyGroups(sourceSequenceItem, destSequenceItem);
					continue;
				}
				ptr<handlers::dataHandlerRaw> sourceHandler(sourceTag->getDataHandlerRaw(itemId, false, ""));
				if(sourceHandler == 0)
				{
					break;
				}
				ptr<handlers::dataHandlerRaw> destHandler(destTag->getDataHandlerRaw(itemId, true, sourceHandler->getDataType()));
				imbxUint8* pSourceBuffer(sourceHandler->getMemoryBuffer());
				size_t sourceSize(sourceHandler->getMemorySize());
				destHandler->setSize(sourceSize);
				imbxUint8* pDestBuffer(destHandler->getMemoryBuffer());
				::memcpy(pDestBuffer, pSourceBuffer, sourceSize);
			}
		}
	}

}

int main(int argc, char* argv[])
{
	std::wstring version(L"1.0.0.1");
	std::wcout << L"changeTransferSyntax version " << version << std::endl;

	try
	{
		std::string inputFileName;
		std::string outputFileName;
		std::wstring transferSyntax;
		if(argc == 4)
		{
			wchar_t* transferSyntaxAllowedValues[]=
			{
				L"1.2.840.10008.1.2.1", // Explicit VR little endian
				L"1.2.840.10008.1.2.2", // Explicit VR big endian
				L"1.2.840.10008.1.2.5", // RLE compression
				L"1.2.840.10008.1.2.4.50", // Jpeg baseline (8 bits lossy)
				L"1.2.840.10008.1.2.4.51", // Jpeg extended (12 bits lossy)
				L"1.2.840.10008.1.2.4.57" // Jpeg lossless NH
			};

			inputFileName = argv[1];
			outputFileName = argv[2];

			std::istringstream convertTransferSyntax(argv[3]);
			int transferSyntaxValue(-1);
			convertTransferSyntax >> transferSyntaxValue;
			if(transferSyntaxValue >= 0 && transferSyntaxValue < sizeof(transferSyntaxAllowedValues)/sizeof(wchar_t*))
			{
				transferSyntax = transferSyntaxAllowedValues[transferSyntaxValue];
			}
		}

		if(inputFileName.empty() || outputFileName.empty() || transferSyntax.empty())
		{
					std::wcout << L"Usage: changeTransferSyntax inputFileName outputFileName newTransferSyntax" << std::endl;
					std::wcout << L"newTransferSyntax values: 0 = Explicit VR little endian" << std::endl;
					std::wcout << L"                          1 = Explicit VR big endian" << std::endl;
					std::wcout << L"                          2 = RLE compression" << std::endl;
					std::wcout << L"                          3 = Jpeg baseline (8 bits lossy)" << std::endl;
					std::wcout << L"                          4 = Jpeg extended (12 bits lossy)" << std::endl;
					std::wcout << L"                          5 = Jpeg lossless NH" << std::endl;
					return 1;
		}

		ptr<dataSet> loadedDataSet;

		// Open the file containing the dicom dataset
		ptr<puntoexe::stream> inputStream(new puntoexe::stream);
		inputStream->openFile(inputFileName, std::ios_base::in);

		// Connect a stream reader to the dicom stream. Several stream reader
		//  can share the same stream
		ptr<streamReader> reader(new streamReader(inputStream));

		// Get a codec factory and let it use the right codec to create a dataset
		//  from the input stream
		ptr<codecs::codecFactory> codecsFactory(codecs::codecFactory::getCodecFactory());
		loadedDataSet = codecsFactory->load(reader, 2048);

		// Now we create a new dataset and copy the tags and images from the loaded dataset
		ptr<puntoexe::imebra::dataSet> newDataSet(new puntoexe::imebra::dataSet);

		// Copy the images first
		try
		{
			imbxUint32 scanImages(0);
			for(;;)
			{
				ptr<image> copyImage(loadedDataSet->getImage(scanImages));
				newDataSet->setImage(scanImages, copyImage, transferSyntax, imebra::codecs::codec::high);
				++scanImages;
			}
		}
		catch(const dataSetImageDoesntExist&)
		{
			// Ignore this
		}

		CopyGroups(loadedDataSet, newDataSet);

		// Write dataset
		ptr<puntoexe::stream> outputStream(new puntoexe::stream);
		outputStream->openFile(outputFileName, std::ios::out);
		ptr<streamWriter> writer(new streamWriter(outputStream));
		ptr<codecs::dicomCodec> writeCodec(new codecs::dicomCodec);
		writeCodec->write(writer, newDataSet);
	}
	catch(...)
	{
		std::wcout << exceptionsManager::getMessage();
		return 1;
	}
}

