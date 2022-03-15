#ifndef _PARSE_MARKERS_H
#define _PARSE_MARKERS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_CODON_LIST_SIZE 20
#define CODON_LIST_SIZE_MULTIPLIER 2
#define DEFAULT_OFFSET 0
#define INITIAL_LINE_SIZE 500
#define LINE_SIZE_MULTIPLIER 2
#define MARKER_SEPARATORS ","
#define CODON_SIZE 3

int parse_markers_file( char* file_name, int** codon_list, int* number_of_codons );

#endif /* _PARSE_MARKERS_H */
