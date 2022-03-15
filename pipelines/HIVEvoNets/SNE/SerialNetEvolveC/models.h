/*  Header file for models.c                               */

/*  Nucleotide Sequence Generator - seq-gen, version 1.02  */
/*  (c) Copyright 1996, Andrew Rambaut & Nick Grassly      */
/*  Department of Zoology, University of Oxford            */


#ifndef _MODELS_H_
#define _MODELS_H_

enum { A, C, G, T };
enum { F84, HKY, REV, numModels };

extern int model;

extern double freq[4], addFreq[4];
extern double tstv, kappa;
extern double freqR, freqY, freqAG, freqCT;
extern double freqA, freqC, freqG, freqT;
extern double Rmat[6];

void SetModel(int theModel);
void SetMatrix(double *matrix, double len);
void SetVector(double *vector, short base, double len);

#endif /* _MODELS_H_ */
