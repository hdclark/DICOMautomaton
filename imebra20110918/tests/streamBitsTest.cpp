#include "streamBitsTest.h"

#include "../library/imebra/include/imebra.h"

#include <vector>
#include <cstdlib>

namespace puntoexe
{

namespace imebra
{

namespace tests
{


void streamBitsTest::test()
{
	std::srand(100);
	ptr<memory> myMemory(new memory);
	ptr<baseStream> theMemoryStream(new memoryStream(myMemory));
	ptr<streamWriter> writer(new streamWriter(theMemoryStream));
	
	writer->m_bJpegTags = true;

	std::vector<imbxUint8> bitsNumber(4000);
	std::vector<imbxUint32> bitsValue(4000);
	for(size_t fillValues = 0; fillValues < bitsValue.size(); ++fillValues)
	{
		imbxUint8 bits =  rand() * 16 / RAND_MAX;
		if(bits == 0) bits = 4;
		imbxUint32 maxValue = (1 << bits) - 1;
		imbxUint32 value = std::rand() * maxValue / RAND_MAX;
		bitsNumber[fillValues] = bits;
		bitsValue[fillValues] = value;
		writer->writeBits(value, bits);
	}
	writer->resetOutBitsBuffer();
	writer->flushDataBuffer();

	{
		ptr<streamReader> reader(new streamReader(theMemoryStream));
		reader->m_bJpegTags = true;

		for(size_t readValues = 0; readValues < bitsValue.size(); ++readValues)
		{
			imbxUint32 value(reader->readBits(bitsNumber[readValues]));
			QCOMPARE(value, bitsValue[readValues]);
		}
	}

	{
		ptr<streamReader> reader(new streamReader(theMemoryStream));
		reader->m_bJpegTags = true;

		for(size_t readValues = 0; readValues < bitsValue.size(); ++readValues)
		{
			imbxUint32 value(0);
			for(imbxUint8 count(bitsNumber[readValues]); count != 0; --count)
			{
				value <<= 1;
				value |= reader->readBit();
			}
			QCOMPARE(value, bitsValue[readValues]);
		}
	}

	{
		ptr<streamReader> reader(new streamReader(theMemoryStream));
		reader->m_bJpegTags = true;

		for(size_t readValues = 0; readValues < bitsValue.size(); ++readValues)
		{
			imbxUint32 value(0);
			for(imbxUint8 count(bitsNumber[readValues]); count != 0; --count)
			{
				reader->addBit(&value);
			}
			QCOMPARE(value, bitsValue[readValues]);
		}
	}

}




} // namespace tests

} // namespace imebra

} // namespace puntoexe
