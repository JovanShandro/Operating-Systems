/*
 * cat++/catn.digit
 *
 *	Implementation of a cat -n transformation.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char *
transform(char *line)
{
    // static counter
    static int i = 1;

    // digit to be added in prefix
    char digit = i + '0';
    
    // get prefix
    char a[] = { digit, ' ', ' '};
    char * prefix = a;
    
    // copy current value of line
    int k = (int)strlen(line);
    char copy[k];
    for(int j = 0; j < k; j++)
        copy[j] = line[j];
    
    // reallocate enough space for new line and change the value of line
    line = (char *) realloc(line, strlen(prefix) + k + 1);
    sprintf(line, "%s%s", prefix, copy);
    
    // increment counter
    i++;
    
    return line;
}
