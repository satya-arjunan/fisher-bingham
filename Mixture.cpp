#include "Mixture.h"
#include "Support.h"

extern int MIXTURE_ID;
extern int MIXTURE_SIMULATION;
extern int INFER_COMPONENTS;
extern int ENABLE_DATA_PARALLELISM;
extern int NUM_THREADS;
extern long double IMPROVEMENT_RATE;

/*!
 *  \brief Null constructor module
 */
Mixture::Mixture()
{
  id = MIXTURE_ID++;
}

/*!
 *  \brief This is a constructor function which instantiates a Mixture
 *  \param K an integer
 *  \param components a reference to a std::vector<Kent>
 *  \param weights a reference to a Vector 
 */
Mixture::Mixture(int K, std::vector<Kent> &components, Vector &weights):
                 K(K), components(components), weights(weights)
{
  assert(components.size() == K);
  assert(weights.size() == K);
  id = MIXTURE_ID++;
  minimum_msglen = 0;
}

/*!
 *  \brief This is a constructor function.
 *  \param K an integer
 *  \param data a reference to a std::vector<Vector >
 *  \param data_weights a reference to a Vector
 */
Mixture::Mixture(int K, std::vector<Vector > &data, Vector &data_weights) : 
                 K(K), data(data), data_weights(data_weights)
{
  id = MIXTURE_ID++;
  N = data.size();
  assert(data_weights.size() == N);
  minimum_msglen = 0;
}

/*!
 *  \brief This is a constructor function.
 *  \param K an integer
 *  \param components a reference to a std::vector<Kent>
 *  \param weights a reference to a Vector
 *  \param sample_size a reference to a Vector
 *  \param responsibility a reference to a std::vector<Vector >
 *  \param data a reference to a std::vector<Vector >
 *  \param data_weights a reference to a Vector
 */
Mixture::Mixture(
  int K, 
  std::vector<Kent> &components, 
  Vector &weights,
  Vector &sample_size, 
  std::vector<Vector > &responsibility,
  std::vector<Vector > &data,
  Vector &data_weights
) : K(K), components(components), weights(weights), sample_size(sample_size),
    responsibility(responsibility), data(data), data_weights(data_weights)
{
  id = MIXTURE_ID++;
  assert(components.size() == K);
  assert(weights.size() == K);
  assert(sample_size.size() == K);
  assert(responsibility.size() == K);
  N = data.size();
  assert(data_weights.size() == N);
  minimum_msglen = 0;
}

/*!
 *  \brief This function assigns a source Mixture distribution.
 *  \param source a reference to a Mixture
 */
Mixture Mixture::operator=(const Mixture &source)
{
  if (this != &source) {
    id = source.id;
    N = source.N;
    K = source.K;
    components = source.components;
    data = source.data;
    data_weights = source.data_weights;
    responsibility = source.responsibility;
    sample_size = source.sample_size;
    weights = source.weights;
    msglens = source.msglens;
    null_msglen = source.null_msglen;
    minimum_msglen = source.minimum_msglen;
    part1 = source.part1;
    part2 = source.part2;
  }
  return *this;
}

/*!
 *  \brief This function checks whether the two Mixture objects are the same.
 *  \param other a reference to a Mixture
 *  \return whether they are the same object or not
 */
bool Mixture::operator==(const Mixture &other)
{
  if (id == other.id) {
    return 1;
  } else {
    return 0;
  }
}

/*!
 *  \brief This function returns the list of all weights.
 *  \return the list of weights
 */
Vector Mixture::getWeights()
{
  return weights;
}

/*!
 *  \brief This function returns the list of components.
 *  \return the components
 */
std::vector<Kent> Mixture::getComponents()
{
  return components;
}

/*!
 *  \brief Gets the number of components
 */
int Mixture::getNumberOfComponents()
{
  return components.size();
}

/*!
 *  \brief This function returns the responsibility matrix.
 */
std::vector<Vector > Mixture::getResponsibilityMatrix()
{
  return responsibility;
}

/*!
 *  \brief This function returns the sample size of the mixture.
 *  \return the sample size
 */
Vector Mixture::getSampleSize()
{
  return sample_size;
}

/*!
 *  \brief This function initializes the parameters of the model.
 */
void Mixture::initialize()
{
  N = data.size();
  cout << "Sample size: " << N << endl;

  // initialize responsibility matrix
  //srand(time(NULL));
  Vector tmp(N,0);
  responsibility = std::vector<Vector >(K,tmp);
  /*for (int i=0; i<K; i++) {
    responsibility.push_back(tmp);
  }*/

  #pragma omp parallel for if(ENABLE_DATA_PARALLELISM) num_threads(NUM_THREADS) 
  for (int i=0; i<N; i++) {
    int index = rand() % K;
    responsibility[index][i] = 1;
  }
  sample_size = Vector(K,0);
  updateEffectiveSampleSize();
  weights = Vector(K,0);
  updateWeights();

  // initialize parameters of each component
  components = std::vector<Kent>(K);
  updateComponents();
}

/*!
 *  \brief This function updates the effective sample size of each component.
 */
void Mixture::updateEffectiveSampleSize()
{
  for (int i=0; i<K; i++) {
    long double count = 0;
    #pragma omp parallel for if(ENABLE_DATA_PARALLELISM) num_threads(NUM_THREADS) reduction(+:count)
    for (int j=0; j<N; j++) {
      count += responsibility[i][j];
    }
    sample_size[i] = count;
  }
}

/*!
 *  \brief This function is used to update the weights of the components.
 */
void Mixture::updateWeights()
{
  long double normalization_constant = N + (K/2.0);
  for (int i=0; i<K; i++) {
    weights[i] = (sample_size[i] + 0.5) / normalization_constant;
  }
}

/*!
 *  \brief This function is used to update the components.
 */
void Mixture::updateComponents()
{
  Vector comp_data_wts(N,0);
  for (int i=0; i<K; i++) {
    #pragma omp parallel for if(ENABLE_DATA_PARALLELISM) num_threads(NUM_THREADS) 
    for (int j=0; j<N; j++) {
      comp_data_wts[j] = responsibility[i][j] * data_weights[j];
    }
    components[i].estimateParameters(data,comp_data_wts);
    //components[i].updateParameters();
  }
}

/*!
 *  \brief This function updates the terms in the responsibility matrix.
 */
void Mixture::updateResponsibilityMatrix()
{
  #pragma omp parallel for if(ENABLE_DATA_PARALLELISM) num_threads(NUM_THREADS) //private(j)
  for (int i=0; i<N; i++) {
    Vector log_densities(K,0);
    for (int j=0; j<K; j++) {
      log_densities[j] = components[j].log_density(data[i]);
    }
    int max_index = maximumIndex(log_densities);
    long double max_log_density = log_densities[max_index];
    for (int j=0; j<K; j++) {
      log_densities[j] -= max_log_density; 
    }
    long double px = 0;
    Vector probabilities(K,0);
    for (int j=0; j<K; j++) {
      probabilities[j] = weights[j] * exp(log_densities[j]);
      px += probabilities[j];
    }
    for (int j=0; j<K; j++) {
      responsibility[j][i] = probabilities[j] / px;
      assert(!boost::math::isnan(responsibility[j][i]));
    }
  }
}

/*!
 *  \brief This function updates the terms in the responsibility matrix.
 */
void Mixture::computeResponsibilityMatrix(std::vector<Vector > &sample,
                                          string &output_file)
{
  int sample_size = sample.size();
  Vector tmp(sample_size,0);
  std::vector<Vector > resp(K,tmp);
  #pragma omp parallel for if(ENABLE_DATA_PARALLELISM) num_threads(NUM_THREADS) //private(j)
  for (int i=0; i<sample_size; i++) {
    Vector log_densities(K,0);
    for (int j=0; j<K; j++) {
      log_densities[j] = components[j].log_density(sample[i]);
    }
    int max_index = maximumIndex(log_densities);
    long double max_log_density = log_densities[max_index];
    for (int j=0; j<K; j++) {
      log_densities[j] -= max_log_density; 
    }
    long double px = 0;
    Vector probabilities(K,0);
    for (int j=0; j<K; j++) {
      probabilities[j] = weights[j] * exp(log_densities[j]);
      px += probabilities[j];
    }
    for (int j=0; j<K; j++) {
      resp[j][i] = probabilities[j] / px;
    }
  }
  ofstream out(output_file.c_str());
  for (int i=0; i<sample_size; i++) {
    for (int j=0; j<K; j++) {
      out << fixed << setw(10) << setprecision(5) << resp[j][i];
    }
    out << endl;
  }
  out.close();
}

/*!
 *  \brief This function computes the probability of a datum from the mixture
 *  model.
 *  \param x a reference to an array<long double,2>
 *  \return the probability value
 */
long double Mixture::probability(Vector &x)
{
  long double px = 0;
  Vector probability_density(K,0);
  for (int i=0; i<K; i++) {
    probability_density[i] = components[i].density(x);
    px += weights[i] * probability_density[i];
  }
  if (!(px > 0)) {
    cout << "px: " << px << endl;
    for (int i=0; i<K; i++) {
      cout << weights[i] << "\t" << probability_density[i] << endl;
    }
  }
  fflush(stdout);
  assert(px >= 0);
  return px;
}

/*!
 *
 */
long double Mixture::log_probability(Vector &x)
{
  Vector log_densities(K,0);
  for (int j=0; j<K; j++) {
      log_densities[j] = components[j].log_density(x);
  }
  int max_index = maximumIndex(log_densities);
  long double max_log_density = log_densities[max_index];
  for (int j=0; j<K; j++) {
    log_densities[j] -= max_log_density;
  }
  long double density = 0;
  for (int j=0; j<K; j++) {
    density += weights[j] * exp(log_densities[j]);
  }
  return max_log_density + log(density);
}

/*!
 *  \brief This function computes the negative log likelihood of a data
 *  sample.
 *  \param a reference to a std::vector<array<long double,2> >
 *  \return the negative log likelihood (base e)
 */
long double Mixture::negativeLogLikelihood(std::vector<Vector > &sample)
{
  long double value = 0,log_density;
  #pragma omp parallel for if(ENABLE_DATA_PARALLELISM) num_threads(NUM_THREADS) private(log_density) reduction(-:value)
  for (int i=0; i<sample.size(); i++) {
    log_density = log_probability(sample[i]);
    value -= log_density;
  }
  return value;
}

/*!
 *  \brief This function computes the minimum message length using the current
 *  model parameters.
 *  \return the minimum message length
 */
long double Mixture::computeMinimumMessageLength()
{
  // encode the number of components
  // assume uniform priors
  long double Ik = log(MAX_COMPONENTS);
  cout << "Ik: " << Ik << endl;
  assert(Ik > 0);

  // enocde the weights
  long double Iw = ((K-1)/2.0) * log(N);
  Iw -= boost::math::lgamma<long double>(K); // log(K-1)!
  for (int i=0; i<K; i++) {
    Iw -= 0.5 * log(weights[i]);
  }
  cout << "Iw: " << Iw << endl;
  assert(Iw >= 0);

  // encode the likelihood of the sample
  long double Il = negativeLogLikelihood(data);
  Il -= 2 * N * log(AOM);
  cout << "Il: " << Il << endl;
  assert(Il > 0);

  // encode the parameters of the components
  long double It = 0,logp;
  for (int i=0; i<K; i++) {
    logp = components[i].computeLogPriorProbability();
    It += logp;
  }
  cout << "It: " << It << endl;
  /*if (It <= 0) { cout << It << endl;}
  fflush(stdout);
  assert(It > 0);*/

  // the constant term
  // # of continuous parameters d = 4K-1 (for D = 3)
  int num_free_params = 4*K - 1;
  long double cd = computeConstantTerm(num_free_params);
  cout << "cd: " << cd << endl;

  minimum_msglen = (Ik + Iw + Il + It + cd)/(log(2));

  part2 = Il + num_free_params/2.0;
  part2 /= log(2);
  part1 = minimum_msglen - part2;

  return minimum_msglen;
}

/*!
 *  \brief Prepares the appropriate log file
 */
string Mixture::getLogFile()
{
  string file_name;
  if (INFER_COMPONENTS == UNSET) {
    if (MIXTURE_SIMULATION == UNSET) {
      file_name = "./mixture/logs/";
    } else if (MIXTURE_SIMULATION == SET) {
      file_name = "./simulation/logs/";
    }
  } else if (INFER_COMPONENTS == SET) {
    file_name = "./infer/logs/";
    file_name += "m_" + boost::lexical_cast<string>(id) + "_";
  }
  file_name += boost::lexical_cast<string>(K) + ".log";
  ofstream log(file_name.c_str());
  //cout << "log file: " << file_name << endl;
  //exit(1);
  return file_name;
}

/*!
 *  \brief This function is used to estimate the model parameters by running
 *  an EM algorithm.
 *  \return the stable message length
 */
long double Mixture::estimateParameters()
{
  initialize();

  EM();

  return minimum_msglen;
}

/*!
 *  \brief This function runs the EM method.
 */
void Mixture::EM()
{
  /* prepare log file */
  string log_file = getLogFile();
  ofstream log(log_file.c_str());

  computeNullModelMessageLength();
  //cout << "null_msglen: " << null_msglen << endl;

  long double prev=0,current;
  int iter = 1;
  printParameters(log,0,0);

  /* EM loop */
  if (ESTIMATION == MML_NEWTON || ESTIMATION == MML_HALLEY || ESTIMATION == MML_COMPLETE) {
    while (1) {
      // Expectation (E-step)
      updateResponsibilityMatrix();
      updateEffectiveSampleSize();
      // Maximization (M-step)
      updateWeights();
      updateComponents();
      current = computeMinimumMessageLength();
      if (fabs(current) >= INFINITY) break;
      msglens.push_back(current);
      printParameters(log,iter,current);
      if (iter != 1) {
        assert(current > 0);
        // because EM has to consistently produce lower 
        // message lengths otherwise something wrong!
        // IMPORTANT: the below condition should not be 
        //          fabs(prev - current) <= 0.0001 * fabs(prev)
        // ... it's very hard to satisfy this condition and EM() goes into
        // ... an infinite loop!
        if (iter > 10 && (prev - current) <= IMPROVEMENT_RATE * prev) {
          log << "\nSample size: " << N << endl;
          log << "vMF encoding rate: " << current/N << " bits/point" << endl;
          log << "Null model encoding: " << null_msglen << " bits.";
          log << "\t(" << null_msglen/N << " bits/point)" << endl;
          break;
        }
      }
      prev = current;
      iter++;
    }
  } else {  // ESTIMATION != MML
    while (1) {
      // Expectation (E-step)
      updateResponsibilityMatrix();
      updateEffectiveSampleSize();
      // Maximization (M-step)
      updateWeights_ML();
      updateComponents();
      current = negativeLogLikelihood_2(data);
      msglens.push_back(current);
      printParameters(log,iter,current);
      if (iter != 1) {
        //assert(current > 0);
        // because EM has to consistently produce lower 
        // -ve likelihood values otherwise something wrong!
        if (iter > 10 && fabs(prev - current) <= IMPROVEMENT_RATE * fabs(prev)) {
          current = computeMinimumMessageLength();
          log << "\nSample size: " << N << endl;
          log << "vMF encoding rate (using ML): " << current/N << " bits/point" << endl;
          log << "Null model encoding: " << null_msglen << " bits.";
          log << "\t(" << null_msglen/N << " bits/point)" << endl;
          break;
        }
      }
      prev = current;
      iter++;
    }
  }
  log.close();

  //plotMessageLengthEM();
}


/*!
 *  \brief This function computes the null model message length.
 *  \return the null model message length
 */
long double Mixture::computeNullModelMessageLength()
{
  // compute logarithm of surface area of nd-sphere
  long double log_area = log(4*PI);
  null_msglen = N * (log_area - ((D-1)*log(AOM)));
  null_msglen /= log(2);
  return null_msglen;
}

/*!
 *  \brief This function returns the minimum message length of this mixture
 *  model.
 */
long double Mixture::getMinimumMessageLength()
{
  return minimum_msglen;
}

/*!
 *  \brief Returns the first part of the msg.
 */
long double Mixture::first_part()
{
  return part1;
}

/*!
 *  \brief Returns the second part of the msg.
 */
long double Mixture::second_part()
{
  return part2;
}

/*!
 *  \brief This function prints the parameters to a log file.
 *  \param os a reference to a ostream
 *  \param iter an integer
 *  \param msglen a long double
 */
void Mixture::printParameters(ostream &os, int iter, long double msglen)
{
  os << "Iteration #: " << iter << endl;
  for (int k=0; k<K; k++) {
    os << "\t" << fixed << setw(5) << "[" << k+1 << "]";
    os << "\t" << fixed << setw(10) << setprecision(3) << sample_size[k];
    os << "\t" << fixed << setw(10) << setprecision(5) << weights[k];
    os << "\t";
    components[k].printParameters(os);
  }
  os << "\t\t\tmsglen: " << msglen << " bits." << endl;
}

/*!
 *  \brief This function prints the parameters to a log file.
 *  \param os a reference to a ostream
 */
void Mixture::printParameters(ostream &os, int num_tabs)
{
  string tabs = "\t";
  if (num_tabs == 2) {
    tabs += "\t";
  }
  for (int k=0; k<K; k++) {
    os << tabs << "[";// << fixed << setw(5) << "[" << k+1 << "]";
    os << fixed << setw(2) << k+1;
    os << "]";
    os << "\t" << fixed << setw(10) << setprecision(3) << sample_size[k];
    os << "\t" << fixed << setw(10) << setprecision(5) << weights[k];
    os << "\t";
    components[k].printParameters(os);
  }
  os << tabs << "ID: " << id << endl;
  os << tabs << "vMF encoding: " << minimum_msglen << " bits. "
     << "(" << minimum_msglen/N << " bits/point)" << endl << endl;
}

/*!
 *  \brief Outputs the mixture weights and component parameters to a file.
 */
void Mixture::printParameters(ostream &os)
{
  for (int k=0; k<K; k++) {
    os << "\t" << fixed << setw(10) << setprecision(5) << weights[k];
    os << "\t";
    components[k].printParameters(os);
  }
}

/*!
 *  \brief This function is used to plot the variation in message length
 *  with each iteration.
 */
void Mixture::plotMessageLengthEM()
{
  string data_file,plot_file,script_file,parsed;
  if (INFER_COMPONENTS == UNSET) {
    if (MIXTURE_SIMULATION == UNSET) {
      data_file = CURRENT_DIRECTORY + "/mixture/msglens/";
      plot_file = CURRENT_DIRECTORY + "/mixture/plots/";
      script_file = CURRENT_DIRECTORY + "/mixture/plots/";
    } else if (MIXTURE_SIMULATION == SET) {
      data_file = CURRENT_DIRECTORY + "/simulation/msglens/";
      plot_file = CURRENT_DIRECTORY + "/simulation/plots/";
      script_file = CURRENT_DIRECTORY + "/simulation/plots/";
    }
  } else if (INFER_COMPONENTS == SET) {
    data_file = CURRENT_DIRECTORY + "/infer/msglens/";
    plot_file = CURRENT_DIRECTORY + "/infer/plots/";
    script_file = CURRENT_DIRECTORY + "/infer/plots/";
    data_file += "m_" + boost::lexical_cast<string>(id) + "_";
    plot_file += "m_" + boost::lexical_cast<string>(id) + "_";
    script_file += "m_" + boost::lexical_cast<string>(id) + "_";
  }

  string num_comp = boost::lexical_cast<string>(K);
  data_file += num_comp + ".dat";
  plot_file += num_comp + ".eps";
  script_file += num_comp + "_script.p";
  ofstream file(data_file.c_str());
  for (int i=0; i<msglens.size(); i++) {
    file << i << "\t" << msglens[i] << endl;
  }
  file.close();

  // prepare gnuplot script file
  ofstream script(script_file.c_str());
	script << "# Gnuplot script file for plotting data in file \"data\"\n\n" ;
	script << "set terminal post eps" << endl ;
	script << "set autoscale\t" ;
	script << "# scale axes automatically" << endl ;
	script << "set xtic auto\t" ;
	script << "# set xtics automatically" << endl ;
	script << "set ytic auto\t" ;
	script << "# set ytics automatically" << endl ;
	script << "set title \"# of components: " << K << "\"" << endl ;
	script << "set xlabel \"# of iterations\"" << endl ;
	script << "set ylabel \"message length (in bits)\"" << endl ;
	script << "set output \"" << plot_file << "\"" << endl ;
	script << "plot \"" << data_file << "\" using 1:2 notitle " 
         << "with linespoints lc rgb \"red\"" << endl ;
  script.close();
  string cmd = "gnuplot -persist " + script_file;
  if(system(cmd.c_str()));
  //cmd = "rm " + script_file;
}

/*!
 *  \brief This function is used to read the mixture details to aid in
 *  visualization.
 *  \param file_name a reference to a string
 *  \param D an integer
 */
void Mixture::load(string &file_name, int D)
{
  sample_size.clear();
  weights.clear();
  components.clear();
  K = 0;
  ifstream file(file_name.c_str());
  string line;
  Vector numbers;
  Vector unit_mean(D,0),mean(D,0);
  long double sum_weights = 0;
  while (getline(file,line)) {
    K++;
    boost::char_separator<char> sep("mukap,:()[] \t");
    boost::tokenizer<boost::char_separator<char> > tokens(line,sep);
    BOOST_FOREACH (const string& t, tokens) {
      istringstream iss(t);
      long double x;
      iss >> x;
      numbers.push_back(x);
    }
    weights.push_back(numbers[0]);
    sum_weights += numbers[0];
    for (int i=1; i<=D; i++) {
      mean[i-1] = numbers[i];
    }
    long double kappa = numbers[D+1];
    normalize(mean,unit_mean);
    Kent vmf(unit_mean,kappa);
    components.push_back(vmf);
    numbers.clear();
  }
  file.close();
  for (int i=0; i<K; i++) {
    weights[i] /= sum_weights;
  }
}

/*!
 *  \brief This function is used to read the mixture details 
 *  corresponding to the given data.
 *  \param file_name a reference to a string
 *  \param D an integer
 *  \param d a reference to a std::vector<Vector >
 *  \param dw a reference to a Vector
 */
void Mixture::load(string &file_name, int D, std::vector<Vector > &d,
              Vector &dw)
{
  load(file_name,D);
  data = d;
  N = data.size();
  data_weights = dw;
  Vector tmp(N,0);
  responsibility = std::vector<Vector >(K,tmp);
  /*for (int i=0; i<K; i++) {
    responsibility.push_back(tmp);
  }*/
  updateResponsibilityMatrix();
  sample_size = Vector(K,0);
  updateEffectiveSampleSize();
  updateComponents();
  minimum_msglen = computeMinimumMessageLength();
}

/*!
 *  \brief This function is used to randomly choose a component.
 *  \return the component index
 */
int Mixture::randomComponent()
{
  long double random = rand() / (long double) RAND_MAX;
  //cout << random << endl;
  long double previous = 0;
  for (int i=0; i<weights.size(); i++) {
    if (random <= weights[i] + previous) {
      return i;
    }
    previous += weights[i];
  }
}

/*!
 *  \brief This function saves the data generated from a component to a file.
 *  \param index an integer
 *  \param data a reference to a std::vector<Vector >
 */
void Mixture::saveComponentData(int index, std::vector<Vector > &data)
{
  string data_file = CURRENT_DIRECTORY + "/visualize/comp";
  data_file += boost::lexical_cast<string>(index+1) + ".dat";
  //components[index].printParameters(cout);
  ofstream file(data_file.c_str());
  for (int j=0; j<data.size(); j++) {
    for (int k=0; k<data[0].size(); k++) {
      file << fixed << setw(10) << setprecision(3) << data[j][k];
    }
    file << endl;
  }
  file.close();
}

/*!
 *  \brief This function is used to randomly sample from the mixture
 *  distribution.
 *  \param num_samples an integer
 *  \param save_data a boolean variable
 *  \return the random sample
 */
std::vector<Vector >
Mixture::generate(int num_samples, bool save_data) 
{
  sample_size = Vector(K,0);
  for (int i=0; i<num_samples; i++) {
    // randomly choose a component
    int k = randomComponent();
    sample_size[k]++;
  }
  /*ofstream fw("sample_size");
  for (int i=0; i<sample_size.size(); i++) {
    fw << sample_size[i] << endl;
  }
  fw.close();*/
  std::vector<Vector > sample;
  for (int i=0; i<K; i++) {
    std::vector<Vector > x = components[i].generate((int)sample_size[i]);
    if (save_data) {
      saveComponentData(i,x);
    }
    for (int j=0; j<x.size(); j++) {
      sample.push_back(x[j]);
    }
  }
  return sample;
}

/*!
 *  \brief This function splits a component into two.
 *  \return c an integer
 *  \param log a reference to a ostream
 *  \return the modified Mixture
 */
Mixture Mixture::split(int c, ostream &log)
{
  log << "\tSPLIT component " << c + 1 << " ... " << endl;

  int num_children = 2; 
  Mixture m(num_children,data,responsibility[c]);
  m.estimateParameters();
  log << "\t\tChildren:\n";
  m.printParameters(log,2); // print the child mixture

  // adjust weights
  Vector weights_c = m.getWeights();
  weights_c[0] *= weights[c];
  weights_c[1] *= weights[c];

  // adjust responsibility matrix
  std::vector<Vector > responsibility_c = m.getResponsibilityMatrix();
  for (int i=0; i<2; i++) {
    #pragma omp parallel for if(ENABLE_DATA_PARALLELISM) num_threads(NUM_THREADS) 
    for (int j=0; j<N; j++) {
      responsibility_c[i][j] *= responsibility[c][j];
    }
  }

  // adjust effective sample size
  Vector sample_size_c(2,0);
  for (int i=0; i<2; i++) {
    long double sum = 0;
    #pragma omp parallel for if(ENABLE_DATA_PARALLELISM) num_threads(NUM_THREADS) reduction(+:sum) 
    for (int j=0; j<N; j++) {
      sum += responsibility_c[i][j];
    }
    sample_size_c[i] = sum;
  }

  // child components
  std::vector<Kent> components_c = m.getComponents();

  // merge with the remaining components
  int K_m = K + 1;
  std::vector<Vector > responsibility_m(K_m);
  Vector weights_m(K_m,0),sample_size_m(K_m,0);
  std::vector<Kent> components_m(K_m);
  int index = 0;
  for (int i=0; i<K; i++) {
    if (i != c) {
      weights_m[index] = weights[i];
      sample_size_m[index] = sample_size[i];
      responsibility_m[index] = responsibility[i];
      components_m[index] = components[i];
      index++;
    } else if (i == c) {
      for (int j=0; j<2; j++) {
        weights_m[index] = weights_c[j];
        sample_size_m[index] = sample_size_c[j];
        responsibility_m[index] = responsibility_c[j];
        components_m[index] = components_c[j];
        index++;
      }
    }
  }

  Vector data_weights_m(N,1);
  Mixture merged(K_m,components_m,weights_m,sample_size_m,responsibility_m,data,data_weights_m);
  log << "\t\tBefore adjustment ...\n";
  merged.printParameters(log,2);
  merged.EM();
  log << "\t\tAfter adjustment ...\n";
  merged.printParameters(log,2);
  return merged;
}

/*!
 *  \brief This function splits a component into two.
 *  \return c an integer
 *  \param children a reference to a Mixture 
 *  \param intermediate a reference to a Mixture 
 *  \param modified a reference to a Mixture 
 */
void Mixture::split(int c, Mixture &children, Mixture &intermediate, Mixture &modified)
{
  int num_children = 2; 
  children = Mixture(num_children,data,responsibility[c]);
  children.estimateParameters();

  // adjust weights
  Vector weights_c = children.getWeights();
  weights_c[0] *= weights[c];
  weights_c[1] *= weights[c];

  // adjust responsibility matrix
  std::vector<Vector > responsibility_c = children.getResponsibilityMatrix();
  for (int i=0; i<2; i++) {
    for (int j=0; j<N; j++) {
      responsibility_c[i][j] *= responsibility[c][j];
    }
  }

  // adjust effective sample size
  Vector sample_size_c(2,0);
  for (int i=0; i<2; i++) {
    for (int j=0; j<N; j++) {
      sample_size_c[i] += responsibility_c[i][j];
    }
  }

  // child components
  std::vector<Kent> components_c = children.getComponents();

  // merge with the remaining components
  int K_m = K + 1;
  std::vector<Vector > responsibility_m(K_m);
  Vector weights_m(K_m,0),sample_size_m(K_m,0);
  std::vector<Kent> components_m(K_m);
  int index = 0;
  for (int i=0; i<K; i++) {
    if (i != c) {
      weights_m[index] = weights[i];
      sample_size_m[index] = sample_size[i];
      responsibility_m[index] = responsibility[i];
      components_m[index] = components[i];
      index++;
    } else if (i == c) {
      for (int j=0; j<2; j++) {
        weights_m[index] = weights_c[j];
        sample_size_m[index] = sample_size_c[j];
        responsibility_m[index] = responsibility_c[j];
        components_m[index] = components_c[j];
        index++;
      }
    }
  }

  Vector data_weights_m(N,1);
  // before adjustment
  intermediate = Mixture(K_m,components_m,weights_m,sample_size_m,responsibility_m,data,data_weights_m);
  // adjustment
  intermediate.EM();
  // after adjustment
  modified = intermediate;
}

/*!
 *  \brief This function deletes a component.
 *  \return c an integer
 *  \param log a reference to a ostream
 *  \return the modified Mixture
 */
Mixture Mixture::kill(int c, ostream &log)
{
  log << "\tKILL component " << c + 1 << " ... " << endl;

  int K_m = K - 1;
  // adjust weights
  Vector weights_m(K_m,0);
  long double residual_sum = 1 - weights[c];
  long double wt;
  int index = 0;
  for (int i=0; i<K; i++) {
    if (i != c) {
      weights_m[index++] = weights[i] / residual_sum;
    }
  }

  // adjust responsibility matrix
  Vector resp(N,0);
  std::vector<Vector > responsibility_m(K_m,resp);
  index = 0;
  for (int i=0; i<K; i++) {
    if (i != c) {
      #pragma omp parallel for if(ENABLE_DATA_PARALLELISM) num_threads(NUM_THREADS) private(residual_sum) 
      for (int j=0; j<N; j++) {
        residual_sum = 1 - responsibility[c][j];
        responsibility_m[index][j] = responsibility[i][j] / residual_sum;
      }
      index++;
    }
  }

  // adjust effective sample size
  Vector sample_size_m(K_m,0);
  for (int i=0; i<K-1; i++) {
    long double sum = 0;
    #pragma omp parallel for if(ENABLE_DATA_PARALLELISM) num_threads(NUM_THREADS) reduction(+:sum) 
    for (int j=0; j<N; j++) {
      sum += responsibility_m[i][j];
    }
    sample_size_m[i] = sum;
  }

  // child components
  std::vector<Kent> components_m(K_m);
  index = 0;
  for (int i=0; i<K; i++) {
    if (i != c) {
      components_m[index++] = components[i];
    }
  }

  log << "\t\tResidual:\n";
  Vector data_weights_m(N,1);
  Mixture modified(K_m,components_m,weights_m,sample_size_m,responsibility_m,data,data_weights_m);
  log << "\t\tBefore adjustment ...\n";
  modified.printParameters(log,2);
  modified.EM();
  log << "\t\tAfter adjustment ...\n";
  modified.printParameters(log,2);
  return modified;
}

/*!
 *  \brief This function deletes a component.
 *  \param c an integer
 *  \param residual a reference to a Mixture
 *  \param modified a reference to a Mixture
 */
void Mixture::kill(int c, Mixture &residual, Mixture &modified)
{
  int K_m = K - 1;

  // adjust weights
  Vector weights_m(K_m,0);
  long double residual_sum = 1 - weights[c];
  long double wt;
  int index = 0;
  for (int i=0; i<K; i++) {
    if (i != c) {
      weights_m[index++] = weights[i] / residual_sum;
    }
  }

  // adjust responsibility matrix
  Vector resp(N,0);
  std::vector<Vector > responsibility_m(K_m,resp);
  index = 0;
  for (int i=0; i<K; i++) {
    if (i != c) {
      for (int j=0; j<N; j++) {
        residual_sum = 1 - responsibility[c][j];
        responsibility_m[index][j] = responsibility[i][j] / residual_sum;
      }
      index++;
    }
  }

  // adjust effective sample size
  Vector sample_size_m(K_m,0);
  for (int i=0; i<K-1; i++) {
    for (int j=0; j<N; j++) {
      sample_size_m[i] += responsibility_m[i][j];
    }
  }

  // child components
  std::vector<Kent> components_m(K_m);
  index = 0;
  for (int i=0; i<K; i++) {
    if (i != c) {
      components_m[index++] = components[i];
    }
  }

  Vector data_weights_m(N,1);
  //before adjustment / residual
  residual = Mixture(K_m,components_m,weights_m,sample_size_m,responsibility_m,data,data_weights_m);
  // adjustment
  residual.EM();
  // after adjustment
  modified = residual;
}

/*!
 *  \brief This function joins two components.
 *  \return c1 an integer
 *  \return c2 an integer
 *  \param log a reference to a ostream
 *  \return the modified Mixture
 */
Mixture Mixture::join(int c1, int c2, ostream &log)
{
  log << "\tJOIN components " << c1+1 << " and " << c2+1 << " ... " << endl;

  int K_m = K - 1;
  // adjust weights
  Vector weights_m(K_m,0);
  int index = 0;
  for (int i=0; i<K; i++) {
    if (i != c1 && i != c2) {
      weights_m[index++] = weights[i];
    }
  }
  weights_m[index] = weights[c1] + weights[c2];

  // adjust responsibility matrix
  std::vector<Vector > responsibility_m(K_m);
  index = 0;
  for (int i=0; i<K; i++) {
    if (i != c1 && i != c2) {
      responsibility_m[index++] = responsibility[i];
    }
  }
  Vector resp(N,0);
  #pragma omp parallel for if(ENABLE_DATA_PARALLELISM) num_threads(NUM_THREADS) 
  for (int i=0; i<N; i++) {
    resp[i] = responsibility[c1][i] + responsibility[c2][i];
  }
  responsibility_m[index] = resp;

  // adjust effective sample size 
  Vector sample_size_m(K_m,0);
  index = 0;
  for (int i=0; i<K; i++) {
    if (i != c1 && i != c2) {
      sample_size_m[index++] = sample_size[i];
    }
  }
  sample_size_m[index] = sample_size[c1] + sample_size[c2];

  // child components
  std::vector<Kent> components_m(K_m);
  index = 0;
  for (int i=0; i<K; i++) {
    if (i != c1 && i != c2) {
      components_m[index++] = components[i];
    }
  }
  Mixture joined(1,data,resp);
  joined.estimateParameters();
  log << "\t\tResultant join:\n";
  joined.printParameters(log,2); // print the joined pair mixture

  std::vector<Kent> joined_comp = joined.getComponents();
  components_m[index++] = joined_comp[0];
  Vector data_weights_m(N,1);
  Mixture modified(K-1,components_m,weights_m,sample_size_m,responsibility_m,data,data_weights_m);
  log << "\t\tBefore adjustment ...\n";
  modified.printParameters(log,2);
  modified.EM();
  log << "\t\tAfter adjustment ...\n";
  modified.printParameters(log,2);
  return modified;
}

/*!
 *  \brief This function joins two components.
 *  \return c1 an integer
 *  \return c2 an integer
 *  \param joined a reference to a Mixture
 *  \param intermediate a reference to a Mixture
 *  \param modified a reference to a Mixture
 */
void Mixture::join(int c1, int c2, Mixture &joined, Mixture &intermediate, Mixture &modified)
{
  int K_m = K - 1;

  // adjust weights
  Vector weights_m(K_m,0);
  int index = 0;
  for (int i=0; i<K; i++) {
    if (i != c1 && i != c2) {
      weights_m[index++] = weights[i];
    }
  }
  weights_m[index] = weights[c1] + weights[c2];

  // adjust responsibility matrix
  std::vector<Vector > responsibility_m(K_m);
  index = 0;
  for (int i=0; i<K; i++) {
    if (i != c1 && i != c2) {
      responsibility_m[index++] = responsibility[i];
    }
  }
  Vector resp(N,0);
  for (int i=0; i<N; i++) {
    resp[i] = responsibility[c1][i] + responsibility[c2][i];
  }
  responsibility_m[index] = resp;

  // adjust effective sample size 
  Vector sample_size_m(K_m,0);
  index = 0;
  for (int i=0; i<K; i++) {
    if (i != c1 && i != c2) {
      sample_size_m[index++] = sample_size[i];
    }
  }
  sample_size_m[index] = sample_size[c1] + sample_size[c2];

  // child components
  std::vector<Kent> components_m(K_m);
  index = 0;
  for (int i=0; i<K; i++) {
    if (i != c1 && i != c2) {
      components_m[index++] = components[i];
    }
  }
  joined = Mixture(1,data,resp);
  joined.estimateParameters();

  // joined pair
  std::vector<Kent> joined_comp = joined.getComponents();
  components_m[index++] = joined_comp[0];
  Vector data_weights_m(N,1);
  // before adjustment
  intermediate = Mixture(K_m,components_m,weights_m,sample_size_m,responsibility_m,data,data_weights_m);
  // adjustment
  intermediate.EM();
  // after adjustment
  modified = intermediate;
}

/*!
 *  \brief This function generates data to visualize the 2D/3D heat maps.
 *  \param res a long double
 */
void Mixture::generateHeatmapData(long double res)
{
  string data_fbins2D = CURRENT_DIRECTORY + "/visualize/prob_bins2D.dat";
  string data_fbins3D = CURRENT_DIRECTORY + "/visualize/prob_bins3D.dat";
  ofstream fbins2D(data_fbins2D.c_str());
  ofstream fbins3D(data_fbins3D.c_str());
  Vector x(3,1);
  Vector point(3,0);
  for (long double theta=0; theta<180; theta+=res) {
    x[1] = theta * PI/180;
    for (long double phi=0; phi<360; phi+=res) {
      x[2] = phi * PI/180;
      spherical2cartesian(x,point);
      long double pr = probability(point);
      // 2D bins
      fbins2D << fixed << setw(10) << setprecision(4) << floor(pr * 100);
      // 3D bins
      for (int k=0; k<3; k++) {
        fbins3D << fixed << setw(10) << setprecision(4) << point[k];
      }
      fbins3D << fixed << setw(10) << setprecision(4) << pr << endl;
    }
    fbins2D << endl;
  }
  fbins2D.close();
  fbins3D.close();
}

/*!
 *  \brief This function identifies each component in a Mixture that is 
 *  close to another component in the other Mixture. 
 *  \param other a reference to a Mixture
 */
void Mixture::mapComponents(Mixture &other, std::vector<int> &mapping)
{
  std::vector<Kent> other_comps = other.getComponents();
  assert(components.size() == other_comps.size());
  int counter = 0;
  bool status = 0;

  while (!status) {
    counter++;
    mapping = std::vector<int>(K,0);
    std::vector<bool> flags(other_comps.size(),0);
    for (int i=0; i<K; i++) {
      // find the closest other_component to component i ...
      int nearest = components[i].getNearestComponentUsingDotProduct(other_comps);
      //int nearest = components[i].getNearestComponent(other_comps);
      mapping[i] = nearest;
      if (flags[nearest] == 0) {
        flags[nearest] = 1;
        if (i == K-1) status = 1;
      } else { // error
        cout << "Error in mapping components ...\n";
        print(cout,mapping); cout << endl;
        exit(1);
        status = 0;
        goto repeat;
      }
    } // for() loop ends ...
    repeat:
    if (counter > 5) {
      cout << counter << " still trying to map components ...\n";
    }
  }
  //print(cout,mapping);
}

/*!
 *  \brief This function classifies the data using this Mixture parameters.
 *  \param sample a reference to a std::vector<Vector >
 */
void Mixture::classify(std::vector<Vector > &sample)
{
  std::vector<std::vector<int> > assignments(K+1);
  std::vector<int> tmp;
  for (int i=0; i<K+1; i++) {
    assignments.push_back(tmp);
  }
  Vector mshp(K,0);
  long double max_mshp,prob_density;
  int class_index;

  for (int i=0; i<sample.size(); i++) {
    long double px = 0;
    for (int j=0; j<K; j++) {
      prob_density = components[j].density(sample[i]);
      mshp[j] = weights[j] * prob_density;
      px += mshp[j];
    }
    for (int j=0; j<K; j++) {
      mshp[j] /= px;
    }
    max_mshp = mshp[0];
    class_index = 0;
    for (int j=1; j<K; j++) {
      if (mshp[j] > max_mshp) {
        max_mshp = mshp[j];
        class_index = j;
      }
    }
    if (max_mshp <= 0.9) {
      class_index = K;  // mixed membership class (no clear assignment)
    }
    assignments[class_index].push_back(i);
  }

  string file_name;
  for (int i=0; i<K+1; i++) {
    if (i != K) {
      file_name = "visualize/class_" + boost::lexical_cast<string>(i+1) + ".dat";
    } else {
      file_name = "visualize/unassigned.dat";
    }
    ofstream file(file_name.c_str());
    for (int j=0; j<assignments[i].size(); j++) {
      int index = assignments[i][j];
      for (int k=0; k<sample[0].size(); k++) {
        file << fixed << setw(10) << setprecision(3) << sample[index][k];
      }
      file << endl;
    }
    file.close();
  }
}

/*!
 *  \brief This function computes the nearest component to a given component.
 *  \param c an integer
 *  \return the index of the closest component
 */
int Mixture::getNearestComponent(int c)
{
  int D = components[c].getDimensionality(); 
  long double current,dist = LARGE_NUMBER;
  int nearest;

  if (D != 3) {
    // generate a sample from the current component 
    int sample_size = 100;
    std::vector<Vector > sample = components[c].generate(sample_size);
    Vector sum_x(D,0);
    for (int i=0; i<sample.size(); i++) {
      for (int j=0; j<D; j++) {
        sum_x[j] += sample[i][j];
      }
    }
    for (int i=0; i<K; i++) {
      if (i != c) {
        current = components[c].distance(components[i],sum_x,sample_size);
        if (current < dist) {
          dist = current;
          nearest = i;
        }
      }
    }
  } else if (D == 3) {
    for (int i=0; i<K; i++) {
      if (i != c) {
        current = components[c].distance_3D(components[i]);
        if (current < dist) {
          dist = current;
          nearest = i;
        }
      }
    }
  }
  return nearest;
}

/*!
 *  \brief This function computes the Akaike information criteria (AIC)
 *  \return the AIC value (natural log -- nits)
 */
long double Mixture::computeAIC()
{
  int D = data[0].size();
  int k = (D+1)*K - 1;
  int n = data.size();
  long double neg_log_likelihood = negativeLogLikelihood(data);
  neg_log_likelihood -= n * (D-1) * log(AOM);
  return AIC(k,n,neg_log_likelihood);
}

long double Mixture::computeAIC_2()
{
  long double aic_e = computeAIC();
  return aic_e / log(2);
}

/*!
 *  \brief This function computes the Bayesian information criteria (AIC)
 *  \return the BIC value (natural log -- nits)
 */
long double Mixture::computeBIC()
{
  int D = data[0].size();
  int k = (D+1)*K - 1;
  int n = data.size();
  long double neg_log_likelihood = negativeLogLikelihood(data);
  neg_log_likelihood -= n * (D-1) * log(AOM);
  return BIC(k,n,neg_log_likelihood);
}

long double Mixture::computeBIC_2()
{
  long double bic_e = computeBIC();
  return bic_e / log(2);
}

/*!
 *  \brief Computes the KL-divergence between two mixtures
 */
long double Mixture::computeKLDivergence(Mixture &original)
{
  long double kldiv = 0,log_fx,log_gx;
  for (int i=0; i<data.size(); i++) {
    log_fx = log_probability(data[i]);
    log_gx = original.log_probability(data[i]);
    kldiv += log_fx - log_gx;
  }
  //assert(kldiv > 0);
  return kldiv/(log(2) * data.size());
}
