#include <utility>
#include <iostream>
#include "doctest/doctest.h"

/*
This unit testing uses the DocTest framework. This is the framework
used in the unit tests for the overall DICOMautomaton repository.

To run tests, run ./compile_and_run.sh from the terminal and a summary
of the tests passed and failed will be printed on the terminal.

For information on how to use this testing framework see:
https://github.com/onqtam/doctest
*/

TEST_CASE("example test"){
	REQUIRE(1==1);

	SUBCASE("example subcase"){
		REQUIRE(2==2);
	}
}
		
