/*
 mathfuncs.h
 
 
   U(0,1): AS 183: Appl. Stat. 31:188-190 
   Wichmann BA & Hill ID.  1982.  An efficient and portable
   pseudo-random number generator.  Appl. Stat. 31:188-190

   x, y, z are any numbers in the range 1-30000.  Integer operation up
   to 30323 required.
 	
*/

#ifndef _MATHFUNCS_H_
#define _MATHFUNCS_H_

//void SetSeed (int seed);

void SetSeed (unsigned long seed);
double rndu (void);

double rndgamma (double s);
double zeroin(double ax, double bx, double (*f)(double x));
int DiscreteGamma (double freqK[], double rK[], double alfa, double beta, int K, int median);

#endif /* _MATHFUNCS_H_ */
