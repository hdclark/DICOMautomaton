#include "exceptionsTest.h"

#if defined(WIN32) | defined(WIN32_WCE)
#include "windows.h"
#else
#include <pthread.h>
#endif

#include "../library/imebra/include/imebra.h"

namespace puntoexe
{

namespace imebra
{

namespace tests
{

// A buffer initialized to a default data type should use the data type OB
void exceptionsTest::testExceptions()
{
	try
	{
		PUNTOEXE_FUNCTION_START(L"exceptionTest_Phase1");
		{
			PUNTOEXE_FUNCTION_START(L"exceptionTest_Phase2");
			{
				PUNTOEXE_FUNCTION_START(L"exceptionTest_Phase3");
				exceptionsManager::getMessage();
				PUNTOEXE_THROW(std::runtime_error, "testPhase3");
				PUNTOEXE_FUNCTION_END();
			}
			PUNTOEXE_FUNCTION_END();
		}
		PUNTOEXE_FUNCTION_END();
	}
	catch(...)
	{
		std::wstring exceptionMessage = puntoexe::exceptionsManager::getMessage();
		QVERIFY(!exceptionMessage.empty());

		std::wstring exceptionMessageEmpty = puntoexe::exceptionsManager::getMessage();
		QVERIFY(exceptionMessageEmpty.empty());

		QVERIFY(exceptionMessage.find(L"exceptionTest_Phase1") != exceptionMessage.npos);
		QVERIFY(exceptionMessage.find(L"exceptionTest_Phase2") != exceptionMessage.npos);
		QVERIFY(exceptionMessage.find(L"exceptionTest_Phase3") != exceptionMessage.npos);
		QVERIFY(exceptionMessage.find(L"testPhase3") != exceptionMessage.npos);
		return;
	}
	QVERIFY(false);
}



} // namespace tests

} // namespace imebra

} // namespace puntoexe
