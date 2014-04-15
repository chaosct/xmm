//
// training_set.h
//
// Multimodal training set
//
// Copyright (C) 2014 Ircam - Jules Francoise. All Rights Reserved.
// author: Jules Francoise <jules.francoise@ircam.fr>
// 

#ifndef rtml_training_set_h
#define rtml_training_set_h

#include <map>
#include <set>
#include "label.h"
#include "phrase.h"

using namespace std;

/**
 * @ingroup TrainingSet
 * @class TrainingSet
 * @brief Base class for the definition of (multimodal) training sets
 * @todo class description
 */
class TrainingSet
{
public:
#pragma mark -
#pragma mark === Public Interface ===
    /**
     * @brief Phrase iterator: allows to iterate over the phrases of the training set
     * @details phrases are stored as a map, the iterator is therefore equivalent to: map<int, Phrase*>::iterator
     */
    typedef typename  map<int, Phrase*>::iterator phrase_iterator;

    /**
     * @brief constant Phrase iterator: allows to iterate over the phrases of the training set
     * @details phrases are stored as a map, the iterator is therefore equivalent to: map<int, Phrase*>::const_iterator
     */
    typedef typename  map<int, Phrase*>::const_iterator const_phrase_iterator;

    /**
     * @brief label iterator: allows to iterate over the labels of the training set
     * @details labels are stored as a map, the iterator is therefore equivalent to: map<int, Label>::iterator
     */
    typedef typename  map<int, Label>::iterator label_iterator;
    
    /**
     * @brief constant label iterator: allows to iterate over the labels of the training set
     * @details labels are stored as a map, the iterator is therefore equivalent to: map<int, Label>::const_iterator
     */
    typedef typename  map<int, Label>::const_iterator const_label_iterator;
    
#pragma mark > Constructors
    /*@{*/
    /** @name Constructors */
    /**
     * @brief Constructor
     * @param _parent parent learning model => the parent is notified each time the training set
     * attributes are modified.
     * @param dimension total dimension of the training data.
     * @param flags construction flags (@see Phrase).
     * @param dimension_input dimension of the input modality in bimodal mode.
     */
    TrainingSet(rtml_flags flags=0,
                Listener* _parent=NULL,
                unsigned int dimension=PHRASE_DEFAULT_DIMENSION,
                unsigned int dimension_input = 0);
    
    TrainingSet(TrainingSet const& src);
    
    TrainingSet& operator=(TrainingSet const& src);
    
    /**
     * @brief Destructor
     * @warning phrases are only deleted if the training set is unlocked
     * @see lock()
     */
    virtual ~TrainingSet();
    
    /*@}*/

#pragma mark > Accessors & tests
    /*@{*/
    /** @name accessors and tests */
    /**
     * @brief checks if the training set is bimodal
     * @return true if the training set is bimodal (construction with BIMODAL flag)
     */
    bool is_bimodal() const;
    
    /**
     * @brief checks if the training set is empty
     * @return true if the training set is empty (no training phrases)
     */
    bool is_empty() const;
    
    /**
     * @brief Size of the training set
     * @return size of the training set (number of phrases)
     */
    unsigned int size() const;
    
    /**
     * @brief check if the training data has changed.
     * @return true is the training data or attributes have changed
     */
    bool has_changed();
    
    /**
     * @brief set the status of the training set to unchanged
     */
    void set_unchanged();
    
    /**
     * @brief Set parent model (to be notified when attributes are modified)
     * @param _parent parent model
     */
    void set_parent(Listener* _parent);
    
    /**
     * @brief Get total dimension of the training data
     * @return dimension of the training data
     */
    unsigned int get_dimension();
    
    /**
     * @brief Get dimension of the input modality in bimodal mode
     * @return dimension of the input modality
     * @throws runtime_error if the phrase is unimodal (no BIMODAL construction flag)
     */
    unsigned int get_dimension_input();
    
    /**
     * @brief Set total dimension of the training data
     * @param dimension dimension of the training data
     * @throws out_of_range if the dimension is < 1
     */
    void set_dimension(unsigned int dimension);
    
    /**
     * @brief Set the dimension of the input modality in bimodal mode
     * @param dimension_input dimension of the input modality
     * @throws runtime_error if the phrase is not bimodal
     * @throws invalid_argument if The dimension of the input modality exceeds the total dimension
     */
    void set_dimension_input(unsigned int dimension_input);
    
    /**
     * @brief checks equality
     * @param src training set to compare
     * @return true if the training sets are equal (same phrases and labels)
     */
    bool operator==(TrainingSet const &src);
    
    /**
     * @brief checks inequality
     * @see operator==
     */
    bool operator!=(TrainingSet const &src);

    /*@}*/

#pragma mark > Access Phrases
    /*@{*/
    /** @name Access Phrases */
    /**
     * @brief iterator to the beginning of phrases
     */
    phrase_iterator begin();
    
    /**
     * @brief iterator to the end of phrases
     */
    phrase_iterator end();
    
    /**
     * @brief Access Phrase by index
     * @param n index of the phrase
     * @return iterator to the phrase of index n
     * @throws out_of_range if the phrase does not exist.
     */
    phrase_iterator operator()(int n);
    
    /*@}*/

#pragma mark > Connect Phrases
    /*@{*/
    /** @name Connect Phrases */
    /**
     * @brief Connect a phrase to the training set (unimodal case)
     * @details This method is used in shared memory to pass an array to the training set.
     * If the phrase does not exist, it is created at the specified index.
     * @param phraseIndex index of the phrase in the training set. If it does not exist, the phrase is created.
     * @param pointer_to_data pointer to the data array
     * @param length length of the phrase
     * @throws runtime_error if not in shared memory (construction with SHARED_MEMORY flag)
     * @throws runtime_error if bimodal (construction with the BIMODAL flag)
     */
    void connect(int phraseIndex, float *pointer_to_data, unsigned int length);
    
    /**
     * @brief Connect a phrase to the training set (synchronous bimodal case)
     * @details This method is used in shared memory to pass an array to the training set.
     * If the phrase does not exist, it is created at the specified index.
     * @param phraseIndex index of the phrase in the training set. If it does not exist, the phrase is created.
     * @param pointer_to_data_input pointer to the data array for the input modality
     * @param pointer_to_data_output pointer to the data array for the output modality
     * @param length length of the phrase
     * @throws runtime_error if not in shared memory (construction with SHARED_MEMORY flag)
     * @throws runtime_error if bimodal (construction with the BIMODAL flag)
     */
    void connect(int phraseIndex, float *pointer_to_data_input, float *pointer_to_data_output, unsigned int length);
    
    /*@}*/

#pragma mark > Record training Data
    /*@{*/
    /** @name Record training Data */
    /**
     * @brief Record training data
     * @details The method appends an observation to the data phrase. The observation need to have a 
     * size "dimension". In bimodal mode, the observation must concatenate input and output observations.
     * A phrase is created if it does not exists at the given index
     * @param phraseIndex index of the phrase
     * @param observation observation vector to append to the phrase
     * @throws runtime_errpr if phrase has shared memory (construction with SHARED_MEMORY flag)
     * @todo: Add input/output methods
     */
    void recordPhrase(int phraseIndex, float *observation);
    
    /**
     * @brief reset phrase to default
     * @details The phrase is set to an empty phrase with the current attributes (dimensions, etc).
     * The phrase is created if it does not exists at the given index.
     * @param phraseIndex index of the phrase
     */
    void resetPhrase(int phraseIndex);
    
    /**
     * @brief delete a phrase
     * @warning if the training set is locked, the phrase iself is not deleted (only the reference), 
     * i.e. its memory is not released.
     * @param phraseIndex index of the phrase
     * @throws out_of_bounds if the phrase does not exist
     */
    void deletePhrase(int phraseIndex);
    
    /**
     * @brief delete all phrases of a given class
     * @warning if the training set is locked, the phrases themselves are not deleted (only the references), 
     * i.e. their memory is not released.
     * @param label label of the class to delete
     * @throws out_of_bounds if the label does not exist
     */
    void deletePhrasesOfClass(Label const& label);
    
    /**
     * @brief delete all empty phrases
     */
    void deleteEmptyPhrases();
    
    /**
     * @brief delete all phrases
     * @warning if the training set is locked, the phrases themselves are not deleted (only their references), 
     * i.e. their memory is not released.
     */
    void clear();
    
    /*@}*/

#pragma mark > Handle Labels
    /*@{*/
    /** @name Handle Labels */
    /**
     * @brief set default phrase label for new phrases
     * @param defLabel default Label
     */
    void setDefaultLabel(Label const& defLabel);
    
    /**
     * @brief set label of a phrase to default
     * @param phraseIndex index of the phrase in the training set
     * @throws out_of_range if the phrase does not exist
     */
    void setPhraseLabelToDefault(int phraseIndex);
    
    /**
     * @brief set the label of a phrase
     * @param phraseIndex index of the phrase in the training set
     * @param label label to set
     * @throws out_of_range if the phrase does not exist
     */
    void setPhraseLabel(int phraseIndex, Label const& label);
    
    /**
     * @brief get the current label of a phrase in the training set
     * @param phraseIndex index of the phrase in the training set
     * @return label of the phrase
     */
    Label getPhraseLabel(int phraseIndex);
    
    /**
     * @brief get the pointer to the sub-training set containing all phrases with a given label
     * @warning in order to protect the phrases in the current training set, the sub-training set
     * returned is locked
     * @param label target label
     * @return pointer to the sub-training set containing all phrases with a given label
     * @throws out_of_range if the label does not exist
     */
    TrainingSet* getSubTrainingSetForClass(Label const& label);
    
    /*@}*/

#pragma mark > JSON I/O
    /*@{*/
    /** @name File IO */
    /**
     * @brief Write to JSON Node
     * @return JSON Node containing training set information and data
     */
    JSONNode to_json() const;
    
    /**
     * @brief Read from JSON Node
     * @param root JSON Node containing training set information and data
     * @throws JSONException if the JSON Node has a wrong format
     */
    void from_json(JSONNode root);
    
    /*@}*/

#pragma mark > Debug
    /*@{*/
    /** @name Debug */
    /**
     * @brief Dump training set information to stream
     * @param outStream output stream
     */
    void dump(ostream& outStream);
    
    /*@}*/

#ifdef SWIGPYTHON
#pragma mark > Python
    /*@{*/
    /** @name Python methods */
    /**
     * @brief special python "print" method to get information on the object
     */
    char *__str__() {
        stringstream ss;
        dump(ss);
        string tmp = ss.str();
        char* cstr = strdup(tmp.c_str());
        return cstr;
    }
    
    // TODO: Make class extension in Swig interface file using %extend ?
    /**
     * @brief Append data to a phrase from a numpy array
     * @param dimension_total total dimension of the multimodal observation vector
     * @param observation multimodal observation vector
     * @todo Make class extension in Swig interface file using %extend ?
     */
    void recordPhrase(int phraseIndex, int dimension_total, double *observation)
    {
        float *observation_float = new float[dimension_total];
        for (int d=0; d<dimension_total; d++) {
            observation_float[d] = float(observation[d]);
        }
        
        recordPhrase(phraseIndex, observation_float);
        
        delete[] observation_float;
    }
    /*@}*/
#endif
    
#pragma mark -
#pragma mark === Public Attributes ===
    /**
     * @brief Training Phrases
     * @details Phrases are stored in a map: allows the easy addition/deletion of phrases by index.
     */
    map<int, Phrase*> phrases;
    
    /**
     * @brief Labels associated to each phrase
     * @details As for phrases, labels are stored in a map for easy addition/deletion of phrases by index.
     */
    map<int, Label> phraseLabels;
    
    /**
     * @brief Set containing all the labels present in the training set
     */
    set<Label> allLabels;
    
private:
#pragma mark -
#pragma mark === Private Methods ===
#pragma mark > between Training Sets
    /*@{*/
    /** @name Copy between Training Sets */
    /**
     * @brief Copy between Training Sets
     * @param dst destination Training Set
     * @param src Source Training Set
     */
    void _copy(TrainingSet *dst, TrainingSet const& src);

    /*@}*/

#pragma mark > Lock
    /*@{*/
    /** @name Lock */
    /**
     * @brief Lock training set to keep the phrases from being deleted at destruction
     */
    void lock();
    
    /*@}*/

#pragma mark > Handle Labels
    /*@{*/
    /** @name Handle Labels: protected methods */
    /**
     * @brief update the sub-training set for a given label
     */
    void updateSubTrainingSet(Label const& label);
    
    /**
     * @brief create all the sub-training sets: one for each label
     * @details each subset contains only the phrase for the given label
     */
    void updateSubTrainingSets();
    
    /**
     * @brief update the list of all existing labels of the training set
     */
    void updateLabelList();

    /*@}*/
    
#pragma mark -
#pragma mark === Private Attributes ===
    /**
     * @brief Construction flags
     * @see  Phrase
     */
    rtml_flags flags_;

    /**
     * @brief defines if the phrase has its own memory
     */
    bool owns_data;

    /**
     * @brief defines if the phrase is bimodal
     */
    bool bimodal_;

    /**
     * @brief total dimension of the training data
     */
    unsigned int dimension_;

    /**
     * @brief dimension of the input modality in bimodal mode
     */
    unsigned int dimension_input_;
    
    /**
     * @brief Parent Object. The parent is notified when attributes of the training set are changed.
     */
    Listener* parent_;
    
    /**
     * @brief Default label for new phrases
     */
    Label defaultLabel_;

    /**
     * @brief used to track changes in the training set
     */
    bool has_changed_;

    /**
     * @brief if true, the training set is locked, i.e. its memory cannot be released.
     */
    bool locked_;
    
    /**
     * @brief Sub-ensembles of the training set for specific classes
     */
    map<Label, TrainingSet> subTrainingSets_;
};

#endif