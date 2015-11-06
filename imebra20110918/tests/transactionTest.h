#if !defined(imebraTransactionTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
#define imebraTransactionTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_

#include <QtTest/QtTest>

namespace puntoexe
{

namespace imebra
{

namespace tests
{

class transactionTest: public QObject
{
	Q_OBJECT

private slots:
	void testOneTransaction();
	void testNestedTransactions0();
	void testNestedTransactions1();
	void testNestedTransactionsFail0();
	void testNestedTransactionsFail1();

};

} // namespace tests

} // namespace imebra

} // namespace puntoexe


#endif // #if !defined(imebraTransactionTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
