//
// hmm.cpp
//
// Hidden Markov Model: Possibly Multimodal and/or submodel of a hierarchical model
//
// Copyright (C) 2014 Ircam - Jules Francoise. All Rights Reserved.
// author: Jules Francoise <jules.francoise@ircam.fr>
// 

#include <cmath>
#include "hmm.h"

using namespace std;

#pragma mark -
#pragma mark Constructors
HMM::HMM(rtml_flags flags,
         TrainingSet *trainingSet,
         int nbStates,
         int nbMixtureComponents)
: EMBasedModel(flags, trainingSet)
{
    is_hierarchical_ = !(flags & HIERARCHICAL);
    
    nbStates_ = nbStates;
    nbMixtureComponents_ = nbMixtureComponents;
    covarianceOffset_ = GAUSSIAN_DEFAULT_COVARIANCE_OFFSET;
    
    allocate();
    
    for (int i=0; i<nbStates; i++) {
        states_[i].set_trainingSet(trainingSet);
    }
    
    play_EM_stopCriterion_.minSteps = PLAY_EM_STEPS;
    play_EM_stopCriterion_.maxSteps = 0;
    play_EM_stopCriterion_.percentChg = PLAY_EM_MAX_LOG_LIK_PERCENT_CHG;
    
    transitionMode_ = LEFT_RIGHT;
    estimateMeans_ = HMM_DEFAULT_ESTIMATEMEANS;
    
    initTraining();
}

HMM::HMM(HMM const& src) : EMBasedModel(src)
{
    _copy(this, src);
}

HMM& HMM::operator=(HMM const& src)
{
    if(this != &src)
    {
        _copy(this, src);
    }
    return *this;
}

void HMM::_copy(HMM *dst, HMM const& src)
{
    EMBasedModel::_copy(dst, src);
    dst->nbMixtureComponents_     = src.nbMixtureComponents_;
    dst->covarianceOffset_        = src.covarianceOffset_;
    dst->nbStates_ = src.nbStates_;
    dst->estimateMeans_ = src.estimateMeans_;
    
    dst->alpha.resize(dst->nbStates_);
    dst->previousAlpha_.resize(dst->nbStates_);
    dst->beta_.resize(dst->nbStates_);
    dst->previousBeta_.resize(dst->nbStates_);
    
    dst->transition_ = src.transition_;
    dst->prior_ = src.prior_;
    dst->transitionMode_ = src.transitionMode_;
    
    dst->states_ = src.states_;
    dst->play_EM_stopCriterion_ = src.play_EM_stopCriterion_;
}


HMM::~HMM()
{
    prior_.clear();
    transition_.clear();
    alpha.clear();
    previousAlpha_.clear();
    beta_.clear();
    previousBeta_.clear();
    states_.clear();
    if (is_hierarchical_) {
        for (int i=0 ; i<3 ; i++)
            this->alpha_h[i].clear();
        exitProbabilities_.clear();
    }
}

#pragma mark -
#pragma mark Parameters initialization


void HMM::allocate()
{
    prior_.resize(nbStates_);
    transition_.resize(nbStates_*nbStates_);
    alpha.resize(nbStates_);
    previousAlpha_.resize(nbStates_);
    beta_.resize(nbStates_);
    previousBeta_.resize(nbStates_);
    states_.assign(nbStates_, GMM(flags_, this->trainingSet, nbMixtureComponents_, covarianceOffset_));
    if (is_hierarchical_)
        updateExitProbabilities(NULL);
}


void HMM::evaluateNbStates(int factor)
{
    if (!this->trainingSet || this->trainingSet->is_empty()) return;
    this->set_nbStates(((*this->trainingSet)(0))->second->length() / factor);
}


void HMM::initParametersToDefault()
{
    for (int i=0; i<nbStates_; i++) {
        states_[i].initParametersToDefault();
    }
}


void HMM::initMeansWithFirstPhrase()
{
    if (!this->trainingSet || this->trainingSet->is_empty()) return;
    
    for (int n=0; n<nbStates_; n++)
        for (int d=0; d<dimension_; d++)
            states_[n].components[0].mean[d] = 0.0;
    
    vector<int> factor(nbStates_, 0);
    int step = ((*this->trainingSet)(0))->second->length() / nbStates_;
    int offset(0);
    for (int n=0; n<nbStates_; n++) {
        for (int t=0; t<step; t++) {
            for (int d=0; d<dimension_; d++) {
                states_[n].components[0].mean[d] += (*((*this->trainingSet)(0)->second))(offset+t, d);
            }
        }
        offset += step;
        factor[n] += step;
    }
    
    for (int n=0; n<nbStates_; n++)
        for (int d=0; d<dimension_; d++)
            states_[n].components[0].mean[d] /= factor[n];
}


void HMM::initMeansWithAllPhrases_single()
{
    if (!this->trainingSet || this->trainingSet->is_empty()) return;
    int nbPhrases = this->trainingSet->size();
    
    for (int n=0; n<nbStates_; n++)
        for (int d=0; d<dimension_; d++)
            states_[n].components[0].mean[d] = 0.0;
    
    vector<int> factor(nbStates_, 0);
    for (int i=0; i<nbPhrases; i++) {
        int step = ((*this->trainingSet)(i))->second->length() / nbStates_;
        int offset(0);
        for (int n=0; n<nbStates_; n++) {
            for (int t=0; t<step; t++) {
                for (int d=0; d<dimension_; d++) {
                    states_[n].components[0].mean[d] += (*((*this->trainingSet)(i)->second))(offset+t, d);
                }
            }
            offset += step;
            factor[n] += step;
        }
    }
    
    for (int n=0; n<nbStates_; n++)
        for (int d=0; d<dimension_; d++)
            states_[n].components[0].mean[d] /= factor[n];
}


void HMM::initCovariancesWithAllPhrases_single()
{
    if (!this->trainingSet || this->trainingSet->is_empty()) return;
    int nbPhrases = this->trainingSet->size();
    
    for (int n=0; n<nbStates_; n++)
        for (int d1=0; d1<dimension_; d1++)
            for (int d2=0; d2<dimension_; d2++)
                states_[n].components[0].covariance[d1*dimension_+d2] = -states_[n].components[0].mean[d1]*states_[n].components[0].mean[d2];
    
    vector<int> factor(nbStates_, 0);
    for (int i=0; i<nbPhrases; i++) {
        int step = ((*this->trainingSet)(i))->second->length() / nbStates_;
        int offset(0);
        for (int n=0; n<nbStates_; n++) {
            for (int t=0; t<step; t++) {
                for (int d1=0; d1<dimension_; d1++) {
                    for (int d2=0; d2<dimension_; d2++) {
                        states_[n].components[0].covariance[d1*dimension_+d2] += (*((*this->trainingSet)(i)->second))(offset+t, d1) * (*((*this->trainingSet)(i)->second))(offset+t, d2);
                    }
                }
            }
            offset += step;
            factor[n] += step;
        }
    }
    
    for (int n=0; n<nbStates_; n++)
        for (int d1=0; d1<dimension_; d1++)
            for (int d2=0; d2<dimension_; d2++)
                states_[n].components[0].covariance[d1*dimension_+d2] /= factor[n];
}


void HMM::initMeansWithAllPhrases_mixture()
{
    if (!this->trainingSet || this->trainingSet->is_empty()) return;
    int nbPhrases = this->trainingSet->size();
    
    for (int i=0; i<min(nbPhrases, nbMixtureComponents_); i++) {
        int step = ((*this->trainingSet)(i))->second->length() / nbStates_;
        int offset(0);
        for (int n=0; n<nbStates_; n++) {
            for (int d=0; d<dimension_; d++) {
                states_[n].components[i].mean[d] = 0.0;
            }
            for (int t=0; t<step; t++) {
                for (int d=0; d<dimension_; d++) {
                    states_[n].components[i].mean[d] += (*((*this->trainingSet)(i)->second))(offset+t, d) / float(step);
                }
            }
            offset += step;
        }
    }
}


void HMM::initCovariancesWithAllPhrases_mixture()
{
    if (!this->trainingSet || this->trainingSet->is_empty()) return;
    int nbPhrases = this->trainingSet->size();
    
    for (int i=0; i<min(nbPhrases, nbMixtureComponents_); i++) {
        int step = ((*this->trainingSet)(i))->second->length() / nbStates_;
        int offset(0);
        for (int n=0; n<nbStates_; n++) {
            for (int d1=0; d1<dimension_; d1++) {
                for (int d2=0; d2<dimension_; d2++) {
                    states_[n].components[i].covariance[d1*dimension_+d2] = -states_[n].components[i].mean[d1]*states_[n].components[i].mean[d2];
                }
            }
            for (int t=0; t<step; t++) {
                for (int d1=0; d1<dimension_; d1++) {
                    for (int d2=0; d2<dimension_; d2++) {
                        states_[n].components[i].covariance[d1*dimension_+d2] += (*((*this->trainingSet)(i)->second))(offset+t, d1) * (*((*this->trainingSet)(i)->second))(offset+t, d2) / float(step);
                    }
                }
            }
            offset += step;
        }
    }
}


void HMM::setErgodic()
{
    for (int i=0 ; i<nbStates_; i++) {
        prior_[i] = 1/(float)nbStates_;
        for (int j=0; j<nbStates_; j++) {
            transition_[i*nbStates_+j] = 1/(float)nbStates_;
        }
    }
}


void HMM::setLeftRight()
{
    for (int i=0 ; i<nbStates_; i++) {
        prior_[i] = 0.;
        for (int j=0; j<nbStates_; j++) {
            transition_[i*nbStates_+j] = ((i == j) || ((i+1) == j)) ? 0.5 : 0;
        }
    }
    transition_[nbStates_*nbStates_-1] = 1.;
    prior_[0] = 1.;
}


void HMM::normalizeTransitions()
{
    double norm_prior(0.), norm_transition;
    for (int i=0; i<nbStates_; i++) {
        norm_prior += prior_[i];
        norm_transition = 0.;
        for (int j=0; j<nbStates_; j++)
            norm_transition += transition_[i*nbStates_+j];
        for (int j=0; j<nbStates_; j++)
            transition_[i*nbStates_+j] /= norm_transition;
    }
    for (int i=0; i<nbStates_; i++)
        prior_[i] /= norm_prior;
}

#pragma mark -
#pragma mark Accessors


int HMM::get_nbStates() const
{
    return nbStates_;
}


void HMM::set_nbStates(int nbStates)
{
    if (nbStates < 1) throw invalid_argument("Number of states must be > 0");;
    if (nbStates == nbStates_) return;
    
    nbStates_ = nbStates;
    allocate();
    
    this->trained = false;
}


int HMM::get_nbMixtureComponents() const
{
    return nbMixtureComponents_;
}


void HMM::set_nbMixtureComponents(int nbMixtureComponents)
{
    if (nbMixtureComponents < 1) throw invalid_argument("The number of Gaussian mixture components must be > 0");;
    if (nbMixtureComponents == nbMixtureComponents_) return;
    
    for (int i=0; i<nbStates_; i++) {
        states_[i].set_nbMixtureComponents(nbMixtureComponents);
    }
    
    nbMixtureComponents_ = nbMixtureComponents;
    
    this->trained = false;
}


float  HMM::get_covarianceOffset() const
{
    return covarianceOffset_;
}


void HMM::set_covarianceOffset(float covarianceOffset)
{
    if (covarianceOffset == covarianceOffset_) return;
    
    for (int i=0; i<nbStates_; i++) {
        states_[i].set_covarianceOffset(covarianceOffset);
    }
    covarianceOffset_ = covarianceOffset;
}


string HMM::get_transitionMode() const
{
    if (transitionMode_ == ERGODIC) {
        return "ergodic";
    } else {
        return "left-right";
    }
}


void HMM::set_transitionMode(string transMode_str)
{
    if (!transMode_str.compare("ergodic")) {
        transitionMode_ = ERGODIC;
    } else if (!transMode_str.compare("left-right")) {
        transitionMode_ = LEFT_RIGHT;
    } else {
        throw invalid_argument("Wrong Transition mode. choose 'ergodic' or 'left-right'");
    }
}

#pragma mark -
#pragma mark Observation probabilities


double HMM::obsProb(const float *observation, unsigned int stateIndex, int mixtureComponent)
{
    if (stateIndex >= nbStates_)
        throw out_of_range("State index is out of bounds");
    return states_[stateIndex].obsProb(observation, mixtureComponent);
}


double HMM::obsProb_input(const float *observation_input, unsigned int stateIndex, int mixtureComponent)
{
    if (!bimodal_)
        throw runtime_error("Model is not bimodal. Use the function 'obsProb'");
    if (stateIndex >= nbStates_)
        throw out_of_range("State index is out of bounds");
    return states_[stateIndex].obsProb_input(observation_input, mixtureComponent);
}


double HMM::obsProb_bimodal(const float *observation_input, const float *observation_output, unsigned int stateIndex, int mixtureComponent)
{
    if (!bimodal_)
        throw runtime_error("Model is not bimodal. Use the function 'obsProb'");
    if (stateIndex >= nbStates_)
        throw out_of_range("State index is out of bounds");
    return states_[stateIndex].obsProb_bimodal(observation_input, observation_output, mixtureComponent);
}

#pragma mark -
#pragma mark Forward-Backward algorithm


double HMM::forward_init(const float *observation, const float *observation_output)
{
    double norm_const(0.);
    for (int i=0 ; i<nbStates_ ; i++) {
        if (bimodal_) {
            if (observation_output)
                alpha[i] = prior_[i] * obsProb_bimodal(observation, observation_output, i);
            else
                alpha[i] = prior_[i] * obsProb_input(observation, i);
        } else {
            alpha[i] = prior_[i] * obsProb(observation, i);
        }
        norm_const += alpha[i];
    }
    if (norm_const > 0) {
        for (int i=0 ; i<nbStates_ ; i++) {
            alpha[i] /= norm_const;
        }
        return 1/norm_const;
    } else {
        for (int j=0; j<nbStates_; j++) {
            alpha[j] = 1./double(nbStates_);
        }
        return 1.;
    }
}


double HMM::forward_update(const float *observation, const float *observation_output)
{
    double norm_const(0.);
    previousAlpha_ = alpha;
    for (int j=0; j<nbStates_; j++) {
        alpha[j] = 0.;
        for (int i=0; i<nbStates_; i++) {
            alpha[j] += previousAlpha_[i] * transition_[i*nbStates_+j];
        }
        if (bimodal_) {
            if (observation_output)
                alpha[j] *= obsProb_bimodal(observation, observation_output, j);
            else
                alpha[j] *= obsProb_input(observation, j);
        } else {
            alpha[j] *= obsProb(observation, j);
        }
        norm_const += alpha[j];
    }
    if (norm_const > 0) {
        for (int j=0; j<nbStates_; j++) {
            alpha[j] /= norm_const;
        }
        return 1./norm_const;
    } else {
        for (int j=0; j<nbStates_; j++) {
            alpha[j] = 1./double(nbStates_);
        }
        return 1.;
    }
}


double HMM::forward_update_withNewObservation(const float *observation, const float *observation_output)
{
    if (forwardInitialized_) {
        double norm_const(0.);
        for (int j=0; j<nbStates_; j++) {
            alpha[j] = 0.;
            for (int i=0; i<nbStates_; i++) {
                alpha[j] += previousAlpha_[i] * transition_[i*nbStates_+j];
            }
            alpha[j] *= obsProb_bimodal(observation, observation_output, j);
            norm_const += alpha[j];
        }
        if (norm_const > 0) {
            for (int j=0; j<nbStates_; j++) {
                alpha[j] /= norm_const;
            }
            return 1./norm_const;
        } else {
            for (int j=0; j<nbStates_; j++) {
                alpha[j] = 1./double(nbStates_);
            }
            return 1.;
        }
    } else {
        return forward_init(observation, observation_output);
    }
}


void HMM::backward_init(double ct)
{
    for (int i=0 ; i<nbStates_ ; i++)
        beta_[i] = ct;
}


void HMM::backward_update(double ct, const float *observation, const float *observation_output)
{
    previousBeta_ = beta_;
    for (int i=0 ; i<nbStates_; i++) {
        beta_[i] = 0.;
        for (int j=0; j<nbStates_; j++) {
            if (bimodal_) {
                if (observation_output)
                    beta_[i] += transition_[i*nbStates_+j] * previousBeta_[j] * obsProb_bimodal(observation, observation_output, j);
                else
                    beta_[i] += transition_[i*nbStates_+j] * previousBeta_[j] * obsProb_input(observation, j);
            } else {
                beta_[i] += transition_[i*nbStates_+j] * previousBeta_[j] * obsProb(observation, j);
            }
            
        }
        beta_[i] *= ct;
        if (isnan(beta_[i]) || isinf(abs(beta_[i]))) {
            beta_[i] = 1e100;
        }
    }
}

#pragma mark -
#pragma mark Training algorithm


void HMM::initTraining()
{
    // Initialize Model Parameters
    // ---------------------------------------
    if (transitionMode_ == ERGODIC) {
        setErgodic();
    } else {
        setLeftRight();
    }
    for (int i=0; i<nbStates_; i++) {
        states_[i].initTraining();
    }
    
    if (!this->trainingSet) return;
    
    if (nbMixtureComponents_ > 1) {
        initMeansWithAllPhrases_mixture();
        initCovariancesWithAllPhrases_mixture();
    } else {
        // initMeansWithAllPhrases_single();
        initMeansWithFirstPhrase();
        initCovariancesWithAllPhrases_single();
    }
    this->trained = false;
    
    int nbPhrases = this->trainingSet->size();
    
    
    // Initialize Algorithm variables
    // ---------------------------------------
    gammaSequence_.resize(nbPhrases);
    epsilonSequence_.resize(nbPhrases);
    gammaSequencePerMixture_.resize(nbPhrases);
    int maxT(0);
    int i(0);
    for (phrase_iterator it = this->trainingSet->begin(); it != this->trainingSet->end(); it++) {
        int T = it->second->length();
        gammaSequence_[i].resize(T*nbStates_);
        epsilonSequence_[i].resize(T*nbStates_*nbStates_);
        gammaSequencePerMixture_[i].resize(nbMixtureComponents_);
        for (int c=0; c<nbMixtureComponents_; c++) {
            gammaSequencePerMixture_[i][c].resize(T*nbStates_);
        }
        if (T>maxT) {
            maxT = T;
        }
        i++;
    }
    alpha_seq_.resize(maxT*nbStates_);
    beta_seq_.resize(maxT*nbStates_);
    
    gammaSum_.resize(nbStates_);
    gammaSumPerMixture_.resize(nbStates_*nbMixtureComponents_);
}


void HMM::finishTraining()
{
    normalizeTransitions();
    BaseModel::finishTraining();
}


double HMM::train_EM_update()
{
    return baumWelch_update();
}


double HMM::baumWelch_update()
{
    double log_prob(0.);
    
    // Forward-backward for each phrase
    // =================================================
    int phraseIndex(0);
    for (phrase_iterator it = this->trainingSet->begin(); it != this->trainingSet->end(); it++) {
        log_prob += baumWelch_forwardBackward(it->second, phraseIndex++);
    }
    
    baumWelch_gammaSum();
    
    // Re-estimate model parameters
    // =================================================
    
    // set covariance and mixture coefficients to zero for each state
    for (int i=0; i<nbStates_; i++) {
        states_[i].setParametersToZero();
    }
    
    baumWelch_estimateMixtureCoefficients();
    if (estimateMeans_)
        baumWelch_estimateMeans();
    
    baumWelch_estimateCovariances();
    if (transitionMode_ == ERGODIC)
        baumWelch_estimatePrior();
    baumWelch_estimateTransitions();
    
    return log_prob;
}


double HMM::baumWelch_forwardBackward(Phrase* currentPhrase, int phraseIndex)
{
    int T = currentPhrase->length();
    
    vector<double> ct(T);
    vector<double>::iterator alpha_seq_it = alpha_seq_.begin();
    
    double log_prob;
    
    // Forward algorithm
    if (bimodal_) {
        ct[0] = forward_init(currentPhrase->get_dataPointer_input(0),
                             currentPhrase->get_dataPointer_output(0));
    } else {
        ct[0] = forward_init(currentPhrase->get_dataPointer(0));
    }
    log_prob = -log(ct[0]);
    copy(alpha.begin(), alpha.end(), alpha_seq_it);
    alpha_seq_it += nbStates_;
    
    for (int t=1; t<T; t++) {
        if (bimodal_) {
            ct[t] = forward_update(currentPhrase->get_dataPointer_input(t),
                                   currentPhrase->get_dataPointer_output(t));
        } else {
            ct[t] = forward_update(currentPhrase->get_dataPointer(t));
        }
        log_prob -= log(ct[t]);
        copy(alpha.begin(), alpha.end(), alpha_seq_it);
        alpha_seq_it += nbStates_;
    }
    
    // Backward algorithm
    backward_init(ct[T-1]);
    vector<double>::iterator beta_seq_it = beta_seq_.begin()+(T-1)*nbStates_;
    copy(beta_.begin(), beta_.end(), beta_seq_it);
    beta_seq_it -= nbStates_;
    
    for (int t=T-2; t>=0; t--) {
        if (bimodal_) {
            backward_update(ct[t],
                            currentPhrase->get_dataPointer_input(t+1),
                            currentPhrase->get_dataPointer_output(t+1));
        } else {
            backward_update(ct[t], currentPhrase->get_dataPointer(t+1));
        }
        copy(beta_.begin(), beta_.end(), beta_seq_it);
        beta_seq_it -= nbStates_;
    }
    
    // Compute Gamma Variable
    for (int t=0; t<T; t++) {
        for (int i=0; i<nbStates_; i++) {
            gammaSequence_[phraseIndex][t*nbStates_+i] = alpha_seq_[t*nbStates_+i] * beta_seq_[t*nbStates_+i] / ct[t];
        }
    }
    
    // Compute Gamma variable for each mixture component
    double oo;
    double norm_const;
    
    for (int t=0; t<T; t++) {
        for (int i=0; i<nbStates_; i++) {
            norm_const = 0.;
            for (int c=0; c<nbMixtureComponents_; c++) {
                if (bimodal_) {
                    oo = obsProb_bimodal(currentPhrase->get_dataPointer_input(t),
                                         currentPhrase->get_dataPointer_output(t),
                                         i,
                                         c);
                } else {
                    oo = obsProb(currentPhrase->get_dataPointer(t),
                                 i,
                                 c);
                }
                gammaSequencePerMixture_[phraseIndex][c][t*nbStates_+i] = gammaSequence_[phraseIndex][t*nbStates_+i] * oo;
                norm_const += oo;
            }
            if (norm_const > 0)
                for (int c=0; c<nbMixtureComponents_; c++)
                    gammaSequencePerMixture_[phraseIndex][c][t*nbStates_+i] /= norm_const;
        }
    }
    
    // Compute Epsilon Variable
    for (int t=0; t<T-1; t++) {
        for (int i=0; i<nbStates_; i++) {
            for (int j=0; j<nbStates_; j++) {
                epsilonSequence_[phraseIndex][t*nbStates_*nbStates_+i*nbStates_+j] = alpha_seq_[t*nbStates_+i]
                * transition_[i*nbStates_+j]
                * beta_seq_[(t+1)*nbStates_+j];
                if (bimodal_) {
                    epsilonSequence_[phraseIndex][t*nbStates_*nbStates_+i*nbStates_+j] *= obsProb_bimodal(currentPhrase->get_dataPointer_input(t+1),
                                                                                                      currentPhrase->get_dataPointer_output(t+1),
                                                                                                      j);
                } else {
                    epsilonSequence_[phraseIndex][t*nbStates_*nbStates_+i*nbStates_+j] *= obsProb(currentPhrase->get_dataPointer(t+1), j);
                }
            }
        }
    }
    
    return log_prob;
}

void HMM::baumWelch_gammaSum()
{
    for (int i=0; i<nbStates_; i++) {
        gammaSum_[i] = 0.;
        for (int c=0; c<nbMixtureComponents_; c++) {
            gammaSumPerMixture_[i*nbMixtureComponents_+c] = 0.;
        }
    }
    
    int phraseLength;
    int phraseIndex(0);
    for (phrase_iterator it = this->trainingSet->begin(); it != this->trainingSet->end(); it++) {
        phraseLength = it->second->length();
        for (int i=0; i<nbStates_; i++) {
            for (int t=0; t<phraseLength; t++) {
                gammaSum_[i] += gammaSequence_[phraseIndex][t*nbStates_+i];
                for (int c=0; c<nbMixtureComponents_; c++) {
                    gammaSumPerMixture_[i*nbMixtureComponents_+c] += gammaSequencePerMixture_[phraseIndex][c][t*nbStates_+i];
                }
            }
        }
        phraseIndex++;
    }
}


void HMM::baumWelch_estimateMixtureCoefficients()
{
    int phraseLength;
    int phraseIndex(0);
    for (phrase_iterator it = this->trainingSet->begin(); it != this->trainingSet->end(); it++)
    {
        phraseLength = it->second->length();
        for (int i=0; i<nbStates_; i++) {
            for (int t=0; t<phraseLength; t++) {
                for (int c=0; c<nbMixtureComponents_; c++) {
                    states_[i].mixtureCoeffs[c] += gammaSequencePerMixture_[phraseIndex][c][t*nbStates_+i];
                }
            }
        }
        phraseIndex++;
    }
    
    // Scale mixture coefficients
    for (int i=0; i<nbStates_; i++) {
        states_[i].normalizeMixtureCoeffs();
    }
}


void HMM::baumWelch_estimateMeans()
{
    int phraseLength;
    // Re-estimate Mean
    int phraseIndex(0);
    for (phrase_iterator it = this->trainingSet->begin(); it != this->trainingSet->end(); it++)
    {
        phraseLength = it->second->length();
        for (int i=0; i<nbStates_; i++) {
            for (int c=0; c<nbMixtureComponents_; c++) {
                for (int d=0; d<dimension_; d++) {
                    states_[i].components[c].mean[d] = 0.0;
                }
            }
            for (int t=0; t<phraseLength; t++) {
                for (int c=0; c<nbMixtureComponents_; c++) {
                    for (int d=0; d<dimension_; d++) {
                        states_[i].components[c].mean[d] += gammaSequencePerMixture_[phraseIndex][c][t*nbStates_+i] * (*it->second)(t, d);
                    }
                }
            }
        }
        phraseIndex++;
    }
    
    // Normalize mean
    for (int i=0; i<nbStates_; i++) {
        for (int c=0; c<nbMixtureComponents_; c++) {
            if (gammaSumPerMixture_[i*nbMixtureComponents_+c] > 0) {
                for (int d=0; d<dimension_; d++) {
                    states_[i].components[c].mean[d] /= gammaSumPerMixture_[i*nbMixtureComponents_+c];
                }
            }
        }
    }
}


void HMM::baumWelch_estimateCovariances()
{
    int phraseLength;
    
    int phraseIndex(0);
    for (phrase_iterator it = this->trainingSet->begin(); it != this->trainingSet->end(); it++)
    {
        phraseLength = it->second->length();
        for (int i=0; i<nbStates_; i++) {
            for (int t=0; t<phraseLength; t++) {
                for (int c=0; c<nbMixtureComponents_; c++) {
                    for (int d1=0; d1<dimension_; d1++) {
                        for (int d2=0; d2<dimension_; d2++) {
                            states_[i].components[c].covariance[d1*dimension_+d2] += gammaSequencePerMixture_[phraseIndex][c][t*nbStates_+i]
                            * ((*it->second)(t, d1) - states_[i].components[c].mean[d1])
                            * ((*it->second)(t, d2) - states_[i].components[c].mean[d2]);
                        }
                    }
                }
            }
        }
        phraseIndex++;
    }
    
    // Scale covariance
    for (int i=0; i<nbStates_; i++) {
        for (int c=0; c<nbMixtureComponents_; c++) {
            if (gammaSumPerMixture_[i*nbMixtureComponents_+c] > 0) {
                for (int d=0; d<dimension_*dimension_; d++) {
                    states_[i].components[c].covariance[d] /= gammaSumPerMixture_[i*nbMixtureComponents_+c];
                }
            }
        }
        states_[i].addCovarianceOffset();
        states_[i].updateInverseCovariances();
    }
}


void HMM::baumWelch_estimatePrior()
{
    // Set prior vector to 0
    for (int i=0; i<nbStates_; i++)
        prior_[i] = 0.;
    
    // Re-estimate Prior probabilities
    double sumprior = 0.;
    int phraseIndex(0);
    for (phrase_iterator it = this->trainingSet->begin(); it != this->trainingSet->end(); it++)
    {
        for (int i=0; i<nbStates_; i++) {
            prior_[i] += gammaSequence_[phraseIndex][i];
            sumprior += gammaSequence_[phraseIndex][i];
        }
        phraseIndex++;
    }
    
    // Scale Prior vector
    if (sumprior == 0) {
        cout << "sumprior == 0" << endl;
    }
    for (int i=0; i<nbStates_; i++) {
        prior_[i] /= sumprior;
    }
}


void HMM::baumWelch_estimateTransitions()
{
    // Set prior vector and transition matrix to 0
    for (int i=0; i<nbStates_; i++)
        for (int j=0; j<nbStates_; j++)
            transition_[i*nbStates_+j] = 0.;
    
    int phraseLength;
    // Re-estimate Prior and Transition probabilities
    int phraseIndex(0);
    for (phrase_iterator it = this->trainingSet->begin(); it != this->trainingSet->end(); it++)
    {
        phraseLength = it->second->length();
        for (int i=0; i<nbStates_; i++) {
            for (int j=0; j<nbStates_; j++)
            {
                for (int t=0; t<phraseLength-1; t++) {
                    transition_[i*nbStates_+j] += epsilonSequence_[phraseIndex][t*nbStates_*nbStates_+i*nbStates_+j];
                }
            }
        }
        phraseIndex++;
    }
    
    // Scale transition matrix
    for (int i=0; i<nbStates_; i++) {
        if (gammaSum_[i] > 0)
            for (int j=0; j<nbStates_; j++)
                transition_[i*nbStates_+j] /= gammaSum_[i];
    }
}

#pragma mark -
#pragma mark Play!


void HMM::initPlaying()
{
    EMBasedModel::initPlaying();
    forwardInitialized_ = false;
    if (is_hierarchical_) {
        for (int i=0 ; i<3 ; i++)
            alpha_h[i].resize(this->nbStates_, 0.0);
        alpha.clear();
        previousAlpha_.clear();
        beta_.clear();
        previousBeta_.clear();
    }
    if (bimodal_)
        results.predicted_output.resize(dimension_ - dimension_input_);
}


void HMM::addCyclicTransition(double proba)
{
    if (!is_hierarchical_)
        transition_[(nbStates_-1)*nbStates_] = proba; // Add Cyclic Transition probability
}


double HMM::play(float *observation)
{
    double ct;
    
    if (forwardInitialized_) {
        ct = forward_update(observation);
    } else {
        this->likelihoodBuffer_.clear();
        ct = forward_init(observation);
    }
    
    forwardInitialized_ = true;

    if (bimodal_) {
        regression(observation, results.predicted_output);
        copy(results.predicted_output.begin(), results.predicted_output.end(), observation + dimension_input_);

        // Em-like estimation of the output sequence: deprecated now but need to be tested.
        // ========================================================================================
        // double obs_prob(log(ct)), old_obs_prob;
        // int n(1);
        // do
        // {
        //     old_obs_prob = obs_prob;
        //     forward_update_withNewObservation(observation, observation + dimension_input);
        //     regression(observation);
        //     ++n;
        // } while (!play_EM_stop(n, obs_prob, old_obs_prob));
        
        copy(observation + dimension_input_,
             observation + dimension_,
             results.predicted_output.begin());
    }
    
    this->updateLikelihoodBuffer(1./ct);
    // TODO: Put this in forward algorithm
    updateTimeProgression();
    
    return results.instant_likelihood;
}

void HMM::regression(float *observation_input, vector<float>& predicted_output)
{
    int dimension_output = dimension_ - dimension_input_;
    predicted_output.assign(dimension_output, 0.0);
    vector<float> tmp_predicted_output(dimension_output);
    
    for (int i=0; i<nbStates_; i++) {
        states_[i].likelihood(observation_input);
        states_[i].regression(observation_input, tmp_predicted_output);
        for (int d = 0; d < dimension_output; ++d)
        {
            if (is_hierarchical_)
                predicted_output[d] += (alpha_h[0][i] + alpha_h[1][i]) * tmp_predicted_output[d];
            else
                predicted_output[d] += alpha[i] * tmp_predicted_output[d];
        }
    }
}

void HMM::updateTimeProgression()
{
    results_hmm.progress = 0.0;
    for (unsigned int i=0 ; i<nbStates_; i++) {
        if (is_hierarchical_)
            results_hmm.progress += alpha_h[0][i] * static_cast<double>(i);
        else
            results_hmm.progress += alpha[i] * static_cast<double>(i);
    }
    results_hmm.progress /= static_cast<double>(nbStates_-1);
}


HMM::Results HMM::getResults() const
{
    return results_hmm;
}

#pragma mark -
#pragma mark File IO


JSONNode HMM::to_json() const
{
    JSONNode json_hmm(JSON_NODE);
    json_hmm.set_name("HMM");
    
    // Write Parent: EM Learning Model
    JSONNode json_emmodel = EMBasedModel::to_json();
    json_emmodel.set_name("EMBasedModel");
    json_hmm.push_back(json_emmodel);
    
    // Scalar Attributes
    json_hmm.push_back(JSONNode("is_hierarchical", is_hierarchical_));
    json_hmm.push_back(JSONNode("estimateMeans", estimateMeans_));
    json_hmm.push_back(JSONNode("dimension", dimension_));
    json_hmm.push_back(JSONNode("nbStates", nbStates_));
    json_hmm.push_back(JSONNode("nbMixtureComponents", nbMixtureComponents_));
    json_hmm.push_back(JSONNode("covarianceOffset", covarianceOffset_));
    json_hmm.push_back(JSONNode("transitionMode", int(transitionMode_)));
    
    // Model Parameters
    json_hmm.push_back(vector2json(prior_, "prior"));
    json_hmm.push_back(vector2json(transition_, "transition"));
    if (is_hierarchical_)
        json_hmm.push_back(vector2json(exitProbabilities_, "exitProbabilities"));
    
    // States
    JSONNode json_states(JSON_ARRAY);
    for (int i=0 ; i<nbStates_ ; i++)
    {
        json_states.push_back(states_[i].to_json());
    }
    json_states.set_name("states");
    json_hmm.push_back(json_states);
    
    return json_hmm;
}


void HMM::from_json(JSONNode root)
{
    try {
        assert(root.type() == JSON_NODE);
        JSONNode::iterator root_it = root.begin();
        
        // Get Parent: EMBasedModel
        assert(root_it != root.end());
        assert(root_it->name() == "EMBasedModel");
        assert(root_it->type() == JSON_NODE);
        EMBasedModel::from_json(*root_it);
        ++root_it;
        
        // Get If Hierarchical
        assert(root_it != root.end());
        assert(root_it->name() == "is_hierarchical");
        assert(root_it->type() == JSON_BOOL);
        if(is_hierarchical_ != root_it->as_bool()) {
            if (is_hierarchical_)
                throw JSONException("Trying to read a non-hierarchical model in a hierarchical model.", root.name());
            else
                throw JSONException("Trying to read a hierarchical model in a non-hierarchical model.", root.name());
        }
        ++root_it;

        // Get If estimate means
        assert(root_it != root.end());
        assert(root_it->name() == "estimateMeans");
        assert(root_it->type() == JSON_BOOL);
        estimateMeans_ = root_it->as_bool();
        ++root_it;
        
        // Get Dimension
        assert(root_it != root.end());
        assert(root_it->name() == "dimension");
        assert(root_it->type() == JSON_NUMBER);
        dimension_ = root_it->as_int();
        ++root_it;
        
        // Get Number of states
        assert(root_it != root.end());
        assert(root_it->name() == "nbStates");
        assert(root_it->type() == JSON_NUMBER);
        nbStates_ = root_it->as_int();
        ++root_it;
        
        // Get Number of Mixture Components
        assert(root_it != root.end());
        assert(root_it->name() == "nbMixtureComponents");
        assert(root_it->type() == JSON_NUMBER);
        nbMixtureComponents_ = root_it->as_int();
        ++root_it;
        
        // Get Covariance Offset
        assert(root_it != root.end());
        assert(root_it->name() == "covarianceOffset");
        assert(root_it->type() == JSON_NUMBER);
        covarianceOffset_ = root_it->as_float();
        ++root_it;
        
        // Get Transition Mode
        assert(root_it != root.end());
        assert(root_it->name() == "transitionMode");
        assert(root_it->type() == JSON_NUMBER);
        transitionMode_ = TRANSITION_MODE(root_it->as_int());
        ++root_it;
        
        // Reallocate model parameters
        allocate();
        
        // Get Prior Probabilities
        assert(root_it != root.end());
        assert(root_it->name() == "prior");
        assert(root_it->type() == JSON_ARRAY);
        json2vector(*root_it, prior_, nbStates_);
        ++root_it;
        
        // Get Transition Matrix
        assert(root_it != root.end());
        assert(root_it->name() == "transition");
        assert(root_it->type() == JSON_ARRAY);
        json2vector(*root_it, transition_, nbStates_*nbStates_);
        ++root_it;

        // Get Exit probabilities
        assert(root_it != root.end());
        assert(root_it->name() == "exitProbabilities");
        assert(root_it->type() == JSON_ARRAY);
        json2vector(*root_it, exitProbabilities_, nbStates_);
        ++root_it;
        
        // Get States
        assert(root_it != root.end());
        assert(root_it->name() == "states");
        assert(root_it->type() == JSON_ARRAY);
        for (int i=0 ; i<nbStates_ ; i++) {
            states_[i].from_json((*root_it)[i]);
        }
        
    } catch (JSONException &e) {
        throw JSONException(e);
    } catch (exception &e) {
        throw JSONException(e, root.name());
    }
    
    this->trained = true;
}

#pragma mark -
#pragma mark Exit Probabilities

void HMM::updateExitProbabilities(float *exitProbabilities)
{
    if (!is_hierarchical_)
        throw runtime_error("Model is Not hierarchical: method cannot be used");
    if (exitProbabilities == NULL) {
        exitProbabilities_.resize(this->nbStates_, 0.0);
        exitProbabilities_[this->nbStates_-1] = HMM_DEFAULT_EXITPROBABILITY_LAST_STATE;
    } else {
        exitProbabilities_.resize(this->nbStates_, 0.0);
        for (int i=0 ; i < this->nbStates_ ; i++)
            try {
                exitProbabilities_[i] = exitProbabilities[i];
            } catch (exception &e) {
                throw invalid_argument("Wrong format for exit probabilities");
            }
    }
}


void HMM::addExitPoint(int stateIndex, float proba)
{
    if (!is_hierarchical_)
        throw runtime_error("Model is Not hierarchical: method cannot be used");
    if (stateIndex >= this->nbStates_)
        throw out_of_range("State index out of bounds");
    exitProbabilities_[stateIndex] = proba;
}
