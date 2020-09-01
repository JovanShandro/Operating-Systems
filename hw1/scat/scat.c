#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>

int main(int argc, char *argv[]) 
{
	int opt, flag = 1;
	char c; //used for copying data a byte at a time

	while( (opt = getopt(argc, argv,"lsp")) != -1)
	{
		switch (opt)
		{
			case 'l':
				flag = 1;
				break;
			case 's':
				flag = 2;
				break;
			case 'p':
				flag = 3;
				break;
			default: /*?*/
				fprintf(stderr, "Option not found!\n");
				exit(EXIT_FAILURE);
		}
	}

	// C lib copy loop
	if(flag == 1)
	{
		while((c = getc(stdin)) != EOF)
		{
			if(ferror(stdin))
			{
				fprintf(stderr, "Error state in stdin!\n");
				exit(EXIT_FAILURE);				
			}

			if(putc(c, stdout) == EOF)//An error occured
				exit(EXIT_FAILURE);				
			
			if(ferror(stdout))
			{
				fprintf(stderr, "Error state in stdout!\n");
				exit(EXIT_FAILURE);				
			}
		}
	}// System call copy loop
	else if (flag == 2) 
	{ 
		while(read(0, &c, 1) >0) // reading from stdin
		{
			if(ferror(stdin))
			{
				fprintf(stderr, "Error state in stdin!\n");
				exit(EXIT_FAILURE);
			}
			//writing in stdout
			if(write(1, &c, 1) < 0)	//An  error occured
			{
				fprintf(stderr, "Error occured in write sys call!\n");
				exit(EXIT_FAILURE);				
			}
			
			if(ferror(stdout))
			{
				fprintf(stderr, "Error state in stdout!\n");
				exit(EXIT_FAILURE);				
			}
		}
	}// sendfile system call 
	else if(flag == 3)
	{
		off_t offset = 0;

		while(sendfile(1, 0, &offset, 4096))
		{	
			if(ferror(stdin) == EOF)
				exit(EXIT_FAILURE);
			if(fflush(stdin) == EOF)
				exit(EXIT_FAILURE);
			if(ferror(stdout) == EOF)
				exit(EXIT_FAILURE);				
		}
	}

	return 0;
}
