%module PerlPluMA
%{
extern void log(char* msg);
extern void dependency(char* plugin);
extern char* prefix();
extern void languageLoad(char* lang);
extern void languageUnload(char* lang);

%}

extern void log(char* msg);
extern void dependency(char* plugin);
extern char* prefix();
extern void languageLoad(char* lang);
extern void languageUnload(char* lang);

