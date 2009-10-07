// $Id: TargetPhrase.cpp 98 2007-09-17 21:02:40Z hieu $

/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2006 University of Edinburgh

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
***********************************************************************/

#include <cassert>
#include "TargetPhrase.h"
#include "PhraseDictionaryMemory.h"
#include "GenerationDictionary.h"
#include "LanguageModel.h"
#include "StaticData.h"
#include "LMList.h"
#include "ScoreComponentCollection.h"
#include "DecodeStepTranslation.h"
#include "DummyScoreProducers.h"

using namespace std;

TargetPhrase::TargetPhrase(FactorDirection direction)
	:Phrase(direction),m_transScore(0.0), m_ngramScore(0.0), m_fullScore(0.0), m_sourcePhrase(0)
{}

void TargetPhrase::SetScore(size_t decodeStepId)
{ // used when creating translations of unknown words:
	m_transScore = m_ngramScore = 0;
	const DecodeStepTranslation &step = static_cast<const DecodeStepTranslation&>(StaticData::Instance().GetDecodeStep(decodeStepId));
	m_fullScore = - step.GetWordPenaltyProducer().GetWPWeight();
	
}

void TargetPhrase::SetAlignment()
{
	m_alignmentPair.SetIdentityAlignment();
}

void TargetPhrase::SetScore(const ScoreProducer* translationScoreProducer,
														const vector<float> &scoreVector, const vector<float> &weightT,
														float weightWP, const LMList &languageModels)
{
	assert(weightT.size() == scoreVector.size());
	// calc average score if non-best

	m_transScore = std::inner_product(scoreVector.begin(), scoreVector.end(), weightT.begin(), 0.0f);
	m_scoreBreakdown.PlusEquals(translationScoreProducer, scoreVector);

  // Replicated from TranslationOptions.cpp
	float totalFutureScore = 0;
	float totalNgramScore  = 0;
	float totalFullScore   = 0;

	LMList::const_iterator lmIter;
	for (lmIter = languageModels.begin(); lmIter != languageModels.end(); ++lmIter)
	{
		const LanguageModel &lm = **lmIter;
		
		if (lm.Useable(*this))
		{ // contains factors used by this LM
			const float weightLM = lm.GetWeight();
			float fullScore, nGramScore;

			lm.CalcScore(*this, fullScore, nGramScore);
			m_scoreBreakdown.Assign(&lm, nGramScore);

			// total LM score so far
			totalNgramScore  += nGramScore * weightLM;
			totalFullScore   += fullScore * weightLM;
			
		}
	}
  m_ngramScore = totalNgramScore;

	m_fullScore = m_transScore + totalFutureScore + totalFullScore
							- (this->GetSize() * weightWP);	 // word penalty
}

void TargetPhrase::SetWeights(const ScoreProducer* translationScoreProducer, const vector<float> &weightT)
{
	// calling this function in case of confusion net input is undefined
	assert(StaticData::Instance().GetInputType()==0); 
	
	/* one way to fix this, you have to make sure the weightT contains (in 
     addition to the usual phrase translation scaling factors) the input 
     weight factor as last element
	*/

	m_transScore = m_scoreBreakdown.PartialInnerProduct(translationScoreProducer, weightT);
}

void TargetPhrase::ResetScore()
{
	m_transScore = m_fullScore = m_ngramScore = 0;
	m_scoreBreakdown.ZeroAll();
}

TargetPhrase *TargetPhrase::MergeNext(const TargetPhrase &inputPhrase) const
{
	if (! IsCompatible(inputPhrase))
	{
		return NULL;
	}

	// ok, merge
	TargetPhrase *clone				= new TargetPhrase(*this);

	int currWord = 0;
	const size_t len = GetSize();
	for (size_t currPos = 0 ; currPos < len ; currPos++)
	{
		const Word &inputWord	= inputPhrase.GetWord(currPos);
		Word &cloneWord = clone->GetWord(currPos);
		cloneWord.Merge(inputWord);
		
		currWord++;
	}

	return clone;
}

// helper functions
void AddAlignmentElement(AlignmentPhraseInserter &inserter
												 , const string &str
												 , size_t phraseSize
												 , size_t otherPhraseSize
												 , list<size_t> &uniformAlignment)
{
	// input
	vector<string> alignPhraseVector;
	alignPhraseVector = Tokenize(str);
	// "(0) (3) (1,2)"
	//		to
	// "(0)" "(3)" "(1,2)"
	assert (alignPhraseVector.size() == phraseSize) ;

	const size_t inputSize = alignPhraseVector.size();
	for (size_t pos = 0 ; pos < inputSize ; ++pos)
	{
		string alignElementStr = alignPhraseVector[pos];
		alignElementStr = alignElementStr.substr(1, alignElementStr.size() - 2);
		AlignmentElement *alignElement = new AlignmentElement(Tokenize<size_t>(alignElementStr, ","));
		// "(1,2)"
		//  to
		// [1] [2]
		if (alignElement->GetSize() == 0)
		{ // no alignment info. add uniform alignment, ie. can be aligned to any word
			alignElement->SetUniformAlignment(otherPhraseSize);
			uniformAlignment.push_back(pos);
		}

		**inserter = alignElement;
		(*inserter)++;		
	}
}

void TargetPhrase::CreateAlignmentInfo(const string &sourceStr
																			 , const string &targetStr
																			 , size_t sourceSize)
{
	AlignmentPhraseInserter sourceInserter = m_alignmentPair.GetInserter(Input)
													,targetInserter = m_alignmentPair.GetInserter(Output);
	list<size_t> uniformAlignmentSource
							,uniformAlignmentTarget;
	AddAlignmentElement(sourceInserter
										, sourceStr
										, sourceSize
										, GetSize()
										, uniformAlignmentSource);
	AddAlignmentElement(targetInserter
										, targetStr
										, GetSize()
										, sourceSize
										, uniformAlignmentTarget);
	// propergate uniform alignments to other side
	m_alignmentPair.GetAlignmentPhrase(Output).AddUniformAlignmentElement(uniformAlignmentSource);
	m_alignmentPair.GetAlignmentPhrase(Input).AddUniformAlignmentElement(uniformAlignmentTarget);
}


TO_STRING_BODY(TargetPhrase);

std::ostream& operator<<(std::ostream& os, const TargetPhrase& tp)
{
  os	<< static_cast<const Phrase&>(tp) 
			<< ", " << tp.GetAlignmentPair()
			<< ", pC=" << tp.m_transScore << ", c=" << tp.m_fullScore;
  return os;
}