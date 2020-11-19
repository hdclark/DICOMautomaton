#include <utility>
#include <iostream>
#include "doctest/doctest.h"
#include "CPD_Rigid.h"
#include "CPD_Affine.h"
#ifndef CPDSHARED_H_
#define CPDSHARED_H_
#include "CPD_Shared.h"
#endif


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
		REQUIRE(w2==2);
	}
}


TEST_CASE("B") {
	MatrixXd xHat(3, 2);
	xHat << 3, 4,
			1, 1,
			1, 2;
	MatrixXd yHat(3, 2);
	yHat << 1, 2,
			2, 4,
			1, 1;
	MatrixXd yHat(3, 2);
	yHat << 1, 2,
			2, 4,
			1, 1;
	MatrixXd postProb(3, 3);
	postProb << 1, 2, 1,
				0, 1, 1,
				1, 1, 1;
	MatrixXd answerB(2, 2);
	postProb << (20./9), (-5./9),
				(28./9), (-7./9);
	MatrixXd B;
	B = CalculateB(xHat, yHat, postProb);

	double threshold = 0.01;
	REQUIRE(((answer - B).norm() < threshold));
}

TEST_CASE("sigma squared") {
	MatrixXd xHat(3, 2);
	xHat << 3, 4,
			1, 1,
			1, 2;
	MatrixXd yHat(3, 2);
	yHat << 1, 2,
			2, 4,
			1, 1;
	MatrixXd yHat(3, 2);
	yHat << 1, 2,
			2, 4,
			1, 1;
	MatrixXd postProb(3, 3);
	postProb << 1, 0, 1,
				2, 1, 1,
				1, 1, 1;
	MatrixXd B(2, 2);
	postProb << (20./9), (-5./9),
				(28./9), (-7./9);
	double Np = 9.;
	double sigmaSquared = SigmaSquared(Np, B, xHat, yHat, postProb);

	double sigmaSquaredAnswer = 257./243;
	double threshold = 0.01;
	REQUIRE(sigmaSquared == doctest::Approx(sigmaSquaredAnswer).epsilon(threshold));
}
