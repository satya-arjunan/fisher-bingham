#include "Support.h"
#include "Test.h"
#include "Experiments.h"
#include "Mixture.h"
#include "Structure.h"

Vector XAXIS,YAXIS,ZAXIS;
int MIXTURE_ID = 1;
int MIXTURE_SIMULATION;
int INFER_COMPONENTS;
int ENABLE_DATA_PARALLELISM;
int NUM_THREADS;
long double MAX_KAPPA;
long double IMPROVEMENT_RATE;

////////////////////// GENERAL PURPOSE FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\

/*!
 *  \brief This function checks to see if valid arguments are given to the 
 *  command line output.
 *  \param argc an integer
 *  \param argv an std::vector of strings
 *  \return the parameters of the model
 */
struct Parameters parseCommandLineInput(int argc, char **argv)
{
  struct Parameters parameters;
  string constrain;
  long double improvement_rate;

  bool noargs = 1;

  cout << "Checking command-line input ..." << endl;
  options_description desc("Allowed options");
  desc.add_options()
       ("help","produce help component")
       ("test","run some test cases")
       ("experiments","run experiments")
       ("iter",value<int>(&parameters.iterations),"number of iterations")
       ("profile",value<string>(&parameters.profile_file),"path to the profile")
       ("profiles",value<string>(&parameters.profiles_dir),"path to all profiles")
       ("max_kappa",value<long double>(&parameters.max_kappa),"maximum value of kappa allowed")
       ("mixture","flag to do mixture modelling")
       ("k",value<int>(&parameters.fit_num_components),"number of components")
       ("infer_components","flag to infer the number of components")
       ("min_k",value<int>(&parameters.min_components),"min components to infer")
       ("max_k",value<int>(&parameters.max_components),"max components to infer")
       ("log",value<string>(&parameters.infer_log),"log file")
       ("continue","flag to continue inference from some state")
       ("begin",value<int>(&parameters.start_from),"# of components to begin inference from")
       ("simulate","to simulate a mixture model")
       ("load",value<string>(&parameters.mixture_file),"mixture file")
       ("components",value<int>(&parameters.simulated_components),"# of simulated components")
       ("samples",value<int>(&parameters.sample_size),"sample size generated")
       ("bins","parameter to generate heat maps")
       ("res",value<long double>(&parameters.res),"resolution used in heat map images")
       ("mt",value<int>(&parameters.num_threads),"flag to enable multithreading")
  ;
  variables_map vm;
  store(command_line_parser(argc,argv).options(desc).run(),vm);
  notify(vm);

  if (vm.count("help")) {
    Usage(argv[0],desc);
  }

  if (vm.count("test")) {
    parameters.test = SET;
  } else {
    parameters.test = UNSET;
  }

  if (vm.count("experiments")) {
    parameters.experiments = SET;
    if (!vm.count("iter")) {
      parameters.iterations = 1;
    }
  } else {
    parameters.experiments = UNSET;
  }

  if (vm.count("bins")) {
    parameters.heat_map = SET;
    if (!vm.count("res")) {
      parameters.res = DEFAULT_RESOLUTION;
    }
  } else {
    parameters.heat_map = UNSET;
  }

  if (vm.count("profiles") || vm.count("profile")) {
    parameters.read_profiles = SET;
  } else {
    parameters.read_profiles = UNSET;
  }

  if (!vm.count("max_kappa")) {
    MAX_KAPPA = DEFAULT_MAX_KAPPA;
  } else {
    MAX_KAPPA = parameters.max_kappa;
  }

  if (vm.count("mixture")) {
    parameters.mixture_model = SET;
    if (!vm.count("k")) {
      parameters.fit_num_components = DEFAULT_FIT_COMPONENTS;
    }
    if (vm.count("infer_components")) {
      parameters.infer_num_components = SET;
      INFER_COMPONENTS = SET;
      if (!vm.count("max_k")) {
        parameters.max_components = -1;
        if (vm.count("continue")) {
          parameters.continue_inference = SET;
        } else {
          parameters.continue_inference = UNSET;
        }
        if (!vm.count("begin")) {
          parameters.start_from = 1;
        }
      }
    } else {
      parameters.infer_num_components = UNSET;
      INFER_COMPONENTS = UNSET;
    }
  } else {
    parameters.mixture_model = UNSET;
  }

  if (vm.count("simulate")) {
    parameters.simulation = SET;
    MIXTURE_SIMULATION = SET;
    if (!vm.count("samples")) {
      parameters.sample_size = DEFAULT_SAMPLE_SIZE;
    }
    if (vm.count("load")) {
      parameters.load_mixture = SET;
    } else {
      parameters.load_mixture = UNSET;
      if (!vm.count("components")) {
        parameters.simulated_components = DEFAULT_SIMULATE_COMPONENTS;
      }
    }
  } else {
    parameters.simulation = UNSET;
    MIXTURE_SIMULATION = UNSET;
  }

  if (vm.count("mt")) {
    NUM_THREADS = parameters.num_threads;
    ENABLE_DATA_PARALLELISM = SET;
  } else {
    ENABLE_DATA_PARALLELISM = UNSET;
    NUM_THREADS = 1;
  }

  if (vm.count("improvement")) {
    IMPROVEMENT_RATE = improvement_rate;
  } else {
    IMPROVEMENT_RATE = 0.001; // 0.1 % default
  }

  return parameters;
}

/*!
 *  \brief This module prints the acceptable input format to the program
 *  \param exe a reference to a const char
 *  \param desc a reference to a options_description object
 */
void Usage(const char *exe, options_description &desc)
{
  cout << "Usage: " << exe << " [options]" << endl;
  cout << desc << endl;
  exit(1);
}

/*!
 *  \brief This module checks whether the input file exists or not.
 *  \param file_name a reference to a string
 *  \return true or false depending on whether the file exists or not.
 */
bool checkFile(string &file_name)
{
  ifstream file(file_name.c_str());
  return file;
}

/*!
 *  \brief This module prints the elements of a std::vector<Vector<> > to a file
 *  \param v a reference to std::vector<Vector >
 *  \param file_name a pointer to a const char
 */
void writeToFile(const char *file_name, std::vector<Vector > &v, int precision)
{
  ofstream file(file_name);
  for (int i=0; i<v.size(); i++) {
    for (int j=0; j<v[i].size(); j++) {
      file << fixed << setw(10) << setprecision(precision) << v[i][j];
    }
    file << endl;
  }
  file.close(); 
}

/*!
 *  \brief This module extracts the file name from the path
 *  \param file a reference to a string
 *  \return the extracted portion of the file name
 */
string extractName(string &file)
{
  unsigned pos1 = file.find_last_of("/");
  unsigned pos2 = file.find(".");
  int length = pos2 - pos1 - 1;
  string sub = file.substr(pos1+1,length);
  return sub;
}

/*!
 *  \brief This function prints the elements of an Vector.
 *  \param os a reference to a ostream
 *  \param v a reference to a Vector
 */
void print(ostream &os, Vector &v, int precision)
{
  if (precision == 0) {
    if (v.size() == 1) {
      os << scientific << "(" << v[0] << ")";
    } else if (v.size() > 1) {
      os << scientific << "(" << v[0] << ", ";
      for (int i=1; i<v.size()-1; i++) {
        os << scientific << v[i] << ", ";
      }
      os << scientific << v[v.size()-1] << ")\t";
    } else {
      os << "No elements in v ...";
    }
  } else if (precision != 0) { // scientific notation
    if (v.size() == 1) {
      os << fixed << setprecision(3) << "(" << v[0] << ")";
    } else if (v.size() > 1) {
      os << fixed << setprecision(3) << "(" << v[0] << ", ";
      for (int i=1; i<v.size()-1; i++) {
        os << fixed << setprecision(3) << v[i] << ", ";
      }
      os << fixed << setprecision(3) << v[v.size()-1] << ")\t";
    } else {
      os << "No elements in v ...";
    }
  }
}

void print(string &type, struct Estimates &estimates)
{
  cout << "TYPE: " << type << endl;
  Vector spherical(3,0);
  cartesian2spherical(estimates.mean,spherical);
  cout << "m0_est: "; print(cout,estimates.mean,3);
  cout << "\t(" << spherical[1]*180/PI << "," << spherical[2]*180/PI << ")\n";
  cartesian2spherical(estimates.major_axis,spherical);
  cout << "m1_est: "; print(cout,estimates.major_axis,3);
  cout << "\t(" << spherical[1]*180/PI << "," << spherical[2]*180/PI << ")\n";
  cartesian2spherical(estimates.minor_axis,spherical);
  cout << "m2_est: "; print(cout,estimates.minor_axis,3);
  cout << "\t(" << spherical[1]*180/PI << "," << spherical[2]*180/PI << ")\n";
  cout << "kappa_est: " << estimates.kappa << "; beta_est: " << estimates.beta << endl;
  // check:
  cout << fixed << scientific
       << "m0_est . m1_est = " << computeDotProduct(estimates.mean,estimates.major_axis)
       << "; m0_est . m2_est = " << computeDotProduct(estimates.mean,estimates.minor_axis)
       << "; m1_est . m2_est = " << computeDotProduct(estimates.major_axis,estimates.minor_axis)
       << endl;
}

////////////////////// MATH FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\

/*!
 *  \brief This module returns the sign of a number.
 *  \param number a long double
 *  \return the sign
 */
int sign(long double number)
{
  if (fabs(number) <= ZERO) {
    return 0;
  } else if (number > 0) {
    return 1;
  } else {
    return -1;
  }
}

/*!
 *  \brief Normalizes a Vector
 *  \param x a reference to a Vector
 *  \param unit a reference to a Vector
 *  \return the norm of the Vector
 */
long double normalize(Vector &x, Vector &unit)
{
  long double l2norm = norm(x);
  for (int i=0; i<x.size(); i++) {
    unit[i] = x[i] / l2norm;
  }
  return l2norm;
}

long double norm(Vector &v)
{
  long double normsq = 0;
  for (int i=0; i<v.size(); i++) {
    normsq += v[i] * v[i];
  }
  return sqrt(normsq);
}

/*!
 *  \brief This function converts the cartesian coordinates into spherical.
 *  (theta with +Z and phi with +X)
 *  \param cartesian a reference to a Vector 
 *  \param spherical a reference to a Vector 
 */
void cartesian2spherical(Vector &cartesian, Vector &spherical)
{
  Vector unit(3,0);
  long double r = normalize(cartesian,unit);

  long double x = unit[0];
  long double y = unit[1];
  long double z = unit[2];

  // theta \in [0,PI]: angle with Z-axis
  long double theta = acos(z);

  // phi \in[0,2 PI]: angle with positive X-axis
  long double ratio = x/sin(theta);
  if (ratio > 1) {
    ratio = 1;
  } else if (ratio < -1) {
    ratio = -1;
  }
  long double angle = acos(ratio);
  long double phi = 0;
  if (x == 0 && y == 0) {
    phi = 0;
  } else if (x == 0) {
    if (y > 0) {
      phi = angle;
    } else {
      phi = 2 * PI - angle;
    }
  } else if (y >= 0) {
    phi = angle;
  } else if (y < 0) {
    phi = 2 * PI - angle;
  }

  spherical[0] = r;
  spherical[1] = theta;
  spherical[2] = phi;
}

/*!
 *  \brief This function converts the cartesian coordinates into spherical.
 *  (theta with +X and phi with +Y)
 *  \param cartesian a reference to a Vector 
 *  \param spherical a reference to a Vector 
 */
void cartesian2sphericalPoleXAxis(Vector &cartesian, Vector &spherical)
{
  Vector unit(3,0);
  long double r = normalize(cartesian,unit);

  long double x = unit[0];
  long double y = unit[1];
  long double z = unit[2];

  // theta \in [0,PI]: angle with X-axis
  long double theta = acos(x);

  // phi \in[0,2 PI]: angle with positive Y-axis
  long double ratio = y/sin(theta);
  if (ratio > 1) {
    ratio = 1;
  } else if (ratio < -1) {
    ratio = -1;
  }
  long double angle = acos(ratio);
  long double phi = 0;
  if (y == 0 && z == 0) {
    phi = 0;
  } else if (y == 0) {
    if (z > 0) {
      phi = angle;
    } else {
      phi = 2 * PI - angle;
    }
  } else if (z >= 0) {
    phi = angle;
  } else if (z < 0) {
    phi = 2 * PI - angle;
  }

  spherical[0] = r;
  spherical[1] = theta;
  spherical[2] = phi;
}

/*!
 *  \brief This function converts the spherical coordinates into cartesian.
 *  \param spherical a reference to a Vector 
 *  \param cartesian a reference to a Vector 
 */
void spherical2cartesian(Vector &spherical, Vector &cartesian)
{
  cartesian[0] = spherical[0] * sin(spherical[1]) * cos(spherical[2]);
  cartesian[1] = spherical[0] * sin(spherical[1]) * sin(spherical[2]);
  cartesian[2] = spherical[0] * cos(spherical[1]);
}

/*!
 *  \brief This funciton computes the dot product between two Vectors.
 *  \param v1 a reference to a Vector
 *  \param v2 a reference to a Vector
 *  \return the dot product
 */
long double computeDotProduct(Vector &v1, Vector &v2) 
{
  assert(v1.size() == v2.size());
  long double dot_product = 0;
  for (int i=0; i<v1.size(); i++) {
    dot_product += v1[i] * v2[i];
  }
  return dot_product;
}

Vector crossProduct(Vector &v1, Vector &v2) 
{
  Vector ans(3,0);
  ans[0] = v1[1] * v2[2] - v1[2] * v2[1];
  ans[1] = v1[2] * v2[0] - v1[0] * v2[2];
  ans[2] = v1[0] * v2[1] - v1[1] * v2[0];
  return ans;
}

/*!
 *  \brief This function computes the surface area of nd-sphere
 *  Surface area = Gamma(d/2+1)/d \pi^(d/2)
 *  \param d an integer
 *  \return the surface area
 */
long double computeLogSurfaceAreaSphere(int d)
{
  long double log_num = log(d) + ((d/2.0) * log(PI));
  long double log_denom = boost::math::lgamma<long double>(d/2.0+1);
  return (log_num - log_denom);
}

/*!
 *  \brief This function computes the log of modified Bessel function value
 *  (used for numerical stability reasons)
 */
long double logModifiedBesselFirstKind(long double alpha, long double x)
{
  if (!(alpha >= 0 && fabs(x) >= 0)) {
    cout << "Error logModifiedBesselFirstKind: (alpha,x) = (" << alpha << "," << x << ")\n";
    //exit(1);
  }
  long double t;
  if (alpha == 0) {
    t = 1;
  } else if (fabs(x) <= TOLERANCE) {
    t = 0;
    return 0;
  } 
  long double x2_4 = x * x * 0.25;
  long double R = 1.0,             // 0-th term
         I = 1.0,             // m=0 sum
         m = 1.0;             // next, m=1
  // m! = m*(m-1)!, and
  // Gamma(m+alpha+1) = (m+alpha)*Gamma(m+alpha), 
  // because Gamma(x+1) = x*Gamma(x), for all x > 0.
  do { 
    long double tmp = x2_4 / (m*(alpha+m));  // the m-th term
    R *= tmp;  // the m-th term
    I += R;                     // total
    if (R >= INFINITY || I >= INFINITY) {
      return INFINITY;
    }
    m += 1.0;
    //cout << "m: " << m << "; tmp: " << tmp << "; R: " << R << "; I: " << I << endl;
  } while( R >= I * ZERO);
  long double log_mod_bessel = log(I) + (alpha * log(x/2.0)) - lgamma<long double>(alpha+1);
  return log_mod_bessel;
}

/*!
 *  Find the roots of a quadratic: ax^2 + bx + c = 0
 */
void solveQuadratic(
  Vector &roots, 
  long double a,
  long double b, 
  long double c
) {
  long double D = sqrt(b*b-4*a*c);
  roots[0] = (-b + D) / (2*a);
  roots[1] = (-b - D) / (2*a);
}

////////////////////// GEOMETRY FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\

std::vector<Vector > load_matrix(string &file_name)
{
  std::vector<Vector > sample;
  ifstream file(file_name.c_str());
  string line;
  Vector numbers(3,0),unit_vector(3,0);
  int i;
  while (getline(file,line)) {
    boost::char_separator<char> sep(" \t");
    boost::tokenizer<boost::char_separator<char> > tokens(line,sep);
    i = 0;
    BOOST_FOREACH(const string &t, tokens) {
      istringstream iss(t);
      long double x;
      iss >> x;
      numbers[i++] = x;
    }
    normalize(numbers,unit_vector);
    sample.push_back(unit_vector);
  }
  file.close();
  return sample;
}

/*!
 *  v1 and v2 are considered to be a column std::vectors
 *  output: v1 * v2' (the outer product matrix)
 */
Matrix outer_prod(Vector &v1, Vector &v2)
{
  assert(v1.size() == v2.size());
  int m = v1.size();
  Matrix ans(m,m);
  for (int i=0; i<m; i++) {
    for (int j=0; j<m; j++) {
      ans(i,j) = v1[i] * v2[j];
    }
  }
  return ans;
}

/*!
 *  v is considered to be a column std::vector
 *  output: m * v (a row std::vector)
 */
Vector prod(Matrix &m, Vector &v)
{
  assert(m.size2() == v.size());
  Vector ans(m.size1(),0);
  for (int i=0; i<m.size1(); i++) {
    for (int j=0; j<m.size2(); j++) {
      ans[i] += m(i,j) * v[j];
    }
  }
  return ans;
}

/*!
 *  v is considered to be a column std::vector
 *  output: v' * m (a row std::vector)
 */
Vector prod(Vector &v, Matrix &m)
{
  assert(m.size1() == v.size());
  Vector ans(m.size2(),0);
  for (int i=0; i<m.size2(); i++) {
    for (int j=0; j<m.size1(); j++) {
      ans[i] += v[j] * m(j,i);
    }
  }
  return ans;
}

/*!
 *  v is considered to be a column std::vector
 *  output: v' M v
 */
long double prod_vMv(Vector &v, Matrix &M)
{
  Vector vM = prod(v,M);
  return computeDotProduct(vM,v);
}

/*!
 *  x,y are considered to be a column std::vectors
 *  output: x' M y
 */
long double prod_xMy(Vector &x, Matrix &M, Vector &y)
{
  Vector xM = prod(x,M);
  return computeDotProduct(xM,y);
}

/*!
 *  determinant of 3 X 3 matrix
 */
long double determinant(Matrix &m)
{
  long double det = 0,subdet;
  subdet = m(1,1) * m(2,2) - m(1,2) * m(2,1);
  det += m(0,0) * subdet;

  subdet = m(1,0) * m(2,2) - m(1,2) * m(2,0);
  det -= m(0,1) * subdet;

  subdet = m(1,0) * m(2,1) - m(1,1) * m(2,0);
  det += m(0,2) * subdet;

  return det;
}

/*!
 *  Computes \sum x (x is a vector)
 */
Vector computeVectorSum(std::vector<Vector > &sample) 
{
  int d = sample[0].size();
  Vector sum(d,0);
  for (int i=0; i<sample.size(); i++) {
    for (int j=0; j<d; j++) {
      sum[j] += sample[i][j];
    }
  }
  return sum;
}

Vector computeVectorSum(std::vector<Vector > &sample, Vector &weights, long double &Neff) 
{
  int d = sample[0].size();
  Vector sum(d,0);
  Neff = 0;
  for (int i=0; i<sample.size(); i++) {
    for (int j=0; j<d; j++) {
      sum[j] += sample[i][j] * weights[i];
    }
    Neff += weights[i];
  }
  return sum;
}

/*!
 *  Computes \sum x / N (x is a vector)
 */
Vector computeNormalizedVectorSum(std::vector<Vector > &sample) 
{
  Vector sum = computeVectorSum(sample);
  for (int j=0; j<sum.size(); j++) {
    sum[j] /= sample.size();
  }
  return sum;
}

/*!
 *  Computes \sum x * x' (x is a vector)
 */
Matrix computeDispersionMatrix(std::vector<Vector > &sample)
{
  int d = sample[0].size();
  Matrix dispersion = ZeroMatrix(d,d);
  for (int i=0; i<sample.size(); i++) {
    dispersion += outer_prod(sample[i],sample[i]);
  }
  return dispersion;
}

Matrix computeDispersionMatrix(std::vector<Vector > &sample, Vector &weights)
{
  int d = sample[0].size();
  Matrix dispersion = ZeroMatrix(d,d);
  Matrix tmp;
  for (int i=0; i<sample.size(); i++) {
    tmp = outer_prod(sample[i],sample[i]);
    dispersion += (weights[i] * tmp);
  }
  return dispersion;
}

/*!
 *  Computes \sum x * x' / N (x is a vector)
 */
Matrix computeNormalizedDispersionMatrix(std::vector<Vector > &sample)
{
  Matrix dispersion = computeDispersionMatrix(sample);
  return dispersion/sample.size();
}

// anti-clockwise rotation about +Y
Matrix rotate_about_yaxis(long double theta)
{
  Matrix r = IdentityMatrix(3,3);
  r(0,0) = cos(theta);
  r(0,2) = sin(theta);
  r(2,0) = -r(0,2); // -sin(theta)
  r(2,2) = r(0,0);  // cos(theta)
  return r;
}

// anti-clockwise rotation about +Z
Matrix rotate_about_zaxis(long double theta)
{
  Matrix r = IdentityMatrix(3,3);
  r(0,0) = cos(theta);
  r(0,1) = -sin(theta);
  r(1,0) = -r(0,1); // sin(theta)
  r(1,1) = r(0,0);  // cos(theta)
  return r;
}

/*!
 *  Matrix to rotate FROM the standard frame of reference.
 */
Matrix computeOrthogonalTransformation(
  Vector &mean,
  Vector &major_axis
) {
  Matrix r1 = align_zaxis_with_vector(mean);
  Matrix r_inv = trans(r1);
  Vector mj_xy = prod(r_inv,major_axis);
  Vector spherical(3,0);
  cartesian2spherical(mj_xy,spherical);
  long double psi = spherical[2];
  Matrix r2 = rotate_about_zaxis(psi);
  Matrix r = prod(r1,r2);
  return r;
}

/*!
 *  Matrix to rotate FROM the standard frame of reference.
 */
Matrix computeOrthogonalTransformation(long double psi, long double alpha, long double eta)
{
  Matrix r1 = rotate_about_zaxis(psi);
  Matrix r2 = rotate_about_yaxis(alpha);
  Matrix r3 = rotate_about_zaxis(eta);
  Matrix tmp = prod(r3,r2);
  Matrix r = prod(tmp,r1);
  return r;
}

void computeOrthogonalTransformation(
  Vector &mean, 
  Vector &major_axis,
  long double &psi,
  long double &alpha,
  long double &eta
) {
  Vector spherical(3,0);
  cartesian2spherical(mean,spherical);
  alpha = spherical[1];
  eta = spherical[2];

  Matrix r1 = align_zaxis_with_vector(mean);
  Matrix r_inv = trans(r1);
  Vector mj_xy = prod(r_inv,major_axis);
  cartesian2spherical(mj_xy,spherical);
  psi = spherical[2];
}

Matrix align_zaxis_with_vector(Vector &y)
{
  Vector spherical(3,0);
  cartesian2spherical(y,spherical);
  long double theta = spherical[1];
  long double phi = spherical[2];

  Matrix r1 = IdentityMatrix(3,3);
  r1(0,0) = cos(theta);
  r1(0,2) = sin(theta);
  r1(2,0) = -r1(0,2); //-sin(theta)
  r1(2,2) = r1(0,0); //cos(theta)

  Matrix r2 = IdentityMatrix(3,3);
  r2(0,0) = cos(phi);
  r2(0,1) = -sin(phi);
  r2(1,0) = -r2(0,1); //sin(phi)
  r2(1,1) = r2(0,0);  // cos(phi)

  Matrix r = prod(r2,r1);
  return r;
}

Matrix align_vector_with_zaxis(Vector &y)
{
  Matrix r = align_zaxis_with_vector(y);
  return trans(r);
}

void generateRandomOrthogonalVectors(
  Vector &mean,
  Vector &major_axis,
  Vector &minor_axis
) {
  long double phi = rand()*2*PI/(long double)RAND_MAX;
  Vector spherical(3,1),major1(3,0);
  spherical[1] = PI/2;
  spherical[2] = phi;
  spherical2cartesian(spherical,major1); // major axis
  Vector mu1 = ZAXIS;
  //Vector minor1 = crossProduct(mu1,major1);

  long double theta = rand()*PI/(long double)RAND_MAX;
  phi = rand()*2*PI/(long double)RAND_MAX;
  spherical[1] = theta;
  spherical[2] = phi;
  mean = Vector(3,0);
  spherical2cartesian(spherical,mean); 

  Matrix r = align_zaxis_with_vector(mean);
  major_axis = prod(r,major1);
  //minor_axis = prod(r,minor1);
  minor_axis = crossProduct(mean,major_axis);
}

/*
 *  \brief Transformation of x using T
 *  \param x a reference to a vector<vector<long double> >
 *  \param T a reference to a Matrix<long double>
 *  \return the transformed vector list
 */
std::vector<Vector > transform(
  std::vector<Vector > &x, 
  Matrix &T
) {
  std::vector<Vector > y(x.size());
  for (int i=0; i<x.size(); i++) {
    y[i] = prod(T,x[i]);
  }
  return y;
}

/*!
 *  Matrix inverse C++ Boost::ublas
 */
bool invertMatrix(const Matrix &input, Matrix &inverse)
{
  typedef permutation_matrix<std::size_t> pmatrix;

  // create a working copy of the input
  Matrix A(input);

  // create a permutation matrix for the LU-factorization
  pmatrix pm(A.size1());

  // perform LU-factorization
  int res = lu_factorize(A, pm);
  if (res != 0)
    return false;

  // create identity matrix of "inverse"
  inverse.assign(IdentityMatrix (A.size1()));

  // backsubstitute to get the inverse
  lu_substitute(A, pm, inverse);

  return true;
}

/*!
 *  Eigen decomposition
 *  Inputs:
 *                m -- a symmetric matrix
 *    eigen_vectors -- an identity matrix
 *  Outputs:
 *    eigen_vectors -- each column is a unit eigen vector
 */
void eigenDecomposition(
  Matrix m, 
  Vector &eigen_values,
  Matrix &eigen_vectors
) {
  // check if m is symmetric
  int num_rows = m.size1();
  int num_cols = m.size2();
  if (num_rows != num_cols) {
    cout << "Error: rows: " << num_rows << " != cols: " << num_cols << endl;
    exit(1);
  }
  for (int i=0; i<num_rows; i++) {
    for (int j=0; j<num_cols; j++) {
      if (fabs(m(i,j)-m(j,i)) >= ZERO) {
        cout << "Error: Matrix is not symmetric ...\n";
        cout << "m: " << m << endl;
        cout << "m(" << i << "," << j << ") != m(" << j << "," << i << ")\n";
        exit(1);
      }
    }
  }

  // matrix is now validated ...
  int MAX_ITERATIONS = 100;
  for (int i=0; i < MAX_ITERATIONS; i++) {
    //find the largest off-diagonal 
    int max_row = 0, max_col = 1;
    int cur_row, cur_col;
    long double max_val = m(max_row,max_col);
    for (cur_row = 0; cur_row < num_rows-1; ++cur_row) {
      for (cur_col = cur_row + 1; cur_col < num_cols; ++cur_col) {
        if (fabs(m(cur_row,cur_col)) > max_val) {
          max_row = cur_row;
          max_col = cur_col;
          max_val = fabs(m(cur_row,cur_col));
        }
      }
    }

    if (max_val <= ZERO) {
      break; //finished
    }

    jacobiRotateMatrix(m,eigen_vectors,max_row,max_col);
  }

  for (int i = 0; i < num_cols; i++) {
    eigen_values[i] = m(i,i);
  }

  cout << "eigen_values: "; print(cout,eigen_values,0); cout << endl;
  cout << "eigen_vectors: " << eigen_vectors << endl;
}

void jacobiRotateMatrix(
  Matrix &m,
  Matrix &eigen_vectors, 
  int max_row, 
  int max_col
) {
  long double diff = m(max_col,max_col) - m(max_row,max_row);
  long double phi, t, c, s, tau, temp;
  int i;
  
  phi = diff / (2.0 * m(max_row,max_col));
  t = 1.0 / (std::fabs(phi) + std::sqrt((phi*phi) + 1.0));
  if(phi < 0){ t = -t; }

  c = 1.0 / std::sqrt(t*t + 1.0);
  s = t*c;
  tau = s/(1.0 + c);

  temp = m(max_row,max_col);
  m(max_row,max_col) = 0;
  m(max_row,max_row) = m(max_row,max_row) - (t*temp);
  m(max_col,max_col) = m(max_col,max_col) + (t*temp);
  for(i = 0; i < max_row; i++){ // Case i < max_row
    temp = m(i,max_row);
    m(i,max_row) = temp - (s*(m(i,max_col) + (tau*temp)));
    m(i,max_col) = m(i,max_col) + (s*(temp - (tau*m(i,max_col))));
  }
  for(i = max_row + 1; i < max_col; i++){ // Case max_row < i < max_col
    temp = m(max_row,i);
    m(max_row,i) = temp - (s*(m(i,max_col) + (tau*m(max_row,i))));
    m(i,max_col) = m(i,max_col) + (s*(temp - (tau*m(i,max_col))));
  }
  for(i = max_col + 1; i < m.size2(); i++){ // Case i > max_col
    temp = m(max_row,i);
    m(max_row,i) = temp - (s*(m(max_col,i) + (tau*temp)));
    m(max_col,i) = m(max_col,i) + (s*(temp - (tau*m(max_col,i))));
  }

  for (i = 0; i < eigen_vectors.size1(); i++) { // update the transformation matrix
    temp = eigen_vectors(i,max_row);
    eigen_vectors(i,max_row) = temp
      - (s*(eigen_vectors(i,max_col) + (tau*temp)));
    eigen_vectors(i,max_col) = eigen_vectors(i,max_col)
      + (s*(temp - (tau*eigen_vectors(i,max_col))));
  }
  return;
}

/*
 *  Dawson's integral required to compute FB4 normalization constant
 */
long double computeDawsonsIntegral(double limit)
{
  //state_type x (1,0.0);
  std::vector<double> x (1,0.0);
  //integrate(rhs,x,0.0,limit,0.1,track);
  integrate(rhs,x,0.0,limit,0.1);
  //cout << "ans: " << x[0] << endl;
  return x[0];
}

void rhs(const std::vector<double> &x, std::vector<double> &dxdt, const double t)
{
    dxdt[0] = 1 - (2 * t * x[0]); 
}

void track(const std::vector<double> &x, const double t)
{
    cout << t << "\t" << x[0] << endl;
}

double Constraint(const std::vector<double> &x, std::vector<double> &grad, void *data)
{
    //double k = x[0];
    //double b = x[1];
    return (2 * x[1] - x[0]);
}

////////////////////// MIXTURE FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\

/*!
 *  \brief This function computes the approximation of the constant term for
 *  the constant term in the message length expression (pg. 257 Wallace)
 *  \param d an integer
 *  \return the constant term
 */
long double computeConstantTerm(int d)
{
  long double ad = 0;
  ad -= 0.5 * d * log(2 * PI);
  ad += 0.5 * log(d * PI);
  return ad;
}

/*!
 *  \brief This function bins the sample data 
 *  \param res a long double
 *  \param unit_coordinates a reference to a vector<vector<long double> > 
 */
std::vector<std::vector<int> > updateBins(std::vector<Vector > &unit_coordinates, long double res)
{
  std::vector<std::vector<int> > bins;
  int num_rows = 180 / res;
  int num_cols = 360 / res;
  std::vector<int> tmp(num_cols,0);
  for (int i=0; i<num_rows; i++) {
    bins.push_back(tmp);
  }

  long double theta,phi;
  int row,col;
  Vector spherical(3,0);
  for (int i=0; i<unit_coordinates.size(); i++) {
    //cout << "i: " << i << endl; 
    cartesian2spherical(unit_coordinates[i],spherical);
    theta = spherical[1] * 180 / PI;
    if (fabs(theta) <= ZERO) {
      row = 0;
    } else {
      row = (int)(ceil(theta/res) - 1);
    }
    phi = spherical[2] * 180 / PI;
    if (fabs(phi) <= ZERO) {
      col = 0;
    } else {
      col = (int)(ceil(phi/res) - 1);
    }
    if (row >= bins.size() || col >= bins[0].size()) {
      cout << "outside bounds: " << row << " " << col << "\n";
      cout << "theta: " << theta << " phi: " << phi << endl;
      cout << "spherical_1: " << spherical[1] << " spherical_2: " << spherical[2] << endl;
      cout << "unit_coordinates[i]_1: " << unit_coordinates[i][1] << " unit_coordinates[i]_2: " << unit_coordinates[i][2] << endl;
      fflush(stdout);
    }
    bins[row][col]++;
    //cout << "row,col: " << row << "," << col << endl;
  }
  return bins;
}

/*!
 *  \brief This function outputs the bin data.
 *  \param bins a reference to a std::vector<std::vector<int> >
 */
void outputBins(std::vector<std::vector<int> > &bins, long double res)
{
  long double theta=0,phi;
  string fbins2D_file,fbins3D_file;
  fbins2D_file = "./visualize/bins2D.dat";
  fbins3D_file = "./visualize/bins3D.dat";
  ofstream fbins2D(fbins2D_file.c_str());
  ofstream fbins3D(fbins3D_file.c_str());
  Vector cartesian(3,0);
  Vector spherical(3,1);
  for (int i=0; i<bins.size(); i++) {
    phi = 0;
    spherical[1] = theta * PI / 180;
    for (int j=0; j<bins[i].size(); j++) {
      fbins2D << fixed << setw(10) << bins[i][j];
      phi += res;
      spherical[2] = phi * PI / 180;
      spherical2cartesian(spherical,cartesian);
      for (int k=0; k<3; k++) {
        fbins3D << fixed << setw(10) << setprecision(4) << cartesian[k];
      }
      fbins3D << fixed << setw(10) << bins[i][j] << endl;
    }
    theta += res;
    fbins2D << endl;
  }
  fbins2D.close();
  fbins3D.close();
}

/*!
 *  \brief This function is used to read the angular profiles and use this data
 *  to estimate parameters of a Von Mises distribution.
 *  \param parameters a reference to a struct Parameters
 */
void computeEstimators(struct Parameters &parameters)
{
  std::vector<Vector > unit_coordinates;
  bool success = gatherData(parameters,unit_coordinates);
  if (parameters.heat_map == SET) {
    std::vector<std::vector<int> > bins = updateBins(unit_coordinates,parameters.res);
    outputBins(bins,parameters.res);
  }
  if (success && parameters.mixture_model == UNSET) {  // no mixture modelling
    modelOneComponent(parameters,unit_coordinates);
  } else if (success && parameters.mixture_model == SET) { // mixture modelling
    modelMixture(parameters,unit_coordinates);
  }
}

/*!
 *  \brief This function reads through the profiles from a given directory
 *  and collects the data to do mixture modelling.
 *  \param parameters a reference to a struct Parameters
 *  \param unit_coordinates a reference to a std::vector<Vector >
 */
bool gatherData(struct Parameters &parameters, std::vector<Vector > &unit_coordinates)
{
  if (parameters.profile_file.compare("") == 0) {
    path p(parameters.profiles_dir);
    cout << "path: " << p.string() << endl;
    if (exists(p)) { 
      if (is_directory(p)) { 
        std::vector<path> files; // store paths,
        copy(directory_iterator(p), directory_iterator(), back_inserter(files));
        cout << "# of profiles: " << files.size() << endl;
        int tid;
        std::vector<std::vector<Vector > > _unit_coordinates(NUM_THREADS);
        #pragma omp parallel num_threads(NUM_THREADS) private(tid)
        {
          tid = omp_get_thread_num();
          if (tid == 0) {
            cout << "# of threads: " << omp_get_num_threads() << endl;
          }
          #pragma omp for 
          for (int i=0; i<files.size(); i++) {
            Structure structure;
            structure.load(files[i]);
            std::vector<Vector > coords = structure.getUnitCoordinates();
            for (int j=0; j<coords.size(); j++) {
              _unit_coordinates[tid].push_back(coords[j]);
            }
          }
        }
        for (int i=0; i<NUM_THREADS; i++) {
          for (int j=0; j<_unit_coordinates[i].size(); j++) {
            unit_coordinates.push_back(_unit_coordinates[i][j]);
          }
        }
        cout << "# of profiles read: " << files.size() << endl;
        return 1;
      } else {
        cout << p << " exists, but is neither a regular file nor a directory\n";
      }
    } else {
      cout << p << " does not exist\n";
    }
    return 0;
  } else if (parameters.profiles_dir.compare("") == 0) {
    if (checkFile(parameters.profile_file)) {
      // read a single profile
      Structure structure;
      structure.load(parameters.profile_file);
      unit_coordinates = structure.getUnitCoordinates();
      return 1;
    } else {
      cout << "Profile " << parameters.profile_file << " does not exist ...\n";
      return 0;
    }
  }
}

/*!
 *  \brief This function models a single component.
 *  \param parameters a reference to a struct Parameters
 *  \param data a reference to a std::vector<Vector >
 */
void modelOneComponent(struct Parameters &parameters, std::vector<Vector > &data)
{
  cout << "Sample size: " << data.size() << endl;
  Kent kent;
  Vector weights(data.size(),1);
  kent.estimateParameters(data,weights);
}

/*!
 *  \brief This function models a mixture of several components.
 *  \param parameters a reference to a struct Parameters
 *  \param data a reference to a std::vector<std::vector<long double,3> >
 */
void modelMixture(struct Parameters &parameters, std::vector<Vector > &data)
{
  Vector data_weights(data.size(),1);
  // if the optimal number of components need to be determined
  /*if (parameters.infer_num_components == SET) {
    if (parameters.max_components == -1) {
      Mixture mixture;
      if (parameters.continue_inference == UNSET) {
        Mixture m(parameters.start_from,data,data_weights);
        mixture = m;
        mixture.estimateParameters();
      } else if (parameters.continue_inference == SET) {
        mixture.load(parameters.mixture_file,parameters.D,data,data_weights);
      } // continue_inference
      ofstream log(parameters.infer_log.c_str());
      Mixture stable = inferComponents(mixture,data.size(),log);
      NUM_STABLE_COMPONENTS = stable.getNumberOfComponents();
      log.close();
    } else {  // parameters.max_components == -1
      inferStableMixtures(data,parameters.min_components,parameters.max_components,
                          parameters.infer_log);
    }
  } else*/ if (parameters.infer_num_components == UNSET) {
    // for a given value of number of components
    // do the mixture modelling
    Mixture mixture(parameters.fit_num_components,data,data_weights);
    mixture.estimateParameters();
  }
}

/*!
 *  \brief This function is used to simulate the mixture model.
 *  \param parameters a reference to a struct Parameters
 */
void simulateMixtureModel(struct Parameters &parameters)
{
  std::vector<Vector > data;
  if (parameters.load_mixture == SET) {
    Mixture original;
    original.load(parameters.mixture_file);
    bool save = 1;
    if (parameters.read_profiles == SET) {
      bool success = gatherData(parameters,data);
      if (!success) {
        cout << "Error in reading data...\n";
        exit(1);
      }
    } else if (parameters.read_profiles == UNSET) {
      data = original.generate(parameters.sample_size,save);
    }
    if (parameters.heat_map == SET) {
      original.generateHeatmapData(parameters.res);
      std::vector<std::vector<int> > bins = updateBins(data,parameters.res);
      outputBins(bins,parameters.res);
    }
  } else if (parameters.load_mixture == UNSET) {
    int k = parameters.simulated_components;
    //srand(time(NULL));
    Vector weights = generateFromSimplex(k);
    std::vector<Kent> components = generateRandomComponents(k);
    Mixture original(k,components,weights);
    bool save = 1;
    data = original.generate(parameters.sample_size,save);
    // save the simulated mixture
    ofstream file("./simulation/simulated_mixture");
    for (int i=0; i<k; i++) {
      file << fixed << setw(10) << setprecision(5) << weights[i];
      file << "\t";
      components[i].printParameters(file);
    }
    file.close();
  }
  // model a mixture using the original data
  if (parameters.mixture_model == UNSET) {
    modelOneComponent(parameters,data);
  } else if (parameters.mixture_model == SET) {
    modelMixture(parameters,data);
  }
}

/*!
 *  \brief This function is used to generate random weights such that
 *  0 < w_i < 1 and \sum_i w_i = 1
 *  \param K an integer
 *  \return the list of weights
 */
Vector generateFromSimplex(int K)
{
  Vector values(K,0);
  long double random,sum = 0;
  for (int i=0; i<K; i++) {
    // generate a random value in (0,1)
    random = rand() / (long double)RAND_MAX;
    assert(random > 0 && random < 1);
    // sampling from an exponential distribution with \lambda = 1
    values[i] = -log(1-random);
    sum += values[i];
  }
  for (int i=0; i<K; i++) {
    values[i] /= sum;
  }
  return values;
}

/*!
 *  \brief This function is used to generate random components.
 *  \param num_components an integer
 *  \param D an integer
 *  \return the list of components
 */
std::vector<Kent> generateRandomComponents(int num_components)
{
  Vector mean(3,0),major_axis(3,0),minor_axis(3,0);

  // generate random kappas
  Vector kappas = generateRandomKappas(num_components);

  // generate random betas 
  Vector betas = generateRandomBetas(kappas);

  std::vector<Kent> components;
  for (int i=0; i<num_components; i++) {
    generateRandomOrthogonalVectors(mean,major_axis,minor_axis);
    // initialize component parameters
    Kent kent(mean,major_axis,minor_axis,kappas[i],betas[i]);
    components.push_back(kent);
  }
  return components;
}

/*!
 *  \brief This function generates random kappas 
 *  \param num_components an integer
 *  \return the list of random kappas 
 */
Vector generateRandomKappas(int K)
{
  Vector random_kappas;
  for (int i=0; i<K; i++) {
    long double kappa = (rand() / (long double) RAND_MAX) * MAX_KAPPA;
    random_kappas.push_back(kappa);
  }
  return random_kappas;
}

Vector generateRandomBetas(Vector &kappas)
{
  Vector random_betas;
  for (int i=0; i<kappas.size(); i++) {
    long double beta = (rand() / (long double) RAND_MAX) * (kappas[i]/2);
    random_betas.push_back(beta);
  }
  return random_betas;
}

////////////////////// TESTING FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\

void TestFunctions(void)
{
  Test test;

  //test.matrixFunctions();

  //test.productMatrixVector();

  //test.dispersionMatrix();

  //test.numericalIntegration();

  //test.normalDistributionFunctions();

  //test.orthogonalTransformations();

  //test.orthogonalTransformations2();

  //test.randomSampleGeneration();

  //test.normalization_constant();

  //test.optimization();

  //test.moment_estimation();

  //test.ml_estimation();

  //test.expectation();

  //test.kl_divergence();

  //test.fisher();

  test.mml_estimation();
}

////////////////////// EXPERIMENTS \\\\\\\\\\\\\\\\\\\\\\\\\\\\

void RunExperiments(int iterations)
{
  Experiments experiments(iterations);

  //experiments.plotBias(10,4.5);
  experiments.plotBias(10,3);
  //experiments.plotBias(10,3);
}

/*!
 *  \brief This function sorts the elements in the list
 *  \param list a reference to a vector<double>
 *  \return the sorted list
 */
Vector sort(Vector &list)
{
  int num_samples = list.size();
	Vector sortedList(list);
  std::vector<int> index(num_samples,0);
	for(int i=0; i<num_samples; i++) {
			index[i] = i;
  }
	quicksort(sortedList,index,0,num_samples-1);
  return sortedList;
}

/*!
 *  This is an implementation of the classic quicksort() algorithm to sort a
 *  list of data values. The module uses the overloading operator(<) to 
 *  compare two Point<T> objects. 
 *  Pivot is chosen as the right most element in the list(default)
 *  This function is called recursively.
 *  \param list a reference to a Vector
 *	\param index a reference to a std::vector<int>
 *  \param left an integer
 *  \param right an integer
 */
void quicksort(Vector &list, std::vector<int> &index, int left, int right)
{
	if(left < right)
	{
		int pivotNewIndex = partition(list,index,left,right);
		quicksort(list,index,left,pivotNewIndex-1);
		quicksort(list,index,pivotNewIndex+1,right);
	}
}

/*!
 *  This function is called from the quicksort() routine to compute the new
 *  pivot index.
 *  \param list a reference to a Vector
 *	\param index a reference to a std::vector<int>
 *  \param left an integer
 *  \param right an integer
 *  \return the new pivot index
 */
int partition(Vector &list, std::vector<int> &index, int left, int right)
{
	long double temp,pivotPoint = list[right];
	int storeIndex = left,temp_i;
	for(int i=left; i<right; i++) {
		if(list[i] < pivotPoint) {
			temp = list[i];
			list[i] = list[storeIndex];
			list[storeIndex] = temp;
			temp_i = index[i];
			index[i] = index[storeIndex];
			index[storeIndex] = temp_i;
			storeIndex += 1;	
		}
	}
	temp = list[storeIndex];
	list[storeIndex] = list[right];
	list[right] = temp;
	temp_i = index[storeIndex];
	index[storeIndex] = index[right];
	index[right] = temp_i;
	return storeIndex;
}

/*!
 *  \brief This module computes the median of a sorted set of samples
 *  \param list a reference to a std::vector<double>
 *  \return the median value
 */
long double computeMedian(Vector &list)
{
  Vector sorted_list = sort(list);
  int n = sorted_list.size();
  if (n % 2 == 1) {
    return sorted_list[n/2];
  } else {
    return (sorted_list[n/2-1]+sorted_list[n/2])/2;
  }
}

/*!
 *  \brief This module computes the mean of a set of samples
 *  \param list a reference to a std::vector<double>
 *  \return the mean value
 */
long double computeMean(Vector &list)
{
  long double sum = 0;
  for (int i=0; i<list.size(); i++) {
    sum += list[i];
  }
  return sum / (long double)list.size();
}

/*!
 *  \brief Computes the variance
 */
long double computeVariance(Vector &list)
{
  long double mean = computeMean(list);
  long double sum = 0;
  for (int i=0; i<list.size(); i++) {
    sum += (list[i]-mean) * (list[i]-mean);
  }
  return sum / (long double) (list.size()-1);
}

int maximumIndex(Vector &values)
{
  int max_index = 0;
  long double max_val = values[0];
  for (int i=1; i<values.size(); i++) { 
    if (values[i] > max_val) {
      max_index = i;
      max_val = values[i];
    }
  }
  return max_index;
}

