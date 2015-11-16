%{
#include <stdlib.h>
#include "y.tab.h"
void yyerror(char *);
%}

%%

\[[^\n]+\]	{
			yylval.string = yytext;
			return ATCMD;
		}

\"[^\n\"]+\"	{
			yylval.string = yytext;
			return RESULT_CODE;
		}

[\[\]]		{
			return *yytext;
		}

\#[^\n]*	;	/* ignore comment line */

[ \t\n]+        ;       /* ignore whitespace */

.               yyerror("Unknown character");
%%

int yywrap(void) {
    return 1;
}
