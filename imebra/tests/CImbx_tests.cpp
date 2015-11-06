// imebra_tests.cpp : Defines the entry point for the console application.
//


#include "ageStringHandlerTest.h"
#include "bitTransformTest.h"
#include "bufferTest.h"
#include "dataSetTest.h"
#include "dateTimeHandlerTest.h"
#include "decimalStringHandlerTest.h"
#include "dicomCodecTest.h"
#include "dicomDirTest.h"
#include "drawBitmapTest.h"
#include "exceptionsTest.h"
#include "huffmanTest.h"
#include "jpegCodecTest.h"
#include "memoryStreamTest.h"
#include "numericHandlerTest.h"
#include "paletteTest.h"
#include "streamBitsTest.h"
#include "transactionTest.h"
#include "unicodeStringHandlerTest.h"

int main(int  , char**  )
{
    puntoexe::imebra::tests::ageStringHandlerTest m_ageStringHandlerTest;
    puntoexe::imebra::tests::bitTransformTest m_bitTransformTest;
    puntoexe::imebra::tests::bufferTest m_bufferTest;
	puntoexe::imebra::tests::dataSetTest m_dataSetTest;
	puntoexe::imebra::tests::dateTimeHandlerTest m_dateTimeHandlerTest;
	puntoexe::imebra::tests::decimalStringHandlerTest m_decimalStringHandlerTest;
	puntoexe::imebra::tests::dicomCodecTest m_dicomCodecTest;
	puntoexe::imebra::tests::dicomDirTest m_dicomDirTest;
	puntoexe::imebra::tests::drawBitmapTest m_drawBitmapTest;
	puntoexe::imebra::tests::exceptionsTest m_exceptionsTest;
	puntoexe::imebra::tests::huffmanTest m_huffmanTest;
	puntoexe::imebra::tests::jpegCodecTest m_jpegCodecTest;
	puntoexe::imebra::tests::memoryStreamTest m_memoryStreamTest;
	puntoexe::imebra::tests::numericHandlerTest m_numericHandlerTest;
	puntoexe::imebra::tests::paletteTest m_paletteTest;
	puntoexe::imebra::tests::streamBitsTest m_streamBitsTest;
	puntoexe::imebra::tests::transactionTest m_transactionTest;
	puntoexe::imebra::tests::unicodeStringHandlerTest m_unicodeHandlerTest;

    return
			QTest::qExec(&m_ageStringHandlerTest) +
			QTest::qExec(&m_bitTransformTest) +
			QTest::qExec(&m_bufferTest) +
			QTest::qExec(&m_dataSetTest) +
			QTest::qExec(&m_dateTimeHandlerTest) +
			QTest::qExec(&m_decimalStringHandlerTest) +
			QTest::qExec(&m_dicomCodecTest) +
			QTest::qExec(&m_dicomDirTest) +
			QTest::qExec(&m_drawBitmapTest) +
			QTest::qExec(&m_exceptionsTest) +
			QTest::qExec(&m_huffmanTest) +
			QTest::qExec(&m_jpegCodecTest) +
			QTest::qExec(&m_memoryStreamTest) +
			QTest::qExec(&m_numericHandlerTest) +
			QTest::qExec(&m_paletteTest) +
			QTest::qExec(&m_streamBitsTest) +
			QTest::qExec(&m_transactionTest) +
			QTest::qExec(&m_unicodeHandlerTest);

}


