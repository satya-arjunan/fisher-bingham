#ifndef MIXTURE_H
#define MIXTURE_H

#include "Kent.h"

class Mixture
{
  private:
    friend class Experiments;

    //! ID
    int id;

    //! Sample size
    int N;

    //! Number of components
    int K;

    //! List of components
    std::vector<Kent> components;
    
    //! Sample (x_i) -- Cartesian coordinates
    //! (on a sphere of unit radius)
    std::vector<Vector > data;

    //! Data weights
    Vector data_weights;

    //! Responsibility matrix (K X N)
    std::vector<Vector > responsibility;

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
    Mixture();

    //! Constructor
    Mixture(int, std::vector<Kent> &, Vector &);

    //! Constructor
    Mixture(int, std::vector<Vector > &, Vector &);

    //! Constructor
    Mixture(int, std::vector<Kent> &, Vector &, Vector &, 
            std::vector<Vector > &, std::vector<Vector > &, Vector &);

    //! Overloading = operator
    Mixture operator=(const Mixture &);

    //! Overloading == operator
    bool operator==(const Mixture &);

    //! Prepare log file
    string getLogFile();

    //! Gets the list of weights
    Vector getWeights();

    //! Gets the list of components
    std::vector<Kent> getComponents();

    //! Returns number of components
    int getNumberOfComponents();

    //! Gets the responsibility matrix
    std::vector<Vector > getResponsibilityMatrix();

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
    void computeResponsibilityMatrix(std::vector<Vector > &, string &);
                                          
    //! Probability of a datum
    long double probability(Vector &);
    long double log_probability(Vector &);

    //! Computes the negative log likelihood
    long double negativeLogLikelihood(std::vector<Vector > &);

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

    //! Plot the variation in message length
    void plotMessageLengthEM();

    //! Loads the mixture file
    void load(string &, int);

    //! Loads the mixture file with the corresponding data
    void load(string &, int, std::vector<Vector > &, Vector &);

    //! Randomly choose a component
    int randomComponent();

    //! Saves the data generated from a component
    void saveComponentData(int, std::vector<Vector > &);

    //! Generate random data from the distribution using mixture proportions
    std::vector<Vector > generate(int, bool);

    //! Splits a component
    Mixture split(int, ostream &);

    //! Splits a component
    void split(int, Mixture &, Mixture &, Mixture &);

    //! Deltes a component
    Mixture kill(int, ostream &);

    //! Deltes a component
    void kill(int, Mixture &, Mixture &);

    //! Joins two  components
    Mixture join(int, int, ostream &);

    //! Joins two  components
    void join(int, int, Mixture &, Mixture &, Mixture &);

    //! Generate heat maps (for d=3)
    void generateHeatmapData(long double);

    //! Identifies identical components in two Mixtures
    void mapComponents(Mixture &, std::vector<int> &);

    //! Classifies the data
    void classify(std::vector<Vector > &);

    //! Get the nearest component
    int getNearestComponent(int);

    //! Computes the Akaike information criterion 
    long double computeAIC();
    long double computeAIC_2();

    //! Computes the Bayesian information criterion 
    long double computeBIC();
    long double computeBIC_2();

    //! Computes the approx KL divergence between two mixtures
    long double computeKLDivergence(Mixture &);
};

#endif
