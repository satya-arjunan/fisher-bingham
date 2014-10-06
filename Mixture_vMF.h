#ifndef MIXTURE_VMF_H
#define MIXTURE_VMF_H

#include "vMF.h"

class Mixture_vMF
{
  private:
    //! ID
    int id;

    //! Sample size
    int N;

    //! Number of components
    int K;

    //! List of components
    std::vector<vMF> components;
    
    //! Sample (x_i) -- Cartesian coordinates
    //! (on a sphere of unit radius)
    std::vector<Vector> data;

    //! Data weights
    Vector data_weights;

    //! Responsibility matrix (K X N)
    std::vector<Vector> responsibility;

    //! Effective sample size for each component (n_k)
    Vector sample_size;

    //! Weights of the components (a_k)
    Vector weights;

    //! List of message lengths over several iterations
    Vector msglens;

    //! Null model message length
    long double null_msglen;

    //! Optimal encoding length
    long double part1,part2,minimum_msglen;

  public:
    //! Null constructor
    Mixture_vMF();

    //! Constructor
    Mixture_vMF(int, std::vector<vMF> &, Vector &);

    //! Constructor
    Mixture_vMF(int, std::vector<Vector> &, Vector &);

    //! Constructor
    Mixture_vMF(int, std::vector<vMF> &, Vector &, Vector &, 
            std::vector<Vector> &, std::vector<Vector> &, Vector &);

    //! Overloading = operator
    Mixture_vMF operator=(const Mixture_vMF &);

    //! Overloading == operator
    bool operator==(const Mixture_vMF &);

    //! Prepare log file
    string getLogFile();

    //! Gets the list of weights
    Vector getWeights();

    //! Gets the list of components
    std::vector<vMF> getComponents();

    //! Returns number of components
    int getNumberOfComponents();

    //! Gets the responsibility matrix
    std::vector<Vector> getResponsibilityMatrix();

    //! Gets the sample size
    Vector getSampleSize();

    //! Initialize parameters
    void initialize();

    //! Updates the effective sample size
    void updateEffectiveSampleSize();

    //! Update the component weights
    void updateWeights();

    //! Update components
    void updateComponents();

    //! Update the responsibility matrix
    void updateResponsibilityMatrix();

    //! Computes the responsibility matrix
    void computeResponsibilityMatrix(std::vector<Vector> &, string &);
                                          
    //! Probability of a datum
    long double log_probability(Vector &);

    //! Computes the negative log likelihood
    long double negativeLogLikelihood(std::vector<Vector> &);

    //! Computes the minimum message length
    long double computeMinimumMessageLength();

    //! Gets the minimum message length
    long double getMinimumMessageLength();

    //! Gets the first part
    long double first_part();

    //! Gets the second part
    long double second_part();

    //! Estimate mixture parameters
    long double estimateParameters();

    //! EM loop
    void EM();

    //! Computes the null model message length
    long double computeNullModelMessageLength();

    //! Prints the model parameters
    void printParameters(ostream &, int, long double);

    //! Prints the model parameters
    void printParameters(ostream &, int);

    //! Prints the model parameters
    void printParameters(ostream &);

    //! Loads the mixture file
    void load(string &);

    //! Loads the mixture file with the corresponding data
    void load(string &, std::vector<Vector> &, Vector &);

    //! Randomly choose a component
    int randomComponent();

    //! Saves the data generated from a component
    void saveComponentData(int, std::vector<Vector> &);

    //! Generate random data from the distribution using mixture proportions
    std::vector<Vector> generate(int, bool);

    //! Splits a component
    Mixture_vMF split(int, ostream &);

    //! Deltes a component
    Mixture_vMF kill(int, ostream &);

    //! Joins two  components
    Mixture_vMF join(int, int, ostream &);

    //! Generate heat maps (for d=3)
    void generateHeatmapData(long double);

    //! Get the nearest component
    int getNearestComponent(int);

    //! Computes the approx KL divergence between two mixtures
    long double computeKLDivergence(Mixture_vMF &);
};

#endif
