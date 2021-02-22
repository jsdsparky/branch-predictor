// Jack DeLano (.11)

// References:
// [1] http://meseec.ce.rit.edu/eecc722-fall2002/papers/branch-prediction/7/michaud97trading.pdf
// [2] https://hal.inria.fr/inria-00073060/document


#include "predictor.h"

#define PHT_CTR_MAX  3
#define TOUR_CTR_MAX 3
#define PHT_CTR_INIT 2

#define BIMOD_IDX_LEN 15
#define TOUR1_LEN  15
#define TOUR2_LEN 14
#define PHT_IDX_LEN 15
#define LOCAL_PHT_IDX_LEN 15
#define BHT_IDX_LEN 12

#define HIST_LEN 21
#define LOCAL_HIST_LEN 21

/////////////// STORAGE BUDGET JUSTIFICATION ////////////////
// Hybrid 2Bc-gskew-pskew Predictor:

// Total storage budget: 62.5KB (+ 21 bits of history)

// Total counters for Bimodal predictor: 2^15
// Total PHT size for Bimodal predictor = 2^15 * 2 bits/counter = 2^16 bits = 8KB
// GHR size: 21 bits

// Total PHT counters for e-gskew predictors: 2*2^15
// Total PHT size for e-gskew predictors = 2*2^15 * 2 bits/counter = 2^17 bits = 16KB

// Total PHT counters for e-pskew predictors: 2*2^15
// Total PHT size for e-pskew predictors = 2*2^15 * 2 bits/counter = 2^17 bits = 16KB

// Total Tournament 1 counters: 2^15
// Total Tournament 1 counter's size = 2^15 * 2 bits/counter = 2^16 bits = 8KB

// Total Tournament 2 counters: 2^14
// Total Tournament 2 counter's size = 2^14 * 2 bits/counter = 2^15 bits = 4KB

// Total BHT counters: 2^12
// Total BHT size: 2^12 * 21 bits/counter = 10.5KB
/////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

PREDICTOR::PREDICTOR(void){
  historyLength = HIST_LEN;
  
  // initialize e-gskew predictor tables 
  phtIdxLength = PHT_IDX_LEN;
  ghr = 0;
  numPhtEntries    = (1<< phtIdxLength);

  pht1 = new UINT32[numPhtEntries];
  pht2 = new UINT32[numPhtEntries];

  for(UINT32 ii = 0; ii < numPhtEntries; ii++) {
    pht1[ii] = PHT_CTR_INIT;
    pht2[ii] = PHT_CTR_INIT;
  }

  // initialize tournament predictor tables
  tour1IdxLength = TOUR1_LEN;
  numTour1Entries = (1 << tour1IdxLength);
  tour1 = new UINT32[numTour1Entries];
  for(UINT32 jj = 0; jj < numTour1Entries; jj++) {
    tour1[jj] = TOUR_CTR_MAX/2 + 1;
  }
  
  tour2IdxLength = TOUR2_LEN;
  numTour2Entries = (1 << tour2IdxLength);
  tour2 = new UINT32[numTour2Entries];
  for(UINT32 kk = 0; kk < numTour2Entries; kk++) {
    tour2[kk] = TOUR_CTR_MAX/2;
  }

  // initialize bimodal predictor table
  bimodalIdxLength = BIMOD_IDX_LEN;
  numBimodalEntries = (1 << bimodalIdxLength);
  bimodal = new UINT32[numBimodalEntries];
  for(UINT32 ll = 0; ll < numBimodalEntries; ll++) {
    bimodal[ll] = PHT_CTR_INIT;
  }
  
  // initialize local history table
  localHistoryLength = LOCAL_HIST_LEN;
  bhtIdxLength = BHT_IDX_LEN;
  numBhtEntries = (1 << bhtIdxLength);
  bht = new UINT32[numBhtEntries];
  for(UINT32 mm = 0; mm < numBhtEntries; mm++) {
    bht[mm] = 0;
  }
  
  // initialize e-pskew predictor tables 
  localPhtIdxLength = LOCAL_PHT_IDX_LEN;
  numLocalPhtEntries    = (1<< localPhtIdxLength);

  localPht1 = new UINT32[numLocalPhtEntries];
  localPht2 = new UINT32[numLocalPhtEntries];

  for(UINT32 nn = 0; nn < numLocalPhtEntries; nn++) {
    localPht1[nn] = PHT_CTR_INIT;
    localPht2[nn] = PHT_CTR_INIT;
  }

}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

// gskew mapping of index (see [1])
UINT32 PREDICTOR::MapIndex(UINT32 a, UINT32 b, UINT32 c, UINT32 n) {
  // perform H(a)
  UINT32 y1 = a % 2;
  UINT32 yn = (a >> (n-1)) % 2;
  UINT32 ha = (a >> 1) + ((y1^yn) << (n-1));
  
  // perform H_inv(b)
  UINT32 ynxory1 = (b >> (n-1)) % 2;
  yn = (b >> (n-2)) % 2;
  y1 = ynxory1^yn;
  UINT32 hinvb = ((b << 1) + y1) % (1 << n);
  
  return ha^hinvb^c;
}

// construct v vector for hashing into table indices (see [1])
PREDICTOR::VVector PREDICTOR::ConstructV(UINT32 PC, UINT32 hist, UINT32 histLen, UINT32 idxLen) {
  UINT32 v = (PC << histLen) + hist;
  
  // v1 is lower idxLen bits of v
  // v2 is NEXT lower idxLen bits of v
  UINT32 v1 = v % (1 << idxLen);
  UINT32 v2 = (v >> idxLen) % (1 << idxLen);
  
  PREDICTOR::VVector myV = {v1, v2};
  return myV;
}

// get overall prediction
bool PREDICTOR::GetPrediction(UINT32 PC) {
  UINT32 tour2Index = PC % (1 << tour2IdxLength);
  
  // choose which predictor to use
  if (tour2[tour2Index] <= TOUR_CTR_MAX/2) {
    // use 2Bc-gskew predictor
    return Get2BcgskewPrediction(PC);
  } else {
    // use e-pskew predictor
    return GetPskewPrediction(PC);
  }
}

// get 2Bc-gskew prediction
bool PREDICTOR::Get2BcgskewPrediction(UINT32 PC) {
  UINT32 history = ghr % (1 << historyLength);
  
  PREDICTOR::VVector v = ConstructV(PC, history, historyLength, tour1IdxLength);
  UINT32 tour1Index = MapIndex(v.v2, v.v1, v.v2, tour1IdxLength);
  
  // choose which predictor to use
  if (tour1[tour1Index] <= TOUR_CTR_MAX/2) {
    // use e-gskew predictor
    return GetGskewPrediction(PC);
  } else {
    // use bimodal predictor
    return GetBimodalPrediction(PC);
  }
}

// get prediction for e-gskew predictor (majority vote of pht tables and bimodal predictor)
bool PREDICTOR::GetGskewPrediction(UINT32 PC) {
  UINT32 history = ghr % (1 << historyLength);
  
  PREDICTOR::VVector v = ConstructV(PC, history, historyLength, phtIdxLength);
  
  UINT32 pht1Index = MapIndex(v.v1, v.v2, v.v2, phtIdxLength);
  UINT32 pht2Index = MapIndex(v.v1, v.v2, v.v1, phtIdxLength);
  
  UINT32 pht1Counter = pht1[pht1Index];
  UINT32 pht2Counter = pht2[pht2Index];
  
  //calculate majority vote consenus
  UINT32 numVoteTaken = 0;
  numVoteTaken += pht1Counter > PHT_CTR_MAX/2;
  numVoteTaken += pht2Counter > PHT_CTR_MAX/2;
  numVoteTaken += GetBimodalPrediction(PC);
  
  if(numVoteTaken >= 2) {
    return TAKEN;
  } else {
    return NOT_TAKEN;
  }
}

// get prediction for e-pskew predictor (majority vote of local pht tables and bimodal predictor)
bool PREDICTOR::GetPskewPrediction(UINT32 PC) {
  UINT32 bhtIndex = PC % numBhtEntries;
  UINT32 localHistory = bht[bhtIndex] % (1 << localHistoryLength);
  
  PREDICTOR::VVector v = ConstructV(PC, localHistory, localHistoryLength, localPhtIdxLength);
  
  UINT32 localPht1Index = MapIndex(v.v1, v.v2, v.v2, localPhtIdxLength);
  UINT32 localPht2Index = MapIndex(v.v1, v.v2, v.v1, localPhtIdxLength);
  
  UINT32 localPht1Counter = localPht1[localPht1Index];
  UINT32 localPht2Counter = localPht2[localPht2Index];
  
  // calculate majority vote consenus
  UINT32 numVoteTaken = 0;
  numVoteTaken += localPht1Counter > PHT_CTR_MAX/2;
  numVoteTaken += localPht2Counter > PHT_CTR_MAX/2;
  numVoteTaken += GetBimodalPrediction(PC);
  
  if(numVoteTaken >= 2) {
    return TAKEN;
  } else {
    return NOT_TAKEN;
  }
}

// get prediction for bimodal predictor
bool PREDICTOR::GetBimodalPrediction(UINT32 PC){
  UINT32 bimodIndex = PC % numBimodalEntries;
  
  if(bimodal[bimodIndex] > PHT_CTR_MAX/2) {
    return TAKEN;
  }
  else {
    return NOT_TAKEN;
  }
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void PREDICTOR::UpdatePredictor(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  UINT32 bhtIndex = PC % numBhtEntries;
  UINT32 localHistory = bht[bhtIndex] % (1 << localHistoryLength);
  
  UINT32 history = ghr % (1 << historyLength);
  
  // get tour2 counter
  UINT32 tour2Index = PC % (1 << tour2IdxLength);
  UINT32 tour2Counter = tour2[tour2Index];
  bool bc2gskewSelected = tour2Counter <= TOUR_CTR_MAX/2; // true for Bc2-gskew, false for e-pskew
  
  // get tour1 counter
  PREDICTOR::VVector vTour1 = ConstructV(PC, history, historyLength, tour1IdxLength);
  UINT32 tour1Index = MapIndex(vTour1.v2, vTour1.v1, vTour1.v2, tour1IdxLength);
  UINT32 tour1Counter = tour1[tour1Index];
  bool gskewSelected = tour1Counter <= TOUR_CTR_MAX/2; // true for e-gskew, false for bimodal
  
  // get e-gskew counters
  PREDICTOR::VVector vPht = ConstructV(PC, history, historyLength, phtIdxLength);
  UINT32 pht1Index = MapIndex(vPht.v1, vPht.v2, vPht.v2, phtIdxLength);
  UINT32 pht2Index = MapIndex(vPht.v1, vPht.v2, vPht.v1, phtIdxLength);
  UINT32 pht1Counter = pht1[pht1Index];
  UINT32 pht2Counter = pht2[pht2Index];
  
  // get e-pskew counters
  PREDICTOR::VVector vLocalPht = ConstructV(PC, localHistory, localHistoryLength, localPhtIdxLength);
  UINT32 localPht1Index = MapIndex(vLocalPht.v1, vLocalPht.v2, vLocalPht.v2, phtIdxLength);
  UINT32 localPht2Index = MapIndex(vLocalPht.v1, vLocalPht.v2, vLocalPht.v1, phtIdxLength);
  UINT32 localPht1Counter = localPht1[localPht1Index];
  UINT32 localPht2Counter = localPht2[localPht2Index];
  
  // get bimodal counter
  UINT32 bimodIndex = PC % numBimodalEntries;
  UINT32 bimodalCounter = bimodal[bimodIndex];
  
  // get predictions
  bool bc2gskewPred = Get2BcgskewPrediction(PC);
  bool gSkewPred = GetGskewPrediction(PC);
  bool pSkewPred = GetPskewPrediction(PC);
  bool bimodalPred = GetBimodalPrediction(PC);
  
  // update tour2 predictor (only when Bc2-gskew and e-pskew disagree)
  if(bc2gskewPred != pSkewPred) {
    if(pSkewPred == resolveDir) {
      // e-pskew was correct
      tour2[tour2Index] = SatIncrement(tour2Counter, TOUR_CTR_MAX);
    }
    else {
      // Bc2-gskew was correct
      tour2[tour2Index] = SatDecrement(tour2Counter);
    }
  }
  
  // update tour1 predictor (only when Bc2-gskew was selected and (bimodal and e-gskew disagree))
  if(bc2gskewSelected && (gSkewPred != bimodalPred)) {
    if(bimodalPred == resolveDir) {
      // bimodal was correct
      tour1[tour1Index] = SatIncrement(tour1Counter, TOUR_CTR_MAX);
    }
    else {
      // e-gskew was correct
      tour1[tour1Index] = SatDecrement(tour1Counter);
    }
  }
  
  // update e-gskew predictor (only when overall prediction is wrong or (Bc2-gskew and e-gskew were selected))
  if((predDir != resolveDir) || (bc2gskewSelected && gskewSelected)) {
    if(gSkewPred != resolveDir) {
      // prediction was wrong (update all tables)
      if(resolveDir == TAKEN) {
        // branch was taken
        pht1[pht1Index] = SatIncrement(pht1Counter, PHT_CTR_MAX);
        pht2[pht2Index] = SatIncrement(pht2Counter, PHT_CTR_MAX);
      }
      else {
        // branch was not taken
        pht1[pht1Index] = SatDecrement(pht1Counter);
        pht2[pht2Index] = SatDecrement(pht2Counter);
      }
    }
    else {
      // prediction was correct (only update tables that were correct)
      if(resolveDir == TAKEN) {
        // branch was taken
        if(pht1Counter > PHT_CTR_MAX/2) {
          pht1[pht1Index] = SatIncrement(pht1Counter, PHT_CTR_MAX);
        }
        if(pht2Counter > PHT_CTR_MAX/2) {
          pht2[pht2Index] = SatIncrement(pht2Counter, PHT_CTR_MAX);
        }
      }
      else {
        // branch was not taken
        if(pht1Counter <= PHT_CTR_MAX/2) {
          pht1[pht1Index] = SatDecrement(pht1Counter);
        }
        if(pht2Counter <= PHT_CTR_MAX/2) {
          pht2[pht2Index] = SatDecrement(pht2Counter);
        }
      }
    }
  }
  
  // update the e-pskew predictor (only when overall prediction is wrong or e-pskew was selected)
  if((predDir != resolveDir) || !bc2gskewSelected) {
    if(pSkewPred != resolveDir) {
      // prediction was wrong (update all tables)
      if(resolveDir == TAKEN) {
        // branch was taken
        localPht1[localPht1Index] = SatIncrement(localPht1Counter, PHT_CTR_MAX);
        localPht2[localPht2Index] = SatIncrement(localPht2Counter, PHT_CTR_MAX);
      }
      else {
        // branch was not taken
        localPht1[localPht1Index] = SatDecrement(localPht1Counter);
        localPht2[localPht2Index] = SatDecrement(localPht2Counter);
      }
    }
    else {
      // prediction was correct (only update tables that were correct)
      if(resolveDir == TAKEN) {
        // branch was taken
        if(localPht1Counter > PHT_CTR_MAX/2) {
          localPht1[localPht1Index] = SatIncrement(localPht1Counter, PHT_CTR_MAX);
        }
        if(localPht2Counter > PHT_CTR_MAX/2) {
          localPht2[localPht2Index] = SatIncrement(localPht2Counter, PHT_CTR_MAX);
        }
      }
      else {
        // branch was not taken
        if(localPht1Counter <= PHT_CTR_MAX/2) {
          localPht1[localPht1Index] = SatDecrement(localPht1Counter);
        }
        if(localPht2Counter <= PHT_CTR_MAX/2) {
          localPht2[localPht2Index] = SatDecrement(localPht2Counter);
        }
      }
    }
  }
  
  //update the bimodal predictor
  if(predDir != resolveDir) {
    // overall prediction was wrong
    if(resolveDir == TAKEN) {
      // branch was taken
      bimodal[bimodIndex] = SatIncrement(bimodalCounter, PHT_CTR_MAX);
    }
    else {
      // branch was not taken
      bimodal[bimodIndex] = SatDecrement(bimodalCounter);
    }
  }
  else {
    // overall prediction was correct (only update if bimodal was correct)
    if(resolveDir == TAKEN && bimodalCounter > PHT_CTR_MAX/2) {
      // branch was taken and bimodal predictor was correct
      bimodal[bimodIndex] = SatIncrement(bimodalCounter, PHT_CTR_MAX);
    }
    else if(resolveDir == NOT_TAKEN && bimodalCounter <= PHT_CTR_MAX/2) {
      // brach was not taken and bimodal predicton was correct
      bimodal[bimodIndex] = SatDecrement(bimodalCounter);
    }
  }

  // update the history
  ghr = (ghr << 1);
  if(resolveDir == TAKEN) {
    ghr++;
  }
  
  //update the bht for local history
  bht[bhtIndex] = (bht[bhtIndex] << 1);
  if(resolveDir == TAKEN){
    bht[bhtIndex]++;
  }
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void PREDICTOR::TrackOtherInst(UINT32 PC, OpType opType, UINT32 branchTarget){

  // This function is called for instructions which are not
  // conditional branches, just in case someone decides to design
  // a predictor that uses information from such instructions.
  // We expect most contestants to leave this function untouched.

  return;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////
