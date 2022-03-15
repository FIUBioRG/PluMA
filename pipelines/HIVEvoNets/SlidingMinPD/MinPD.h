#ifndef _MINPD_H
#define _MINPD_H



#define MAXLENGHT 10000 
#define LINELIMIT 1000
#define NUMSEQS 1000
#define OFFSET 60
#define NAMELEN 18
#define DEBUG_MODE 0
#define INF9999 9999.9


#define RepErr(s) { perror((s)); exit(EXIT_FAILURE); }

#define MALLOC(s,t) if(((s) = malloc(t)) == NULL) { RepErr("error: malloc() "); }

char **MatrixInit(int s, int dim1, int dim2);

typedef struct {
     char Seq[MAXLENGHT];
     int time; /* in months or years*/
     char Id[NAMELEN];
}Fasta;

typedef struct  {
	int x; 
	int y;
}XY_COORD;

typedef struct  {
	int bkp; 
	double dist;
}RecRes;

typedef struct {
     int Anc_Idx;
     int Child_Idx;
	 double div;
	 double dist;
}Mintaxa;

typedef struct BKPNode BKPNode;  /* To link minCLRs */
struct BKPNode{
	int Child_Idx;
	BKPNode *Next; 
	int BKP;
};

typedef struct FDisNode FDisNode;  /* To link minCLRs */
struct FDisNode{
	int Idx;
	double dist;
};

double TN93distance(int pT, int pC, int pG, int pA, int p1, int p2, int gaps,
					int matches, int al_len, double alpha, int s, int t);

double JC69distance(int gaps, int matches, int al_len, double alpha);
double K2P80distance(int p1, int p2, int gaps, int matches, int al_len, double alpha);

void ErrorMessageExit(char *errMsg,int opt);


enum  {false, true} Bool;
enum {JC69, K80, TN93} Models;

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define MAX3(c,d,e) (MAX(MAX((c),(d)),(e)))


#endif /* _MINPD_H */
