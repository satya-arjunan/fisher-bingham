#ifndef KENT_H
#define KENT_H

#include "Header.h"
#include "Support.h"

class Kent  // FB5
{
  private:

    Vector mu,major_axis,minor_axis;

    long double kappa,beta; // gamma = 0

  public:
    Kent();

    Kent(long double, long double);

    Kent(Vector &, Vector &, Vector &, long double, long double);
 
    Kent operator=(const Kent &);

    std::vector<Vector> generate(int);

    std::vector<Vector> generateCanonical(int);

    long double eccentricity();

    long double computeLogNormalizationConstant();

    long double computeLogNormalizationConstant(long double, long double);

    long double computeNegativeLogLikelihood(std::vector<Vector> &);

    long double computeNegativeLogLikelihood(Vector &, Matrix &);

    struct Estimates computeMomentEstimates(std::vector<Vector> &);

    struct Estimates computeMomentEstimates(Vector &, Matrix &);

    struct Estimates computeMLEstimates(std::vector<Vector> &);

    struct Estimates computeMLEstimates(Vector &, Matrix &);
};

#endif

