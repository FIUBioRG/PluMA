#include "parse_markers.h"

int debug = 0;

int parse_markers_file( char* file_name, int** codon_list, int* number_of_codons ){

	FILE* file;
	int codon_list_size = INITIAL_CODON_LIST_SIZE;
	char c;
	int offset = DEFAULT_OFFSET;
	int line_size = INITIAL_LINE_SIZE;
	int line_index = 0;	
	char* line;
	char* marker;
	int codon_list_index = 0;

	file = fopen( file_name, "r" );
	if ( file == NULL ){
		fprintf( stderr, "couldn't open '%s'\n", file_name );
		return( -1 );	
	}
	
	*codon_list = (int*)malloc( sizeof( int ) * codon_list_size );
	if( codon_list == NULL ){
		fprintf( stderr, "couldn't realloc codon_list\n" );
		return( -1 );
	}
	while( ( c = fgetc( file ) ) != -1 ){
		line_size = INITIAL_LINE_SIZE;
		line_index = 0;
		line = ( char* )malloc( sizeof( char ) * line_size );
		if( line == NULL ){
			fprintf( stderr, "couldn't malloc line\n" );
			return( -1 );		
		}
		line[0] = '\0';
		ungetc( c, file );
		while( ( c = fgetc( file ) ) != -1 ){
			if( ( line_index + 2 ) >= line_size ){
				line_size *= LINE_SIZE_MULTIPLIER;
				line = ( char* )realloc( line, sizeof( char ) * line_size );
				if( line == NULL ){
					fprintf( stderr, "couldn't realloc line\n" );
					return( -1 );
				}
			}
			line[line_index++] = c;
			line[line_index] = '\0';
			if( c == '\n' || c == '\r' ){
				line[line_index-1] = '\0';
				while( -1 != (c = fgetc( file )) ){
					if( !isspace( c ) ){
						ungetc( c, file );
						break;
					}
				}
				break;
			}
			if( c == '#' ){
				line[line_index - 1] = '\0';
			}
		}
		offset = 0;
		if( debug ){
			printf( "'%s'\n", line );
		}
		marker = strtok( line, MARKER_SEPARATORS );
		if( marker != NULL ){
			if( debug ){
				printf("%s",marker);
			}
			if( 1 != sscanf( marker, "%i", &offset ) ){
				fprintf( stderr, "couldn't get offset from line\n" );
				free( line );
				continue;
			}
			marker = strtok( NULL, MARKER_SEPARATORS );
		}
		else{
			free( line );
			continue;
		}
		while( marker != NULL ){
			if( debug ){
				fprintf( stderr, "\t%s\n", marker );
			}
			if( codon_list_index >= codon_list_size ){
				codon_list_size *= CODON_LIST_SIZE_MULTIPLIER;
				*codon_list = ( int* )realloc( *codon_list, sizeof( int ) * codon_list_size );
				if( *codon_list == NULL ){
					fprintf( stderr, "error reallocing codon_list\n" );
					return( -1 );
				}
			}
			if( 1 != sscanf( marker, "%*c%d", (*codon_list) + codon_list_index++ ) ){
				fprintf( stderr, "error parsing marker '%s'\n", marker );
				return( -1 );
			}
			else{
				*(*codon_list + codon_list_index - 1 ) += offset;
				*(*codon_list + codon_list_index - 1 ) *= CODON_SIZE;
				if( debug ){
					fprintf( stderr, "got %d\n", *(*codon_list + codon_list_index - 1 ) );
				}
			}
			marker = strtok( NULL, MARKER_SEPARATORS );
		}
		free( line );
	}
	*number_of_codons = codon_list_index;
	fclose( file );
	if( debug ){
		fprintf( stderr, "found %d marker(s):\n", *number_of_codons );
		for( codon_list_index = 0; codon_list_index < *number_of_codons; codon_list_index++ ){
			fprintf( stderr, "%d\n", *( *codon_list + codon_list_index ) );
		}
		fprintf( stderr, "done parsing markers file\n" );
	}
	return( 0 );
}

