#include "huffmanTest.h"

#include "../library/imebra/include/imebra.h"

#include <vector>
#include <stdlib.h>

namespace puntoexe
{

namespace imebra
{

namespace tests
{

// A buffer initialized to a default data type should use the data type OB
void huffmanTest::test()
{
	ptr<memory> myMemory(new memory);
	ptr<baseStream> theMemoryStream(new memoryStream(myMemory));
	ptr<streamWriter> writer(new streamWriter(theMemoryStream));

	huffmanTable huffman(8);

	std::vector<imbxUint8> values(4000);
	for(size_t fillValues = 0; fillValues < values.size(); ++fillValues)
	{
		imbxUint8 value =  (imbxUint8)(rand() * (double)255 / (double)RAND_MAX);
		values[fillValues] = value;
		huffman.incValueFreq(values[fillValues]);
	}
	huffman.calcHuffmanCodesLength(16);
	huffman.calcHuffmanTables();

	for(size_t writeValues = 0; writeValues < values.size(); ++writeValues)
	{
		imbxUint32 value = values[writeValues];
		huffman.writeHuffmanCode(value, writer.get());
	}
	writer->resetOutBitsBuffer();

	ptr<streamReader> reader(new streamReader(theMemoryStream));
	for(size_t readValues = 0; readValues < values.size(); ++readValues)
	{
		imbxUint32 value(huffman.readHuffmanCode(reader.get()));

		QCOMPARE((imbxUint8)value, values[readValues]);
	}
}



} // namespace tests

} // namespace imebra

} // namespace puntoexe
