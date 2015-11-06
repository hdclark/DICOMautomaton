// dicomdirItems.cpp
//

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include "../../library/imebra/include/imebra.h"

using namespace puntoexe;
using namespace puntoexe::imebra;

///////////////////////////////////////////////////////////
//
// Convert a string to XML entities that can be safely
//  embedded in a XML file
//
///////////////////////////////////////////////////////////
std::wstring xmlEntities(std::wstring value)
{
	std::wostringstream outputStream;
	for(std::wstring::iterator scanValue(value.begin()); scanValue != value.end(); ++scanValue)
	{
		switch(*scanValue)
		{
		case L'"':
			outputStream << L"&quot;";
			break;
		case L'&':
			outputStream << L"&amp;";
			break;
		case L'\'':
			outputStream << L"&apos;";
			break;
		case L'<':
			outputStream << L"&lt;";
			break;
		case '>':
			outputStream << L"&gt;";
			break;
		default:
			if(*scanValue < 33)
			{
				outputStream << L"&#" << (int)(*scanValue) << L";";
				break;
			}
			outputStream << *scanValue;
		}
	}

	return outputStream.str();
}


///////////////////////////////////////////////////////////
//
// Output a tag from the dataset in an XML tag
//
///////////////////////////////////////////////////////////
void outputTag(ptr<dataSet> pDataSet, imbxUint16 group, imbxUint16 tag, std::wostream* pOutputStream, std::wstring tagName, imbxUint16 id = 0)
{
	ptr<handlers::dataHandler> tagHandler(pDataSet->getDataHandler(group, 0, tag, 0, false));
	if(tagHandler == 0)
	{
		*pOutputStream << L"<" << tagName << L" />\n";
		return;
	}

	*pOutputStream << L"<" << tagName;
	if(id != 0)
	{
		*pOutputStream << L" tagid=\"" << id << L"\" ";
	}
	*pOutputStream << L">";

        for(size_t scanValues(0); tagHandler->pointerIsValid(scanValues); ++scanValues)
	{
                if(scanValues != 0)
		{
			*pOutputStream << L"\\";
		}
                *pOutputStream << xmlEntities(tagHandler->getUnicodeString(scanValues));
	}

	*pOutputStream << L"</" << tagName << L">\n";
}


///////////////////////////////////////////////////////////
//
// Scan all the sibling records (and their children) of
//  the specified one
//
///////////////////////////////////////////////////////////
void scanChildren(ptr<directoryRecord> pRecord, std::wostream* pOutputStream)
{
	for(;pRecord != 0; pRecord = pRecord->getNextRecord())
	{
		ptr<dataSet> pRecordDataSet(pRecord->getRecordDataSet());

		// Output the record
		(*pOutputStream) <<
			L"<record id=\"" <<
			pRecordDataSet->getItemOffset() <<
			L"\" type=\"" <<
			pRecord->getTypeString() << L"\">\n";

		// Output the file parts
		outputTag(pRecordDataSet, 0x4, 0x1500, pOutputStream, L"file");

		// Output the class UID
		outputTag(pRecordDataSet, 0x4, 0x1510, pOutputStream, L"class");

		// Output the instance UID
		outputTag(pRecordDataSet, 0x4, 0x1511, pOutputStream, L"instance");

		// Output the transfer syntax
		outputTag(pRecordDataSet, 0x4, 0x1512, pOutputStream, L"transfer");

		// Output the groups (everything but group 2 and 4)
		ptr<dataCollectionIterator<dataGroup> > scanGroups(pRecordDataSet->getDataIterator());
		for(scanGroups->reset(); scanGroups->isValid(); scanGroups->incIterator())
		{
			imbxUint16 groupId(scanGroups->getId());
			if(groupId == 2 || groupId == 4)
			{
				continue;
			}
			*pOutputStream << L"<group groupid=\"" << groupId << L"\">";

			ptr<dataCollectionIterator<data> > scanTags(scanGroups->getData()->getDataIterator());
			for(scanTags->reset(); scanTags->isValid(); scanTags->incIterator())
			{
				imbxUint16 id(scanTags->getId());
				if(id == 0)
				{
					continue;
				}
				outputTag(pRecordDataSet, groupId, id, pOutputStream, L"tag", id);
			}

			*pOutputStream << L"</group>";

		}


		// Output the child records
		(*pOutputStream) << L"<children>\n";
		scanChildren(pRecord->getFirstChildRecord(), pOutputStream);
		(*pOutputStream) << L"</children>\n";


		(*pOutputStream) << L"</record>\n";
	}
}


///////////////////////////////////////////////////////////
//
// Entry point
//
///////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{


	// Output the help if the parameters have not been
	//  specified
	if(argc < 2)
	{
        std::wstring version(L"1.0.0.1");
        std::wcout << L"dicomdirItems version " << version << L"\n";
		std::wcout << L"Usage: dicomdirItems dicomdirFileName outputFileName\n";
		std::wcout << L" dicomdirFileName = name of the DICOMDIR file\n";
		return 1;
	}

	// Open the file containing the dicom directory
	ptr<puntoexe::stream> inputStream(new puntoexe::stream);
	inputStream->openFile(argv[1], std::ios_base::in);

	// Connect a stream reader to the dicom stream
	ptr<puntoexe::streamReader> reader(new streamReader(inputStream));

	// Get a codec factory and let it use the right codec to create a dataset
	//  from the input stream
	ptr<codecs::codecFactory> codecsFactory(codecs::codecFactory::getCodecFactory());
	ptr<dataSet> loadedDataSet(codecsFactory->load(reader, 2048));

	// Now create a dicomdir object
	ptr<dicomDir> directory(new dicomDir(loadedDataSet));

	try
	{
		std::wcout << L"<dicomdir>";
		scanChildren(directory->getFirstRootRecord(), &(std::wcout));
		std::wcout << L"</dicomdir>";
		return 0;
	}
	catch(...)
	{
		std::wcout << exceptionsManager::getMessage();
		return 1;
	}
}

