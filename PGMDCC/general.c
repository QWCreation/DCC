#include "general.h"

// global paraments
extern char *dc_ip;
extern char *dc_user;

extern char *db_ip;
extern char *db_user;

extern unsigned short dc_port;

extern int log_lev ;

int set_envionment ( const char *filename )
{
	int             rc = 0; 
	FILE            *fp1; 
	char            dummy_char[200];
	char			chk_char;

    if ( ( fp1 = fopen ( filename, "r" ) ) == NULL )
    {
		fprintf(stderr, "Failed to read System Config File: %s. errno[%d]\n", filename, errno );
		rc = errno;
	}
	if( 0 == rc ){
	    while( ( fgets ( dummy_char, 200, fp1 ) ) != NULL ) {
	        memcpy ( &chk_char, dummy_char, 1 );
	        if ( chk_char != '#' ) {
				char *tmpPonter = strtok( dummy_char , "" );
				tmpPonter = strtok( dummy_char , "=" );
				tmpPonter = strtok( NULL, "\n" );			
				if ( tmpPonter ){
					rc = setenv( dummy_char, tmpPonter ,1);
					if ( rc != 0 )
					{
						fprintf(stderr, "Failed to set envionment. error[%d]\n", errno );
						break;
					}
				}
	        }
	    }
	    fclose ( fp1 );	
	}
	
	char *ch_lev = getenv("LOG_LEV");
	if( ch_lev != NULL || ch_lev != "" ){
		log_lev = atoi( ch_lev );
	}
	return rc;
}
