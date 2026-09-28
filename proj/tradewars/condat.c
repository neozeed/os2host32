#include <stdio.h>
#include <stdlib.h>

#include "tw4.h"

users userr;

void main(int argc, char *argv[])
{
FILE *in;
FILE *out;
int done=0;
int count;

in=fopen("twdata.txt","r");
if(in==NULL){printf("error!\n");exit(-1);}
out=fopen("twdata.dat","wb");
if(in==NULL){printf("error!\n");exit(-1);}
count=0;

fwrite("a",1,1,out);   //think of it as a headder... It should mean something..
while (!feof(in))
	{
	char buffer[1024];

	fgets(&buffer,1024,in);
	if(!feof(in))
		{
//		memset(&buffer,0x0,sizeof(buffer));
		memset(&userr,0x0,sizeof(userr));


		memcpy(userr.fa,buffer,strlen(buffer)-1);
//		printf("%s\n",userr.fa);
	printf("\r%d",count);
	count++;

	fgets(buffer,1024,in);
	userr.fb=atoi(buffer);


	fgets(buffer,1024,in);
	userr.fc=atoi(buffer);

	fgets(buffer,1024,in);
	userr.fd=atoi(buffer);

	fgets(buffer,1024,in);
	userr.fe=atoi(buffer);

	fgets(buffer,1024,in);
	userr.ff=atoi(buffer);

	fgets(buffer,1024,in);
	userr.fg=atoi(buffer);

	fgets(buffer,1024,in);
	userr.fh=atoi(buffer);

	fgets(buffer,1024,in);
	userr.fi=atoi(buffer);

	fgets(buffer,1024,in);
	userr.fj=atoi(buffer);

	fgets(buffer,1024,in);
	userr.fk=atoi(buffer);

	fgets(buffer,1024,in);
	userr.fl=atoi(buffer);

	fgets(buffer,1024,in);
	userr.fr=atoi(buffer);

	fgets(buffer,1024,in);
	userr.fp=atoi(buffer);

	fgets(buffer,1024,in);
	userr.fm=atoi(buffer);

	fgets(buffer,1024,in);
	userr.fo=atoi(buffer);

	fgets(buffer,1024,in);
	userr.fq=atoi(buffer);

	fgets(buffer,1024,in);
	userr.ft=atoi(buffer);

	fgets(buffer,1024,in);
	userr.fv=atoi(buffer);

		fgets(buffer,1024,in);
		userr.newcash=atof(buffer);

		fwrite(&userr,sizeof(userr),1,out);

		}

//printf(".");

	}
	printf("\nAll done!\n");
//getch();
}