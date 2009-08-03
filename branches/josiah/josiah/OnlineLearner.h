#pragma once
#include <iostream>
#include <iomanip>
#include <fstream>
#ifdef MPI_ENABLED
#include <mpi.h>
#endif


#include "ScoreComponentCollection.h"

namespace Josiah {
  class TranslationDelta; 
  class Sampler;
  class WeightNormalizer;
  
  class OnlineLearner {
    public :
      OnlineLearner(const Moses::ScoreComponentCollection& initWeights, const std::string& name) : m_currWeights(initWeights), m_cumulWeights(initWeights), m_name(name), m_iteration(0) {} //, m_averaging(true)
      virtual void doUpdate(TranslationDelta* curr, TranslationDelta* target, TranslationDelta* noChangeDelta, Sampler& sampler)  = 0;
      void UpdateCumul() ;
      const Moses::ScoreComponentCollection& GetCurrWeights() { return m_currWeights; }
      Moses::ScoreComponentCollection GetAveragedWeights() ;
      virtual ~OnlineLearner() {}
      virtual void reset() {}
      virtual size_t GetNumUpdates() = 0;
      const std::string & GetName() {return m_name;}
#ifdef MPI_ENABLED    
      void SetRunningWeightVector(int, int);
#endif
    protected:
      //bool m_averaging;
      Moses::ScoreComponentCollection m_currWeights;
      Moses::ScoreComponentCollection m_cumulWeights;
      std::string m_name;
      size_t m_iteration;
      std::vector<float> hildreth ( const std::vector<Moses::ScoreComponentCollection>& a, const std::vector<float>& b );
      std::vector<float> hildreth ( const std::vector<Moses::ScoreComponentCollection>& a, const std::vector<float>& b, float );
  };

  class PerceptronLearner : public OnlineLearner {
    public :
      PerceptronLearner(const Moses::ScoreComponentCollection& initWeights, const std::string& name, float learning_rate = 1.0) : OnlineLearner(initWeights, name), m_learning_rate(learning_rate), m_numUpdates() {}
      virtual void doUpdate(TranslationDelta* curr, TranslationDelta* target, TranslationDelta* noChangeDelta, Sampler& sampler);
      virtual ~PerceptronLearner() {}
      virtual void reset() {m_numUpdates = 0;}
      virtual size_t GetNumUpdates() { return m_numUpdates;}
    private:
      float m_learning_rate;
      size_t m_numUpdates;
  };
  
  class MiraLearner : public OnlineLearner {
    public :
    MiraLearner(const Moses::ScoreComponentCollection& initWeights,  const std::string& name, bool fixMargin, float margin, float slack, WeightNormalizer* wn = NULL) : OnlineLearner(initWeights, name), m_numUpdates(), m_fixMargin(fixMargin), m_margin(margin), m_slack(slack), m_normalizer(wn) {}
      virtual void doUpdate(TranslationDelta* curr, TranslationDelta* target, TranslationDelta* noChangeDelta, Sampler& sampler);
      virtual ~MiraLearner() {}
      virtual void reset() {m_numUpdates = 0;}
      virtual size_t GetNumUpdates() { return m_numUpdates;} 
      void SetNormalizer(WeightNormalizer* normalizer) {m_normalizer = normalizer;}
    protected:
      size_t m_numUpdates;
      bool m_fixMargin;
      float m_margin;
      float m_slack;
      WeightNormalizer* m_normalizer;
  };
  
  class MiraPlusLearner : public MiraLearner {
    public :
      MiraPlusLearner(const Moses::ScoreComponentCollection& initWeights, const std::string& name, bool fixMargin, float margin, float slack, WeightNormalizer* wn = NULL) : MiraLearner(initWeights, name, fixMargin, margin, slack, wn) {}
      virtual void doUpdate(TranslationDelta* curr, TranslationDelta* target, TranslationDelta* noChangeDelta, Sampler& sampler);
      virtual ~MiraPlusLearner() {}
  };
  
  class WeightNormalizer {
    public :
      WeightNormalizer(float norm) {m_norm = norm;}
      virtual ~WeightNormalizer() {}
      virtual void Normalize(Moses::ScoreComponentCollection& ) = 0; 
    protected :
      float m_norm;
  };
  
  class L1Normalizer : public WeightNormalizer {
    public:
      L1Normalizer (float norm) : WeightNormalizer(norm) {}
      virtual ~L1Normalizer() {}
      virtual void Normalize(Moses::ScoreComponentCollection& weights) {
        float currNorm = weights.GetL1Norm();
        weights.MultiplyEquals(m_norm/currNorm);
      } 
  };
  
  class L2Normalizer : public WeightNormalizer {
    public:
      L2Normalizer (float norm) : WeightNormalizer(norm) {}
      virtual ~L2Normalizer() {}
      virtual void Normalize(Moses::ScoreComponentCollection& weights) {
        float currNorm = weights.GetL2Norm();
        weights.MultiplyEquals(m_norm/currNorm);
      } 
  };
}