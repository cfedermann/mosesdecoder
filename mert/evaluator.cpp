#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <getopt.h>
#include <math.h>

#include "Scorer.h"
#include "ScorerFactory.h"
#include "Timer.h"
#include "Util.h"

using namespace std;

namespace {

Scorer* g_scorer = NULL;
bool g_has_more_files = false;
bool g_has_more_scorers = false;
const float g_alpha = 0.05;


class EvaluatorUtil {
 public:
  static void evaluate(const string& candFile, int bootstrap);
  static float average(const vector<float>& list);
  static string int2string(int n);

 private:
  EvaluatorUtil() {}
  ~EvaluatorUtil() {}
};

void EvaluatorUtil::evaluate(const string& candFile, int bootstrap)
{
  ifstream cand(candFile.c_str());
  if (!cand.good()) throw runtime_error("Error opening candidate file");

  vector<ScoreStats> entries;

  // Loading sentences and preparing statistics
  ScoreStats scoreentry;
  string line;
  while (getline(cand, line))
  {
    g_scorer->prepareStats(entries.size(), line, scoreentry);
    entries.push_back(scoreentry);
  }

  int n = entries.size();
  if (bootstrap)
  {
    vector<float> scores;
    for (int i = 0; i < bootstrap; ++i)
    {
      // TODO: Use smart pointer for exceptional-safety.
      ScoreData* scoredata = new ScoreData(g_scorer);
      for (int j = 0; j < n; ++j)
      {
        int randomIndex = random() % n;
        string str_j = int2string(j);
        scoredata->add(entries[randomIndex], str_j);
      }
      g_scorer->setScoreData(scoredata);
      candidates_t candidates(n, 0);
      float score = g_scorer->score(candidates);
      scores.push_back(score);
      delete scoredata;
    }

    float avg = average(scores);

    sort(scores.begin(), scores.end());

    int lbIdx = scores.size() * (g_alpha / 2);
    int rbIdx = scores.size() * (1 - g_alpha / 2);

    float lb = scores[lbIdx];
    float rb = scores[rbIdx];

    if (g_has_more_files) cout << candFile << "\t";
    if (g_has_more_scorers) cout << g_scorer->getName() << "\t";

    cout.setf(ios::fixed, ios::floatfield);
    cout.precision(4);
    cout << avg << "\t[" << lb << "," << rb << "]" << endl;
  }
  else
  {
    // TODO: Use smart pointer for exceptional-safety.
    ScoreData* scoredata = new ScoreData(g_scorer);
    for (int sid = 0; sid < n; ++sid)
    {
      string str_sid = int2string(sid);
      scoredata->add(entries[sid], str_sid);
    }
    g_scorer->setScoreData(scoredata);
    candidates_t candidates(n, 0);
    float score = g_scorer->score(candidates);
    delete scoredata;

    if (g_has_more_files) cout << candFile << "\t";
    if (g_has_more_scorers) cout << g_scorer->getName() << "\t";

    cout.setf(ios::fixed, ios::floatfield);
    cout.precision(4);
    cout << score << endl;
  }
}

string EvaluatorUtil::int2string(int n)
{
  stringstream ss;
  ss << n;
  return ss.str();
}

float EvaluatorUtil::average(const vector<float>& list)
{
  float sum = 0;
  for (vector<float>::const_iterator it = list.begin(); it != list.end(); ++it)
    sum += *it;

  return sum / list.size();
}

void usage()
{
  cerr << "usage: evaluator [options] --reference ref1[,ref2[,ref3...]] --candidate cand1[,cand2[,cand3...]] " << endl;
  cerr << "[--sctype|-s] the scorer type (default BLEU)" << endl;
  cerr << "[--scconfig|-c] configuration string passed to scorer" << endl;
  cerr << "\tThis is of the form NAME1:VAL1,NAME2:VAL2 etc " << endl;
  cerr << "[--reference|-R] comma separated list of reference files" << endl;
  cerr << "[--candidate|-C] comma separated list of candidate files" << endl;
  cerr << "[--factors|-f] list of factors passed to the scorer (e.g. 0|2)" << endl;
  cerr << "[--filter|-l] filter command which will be used to preprocess the sentences" << endl;
  cerr << "[--bootstrap|-b] number of booststraped samples (default 0 - no bootstraping)" << endl;
  cerr << "[--rseed|-r] the random seed for bootstraping (defaults to system clock)" << endl;
  cerr << "[--help|-h] print this message and exit" << endl;
  cerr << endl;
  cerr << "Evaluator is able to compute more metrics at once. To do this," << endl;
  cerr << "specify more --sctype arguments. You can also specify more --scconfig strings." << endl;
  cerr << endl;
  cerr << "The example below prints BLEU score, PER score and interpolated" << endl;
  cerr << "score of CDER and PER with the given weights." << endl;
  cerr << endl;
  cerr << "./evaluator \\" << endl;
  cerr << "\t--sctype BLEU --scconfig reflen:closest \\" << endl;
  cerr << "\t--sctype PER \\" << endl;
  cerr << "\t--sctype CDER,PER --scconfig weights:0.25+0.75 \\" << endl;
  cerr << "\t--candidate CANDIDATE \\" << endl;
  cerr << "\t--reference REFERENCE" << endl;
  cerr << endl;
  cerr << "If you specify only one scorer and one candidate file, only the final score" << endl;
  cerr << "will be printed to stdout. Otherwise each line will contain metric name" << endl;
  cerr << "and/or filename and the final score. Since most of the metrics prints some" << endl;
  cerr << "debuging info, consider redirecting stderr to /dev/null." << endl;
  exit(1);
}

static struct option long_options[] = {
  {"sctype", required_argument, 0, 's'},
  {"scconfig", required_argument, 0, 'c'},
  {"reference", required_argument, 0, 'R'},
  {"candidate", required_argument, 0, 'C'},
  {"bootstrap", required_argument, 0, 'b'},
  {"rseed", required_argument, 0, 'r'},
  {"factors", required_argument, 0, 'f'},
  {"filter", required_argument, 0, 'l'},
  {"help", no_argument, 0, 'h'},
  {0, 0, 0, 0}
};

// Options used in evaluator.
struct ProgramOption {
  vector<string> scorer_types;
  vector<string> scorer_configs;
  string reference;
  string candidate;
  vector<string> scorer_factors;
  vector<string> scorer_filter;
  int bootstrap;
  int seed;
  bool has_seed;

  ProgramOption()
      : reference(""),
        candidate(""),
        bootstrap(0),
        seed(0),
        has_seed(false) { }
};

void ParseCommandOptions(int argc, char** argv, ProgramOption* opt) {
  int c;
  int option_index;
  int last_scorer_index = -1;
  while ((c = getopt_long(argc, argv, "s:c:R:C:b:r:f:l:h", long_options, &option_index)) != -1) {
    switch(c) {
      case 's':
        opt->scorer_types.push_back(string(optarg));
        opt->scorer_configs.push_back(string(""));
        opt->scorer_factors.push_back(string(""));
        opt->scorer_filter.push_back(string(""));
        last_scorer_index++;
        break;
      case 'c':
        opt->scorer_configs[last_scorer_index] = string(optarg);
        break;
      case 'R':
        opt->reference = string(optarg);
        break;
      case 'C':
        opt->candidate = string(optarg);
        break;
      case 'b':
        opt->bootstrap = atoi(optarg);
        break;
      case 'r':
        opt->seed = strtol(optarg, NULL, 10);
        opt->has_seed = true;
        break;
      case 'f':
        opt->scorer_factors[last_scorer_index] = string(optarg);
        break;
      case 'l':
        opt->scorer_filter[last_scorer_index] = string(optarg);
        break;
      default:
        usage();
    }
  }

  // Add default scorer if no scorer provided
  if (opt->scorer_types.size() == 0)
  {
    opt->scorer_types.push_back(string("BLEU"));
    opt->scorer_configs.push_back(string(""));
    opt->scorer_factors.push_back(string(""));
    opt->scorer_filter.push_back(string(""));
  }
}

void InitSeed(const ProgramOption *opt) {
  if (opt->has_seed) {
    cerr << "Seeding random numbers with " << opt->seed << endl;
    srandom(opt->seed);
  } else {
    cerr << "Seeding random numbers with system clock " << endl;
    srandom(time(NULL));
  }
}

} // anonymous namespace

int main(int argc, char** argv)
{
  ResetUserTime();

  ProgramOption option;
  ParseCommandOptions(argc, argv, &option);

  if (option.bootstrap)
  {
    InitSeed(&option);
  }

  try {
    vector<string> refFiles;
    vector<string> candFiles;

    if (option.reference.length() == 0) throw runtime_error("You have to specify at least one reference file.");
    split(option.reference, ',', refFiles);

    if (option.candidate.length() == 0) throw runtime_error("You have to specify at least one candidate file.");
    split(option.candidate, ',', candFiles);

    if (candFiles.size() > 1) g_has_more_files = true;
    if (option.scorer_types.size() > 1) g_has_more_scorers = true;

    for (vector<string>::const_iterator fileIt = candFiles.begin(); fileIt != candFiles.end(); ++fileIt)
    {
        for (size_t i = 0; i < option.scorer_types.size(); i++)
        {
            g_scorer = ScorerFactory::getScorer(option.scorer_types[i], option.scorer_configs[i]);
            g_scorer->setFactors(option.scorer_factors[i]);
            g_scorer->setFilter(option.scorer_filter[i]);
            g_scorer->setReferenceFiles(refFiles);
            EvaluatorUtil::evaluate(*fileIt, option.bootstrap);
            delete g_scorer;
        }
    }
    return EXIT_SUCCESS;
  } catch (const exception& e) {
    cerr << "Exception: " << e.what() << endl;
    return EXIT_FAILURE;
  }
}
