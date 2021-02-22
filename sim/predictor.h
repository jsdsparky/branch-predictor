// Jack DeLano (.11)
#ifndef _PREDICTOR_H_
#define _PREDICTOR_H_

#include "utils.h"
#include "tracer.h"

#define UINT16      unsigned short int

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

class PREDICTOR{

  // The state is defined for Gskew

 private:


  UINT32 ghr;             // global history register
  UINT32 historyLength;
  
  UINT32 *pht1;          // pattern history table 1
  UINT32 *pht2;          // pattern history table 2
  UINT32 phtIdxLength;
  UINT32 numPhtEntries;
  
  UINT32 *localPht1;
  UINT32 *localPht2;
  UINT32 localPhtIdxLength;
  UINT32 numLocalPhtEntries;

  UINT32 *bimodal;
  UINT32 bimodalIdxLength;
  UINT32 numBimodalEntries;
  
  UINT32 *bht;
  UINT32 bhtIdxLength;
  UINT32 numBhtEntries;
  UINT32 localHistoryLength;

  //for tournament predictors
  UINT32 tour1IdxLength;
  UINT32 *tour1;
  UINT32 numTour1Entries;
  
  UINT32 tour2IdxLength;
  UINT32 *tour2;
  UINT32 numTour2Entries;
  
  typedef struct VVector {
    UINT32 v1;
    UINT32 v2;
  } VVector;
  

 public:

  // The interface to the four functions below CAN NOT be changed

  PREDICTOR(void);
  bool    GetPrediction(UINT32 PC);

  //add for tournament predictor
  bool    GetLocalPrediction(UINT32 PC);
  bool    GetGlobalPrediction(UINT32 PC);

  void    UpdatePredictor(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget);
  void    TrackOtherInst(UINT32 PC, OpType opType, UINT32 branchTarget);


  // Contestants can define their own functions below

  UINT32 MapIndex(UINT32 a, UINT32 b, UINT32 c, UINT32 n);
  VVector ConstructV(UINT32 PC, UINT32 hist, UINT32 histLen, UINT32 idxLen);
  bool Get2BcgskewPrediction(UINT32 PC);
  bool GetGskewPrediction(UINT32 PC);
  bool GetPskewPrediction(UINT32 PC);
  bool GetBimodalPrediction(UINT32 PC);

};


/***********************************************************/
#endif

