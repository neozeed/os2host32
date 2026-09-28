#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//#include <conio.h>
#include <string.h>


//32bit compiler
typedef short int16;
typedef long  int32;
typedef long BOOL;




typedef struct {
	char	msg[1024];
	int16	destin;
} smr;

typedef struct {
	char 	name[25];
	char 	realname[14];
	char	laston[10];
	char	linelen;
	char	pagelen;
	char	sl;
	char	age;
	char	sex;
	char 	callsign[8];
	double	gold;
	char	alert;
	char	smw;
	char	nomail;
	}userrec;


// I think this is working accidentally
// the logic is all fucked up.
// but it shouldn't be *THAT* hard
// I think because I'm doing it BACKWARDS....
void ssm(int16 dest,char *stra)
{
	int16 e;
	int16 cp;
	int16 t;
	smr x;
	userrec u;
	FILE *smg;

	smg=fopen("twsmf.dat","a+b");
	if(smg==NULL)
	{printf("error with twsmf.dat\n");}
	else
	{
		fseek(smg,0,SEEK_END);
		e=ftell(smg);
		rewind(smg);
		if(e==0)		//empty file, so we start at the beginning.....
			cp=0;
		else
		{
			t=e-(1*sizeof(x));						//set for the last record
			fseek(smg,t*sizeof(x),SEEK_SET);		//position file
			fread(&x,sizeof(x),1,smg);				//read in last record
			while( (t>0) && (x.destin!=-1) )
			{
				t=t-(1*sizeof(x));
				fseek(smg,t*sizeof(x),SEEK_SET);
				fread(&x,sizeof(x),1,smg);
			}
			if(t==0)
				cp=e/sizeof(x);
			cp=t+1;
		}
		fseek(smg,cp*sizeof(x),SEEK_SET);
		memset(x.msg,0x0,sizeof(x.msg));
		memcpy(x.msg,stra,strlen(stra));
		x.destin=dest;
		fwrite(&x,sizeof(x),1,smg);
		fclose(smg);
	}
}


void main(void)
{
FILE *smg;
smr x;
int len;
int count;

count=0;

	smg=fopen("twsmf.dat","a+b");
	if(smg==NULL)
	{printf("error with twsmf.dat\n");}
	else
	{
	fseek(smg,0,SEEK_END);
	len=ftell(smg);
	printf("the message file is %d bytes or %d records\n",len,len/sizeof(x));
	printf("rec\tto\tmessage\n");
	while(count<len/sizeof(x))
		{
		fseek(smg,count*sizeof(x),SEEK_SET);		//position file
		fread(&x,sizeof(x),1,smg);
		if(x.msg[strlen(x.msg)-1]==10)
			x.msg[strlen(x.msg)-1]=0x0;
		//printf("%d",x.msg[strlen(x.msg)-1]);


		printf("%d\t%d\t[",count,x.destin);
		fwrite(x.msg,1,strlen(x.msg),stdout);
		printf("]\n");
		count++;
		}

	fclose(smg);
	}//we opened the file!
}