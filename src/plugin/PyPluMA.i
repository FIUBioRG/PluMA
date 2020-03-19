%module PyPluMA
%{
extern void log(char* msg);
extern void dependency(char* plugin);
%}

extern void log(char* msg);
extern void dependency(char* plugin);
