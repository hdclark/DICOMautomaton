#if !defined(imebraNumericHandlerTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
#define imebraNumericHandlerTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_

#include <QtTest/QtTest>

namespace puntoexe
{

namespace imebra
{

namespace tests
{

class numericHandlerTest: public QObject
{
	Q_OBJECT

private slots:
	void interleavedCopy();
	void stringConversion();
	void validPointer();

};

} // namespace tests

} // namespace imebra

} // namespace puntoexe


#endif // #if !defined(imebraNumericHandlerTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
