/* Header treevolve.h     */
/* (c) Nick Grassly 1997  */
/* Dept. Zoology, Oxford. */

#ifndef _TREEVOLVE_H_
#define _TREEVOLVE_H_

#define MAX_NUMBER_REGIMES 10
#define MAX_NUMBER_SUBPOPS 100

#define MIN_NE 1.0e-15


#define PROGRAM_NAME "SerialNetEvolve"
#define VERSION_NO 1.0

typedef struct Node Node;
struct Node {
	Node *daughters[2];
	double time;
	short *ancestral;
	char *sequence;
	short type;/*0=ca 1=re 2=tip 3=looked at*/
	int cutBefore;
	int deme;
	Node *next;
	Node *previous;
	Node *L_father;	/* Added by PB for MaxQueue/slimTree */
	Node *R_father;	/* Added by PB for MaxQueue/slimTree */
	Node *MaxNext;	/* Added by PB for MaxQueue/slimTree */
	char ID[10];	/* Added by PB */
	int Period;		/* Added by PB */
	int sampled;/* Added by PB 9/17/04*/
	Node *jumpright;/* Added by PB for slimTree*/
	Node *jumpleft;/* Added by PB for slimTree*/
};


Node ***NodesPerPeriod; /* Added by PB 12/07/05 for conservative sampling */




typedef struct Regime Regime;
struct Regime {
	double t, N, e, r, m;
	int d;
};

extern int numBases, sampleSize;
extern double mutRate;
extern Node *first, *avail;
extern int ki[MAX_NUMBER_SUBPOPS], K, noRE, noCA, noMI;
extern double globTime, factr;
extern double genTimeInvVar;

extern double intervalDis;
extern double prevTime;
extern int pCounter;


extern int SSizeperP; /* Sample Size per period*/
extern int PeriodsWanted; /* Number of Sampling Periods */
extern int StartAt; /* First Period to start sampling */
extern int ClassicTV;
extern int NoClock; /* Clock */
extern double iNodeProb;
extern int range;
extern int repNo;

extern char OFile[100];
extern int outputFile;

/*prototypes*/
void Stack(Node *going);
long CalcGi(int deMe);
void Recombine(double t, int deme);
void Coalesce(double t, int deme);
void Migration(int deme, int numDemes);

#endif /* _TREEVOLVE_H_ */
