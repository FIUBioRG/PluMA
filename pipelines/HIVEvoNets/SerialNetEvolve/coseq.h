/*Header for coseq.c*/
/*Nick Grassly 1996*/

#ifndef _COSEQ_H_
#define _COSEQ_H_

#define MAX_RATE_CATS 32

extern double gammaShape;
extern int numCats, rateHetero;
extern double catRate[MAX_RATE_CATS];

enum {
	NoRates,
	CodonRates,
	GammaRates,
	DiscreteGammaRates
};

/* prototypes */

void SetModel(int model);
void SetCategories();
void SeqEvolve();
void SeqEvolveSSTV();
void RateHetMem();

#endif /* _COSEQ_H_ */
