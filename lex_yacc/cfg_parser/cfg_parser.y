%{
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/* prototypes */
int yylex(void);
void yyerror(char *s);

extern FILE *yyin;
%}

%union {
	int number;		/* integer value */
	char *string;		/* string value */
 }

%token <number> NUMBER
%token <string> STRING
%token ATCMD RESULT_CODE

%%

parameters:
        | parameters	parameter
        ;

parameter:
	atcmd
	|
	result_code
        ;

atcmd:
	ATCMD		{ printf("%s\n", yylval.string); }
        ;

result_code:
	RESULT_CODE	{printf("%s\n", yylval.string);}
	;

%%


void yyerror(char *s) {
	fprintf(stdout, "%s\n", s);
}

int main(int argc, char **argv) {

	if (argc > 1) {
		yyin = fopen(argv[1], "r");
	}
	else {
		fprintf(stdout, "Usage: config <file_path>\n");
		return -1;
	}
	if (yyin == NULL) {
		fprintf(stdout, "Error: file open error...\n");
		return -1;
	}
	
	yyparse();
	
	fclose(yyin);
	
	return 0;
}
