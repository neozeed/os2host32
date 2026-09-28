#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "tw4.h"


void compmenu(void)
{
	printf("\nComputer commands:\n");
	printf("\nC - <C>ommerce report");
	printf("\nF - <F>ind sector");
	printf("\nI - <I>nformation about your ship");
	printf("\nR - <R>ank players");
	printf("\nS - <S>end a message to another ship");
	printf("\nM - Read your <M>essages");
	printf("\nQ - <Q>uit computer");
	fflush(stdin);
}

//finds & prints a path to a sector...
void findsec(int16 prr)
{
	int16 a;
	int16 b;                //the numeric 'destination'..
	int16 sud;
	char i[1024];

	a=prr;
	printf("\nWhat is your target sector? ");
	fflush(stdin);
	scanf("%s",i);
	b=atoi(i);
	if( (b<1) || (b>ls-lp) )
		printf("Valid sector numbers are from 1 to %d\n",ls-lp);
	else
	{
		if(a==b)
			printf("You are already in that sector!\n");
		else
		{
			printf("Computing shortest path...\n");
			shortest(a,b);
			if(s[a][1]==0)
				printf("There was an error in computation between sectors.\n");
			else
			{
				printf("\nThe shortest path from sector %d to sector %d is:\n",a,b);
				printf("%d",a);
				sud=a;
				while(sud>0)
				{
					sud=s[sud][1];
					if(sud!=0)
						printf(" - %d",sud);
					else
						printf("\n");
				}
				usert=readin(lp+prr);
				e[1]=usert.fb;
				e[2]=usert.fc;
				e[3]=usert.fd;                  //reset the exits to the current sector...
				e[4]=usert.fe;
				e[5]=usert.ff;
				e[6]=usert.fg;
			}
		}//end looking for sector..
	}//end else for valid sector.

}

void reportsec(int16 s2)
{
	int16 a;
	int16 p;
	int16 n; //the sector to work with....
	char i[1024];

	printf("\nWhat sector is the port in[%d]",userr.ff);
	memset(i,0x0,sizeof(i));
	fflush(stdin);
	scanf("%s",i);
	a=atoi(i);
	if(strlen(i)<1)
		n=userr.ff;
	if( (a<1) || (a>ls-lp) )
		printf("The Empire only possesses sectors 1 to %d\n",ls-lp);
	else
	{
		usert=readin(lp+a);
		p=usert.fh;
		if( (p==0) || ((usert.fl>0)&&(usert.fm!=pn)))
			printf("\nI have no information about that port.\n");
		else
		if(p!=1)
		{
			upport(lp+a);
			otherport(p+ls);
		}
		else
			port1();
	}

}

void rankings(void)
{
	int16 p;
	int16 r;
	BOOL abort;
	BOOL next;

	printf("\nRanking players...\n");
	p=rank();
	printf("Player Rankings: %s %s\n","date","time");
	printf("Rank     Value          Player\n");
    printf("~~~~  ~~~~~~~~~~~~      ~~~~~~\n");
	r=1;
	abort=FALSE;
	//while( !(p=-1) && !abort)
	while(p!=-1)
	{
		usert=readin(p);
		printf("%d\t%d\t\t%s\n",r,usert.fv,usert.fa);
		p=usert.ft;
		r=r+1;
	}

}

int16 rank(void)
{
	int16 l;
	int16 go;
	int16 ho;
	int16 fo;
	int16 n;
	int16 o;
	int16 jo;
	int16 ko;
	int16 lo;
	int16 v;
	int16 c;
	int16 p;
	BOOL done;

	for(l=2;l!=lp;l++)              //cycle through the users...
	{
		usert=readin(l);
		if(usert.fm==0)         //unknown....!!!!
		{
			usert.fv=-1;
			writeout(l,usert);
		}
		else
			if(usert.fc!=0) //killed by....
			{
				usert.fv=0;
				writeout(l,usert);
			}
			else
			{
				go=usert.fg+usert.fe;   //fighers + armor
				ho=usert.fh;                    //cargo holds
				fo=usert.fi;                    //ore
				jo=usert.fj;                    //organics
				ko=usert.fk;                    //equipment
				lo=usert.fl;                    //credits
				//v := g0*2+h0*25+ROUND(f0*2.5)+j0*5+ROUND(k0*8.75)+ROUND(l0/20);
				  v  = go*2+ho*25+      fo*2.5 +jo*5+      ko*8.73 +10/20;
				usert.fv=v;             //score...
				writeout(l,usert);
			}
	}
	for(l=lp+1;l!=ls;l++)
	{
		usert=readin(l);
		if((usert.fl!=0) && (usert.fm>2))
		{
			a=usert.fl;
			p=usert.fm;
			usert=readin(p);
			usert.fv=usert.fv+a*10;
			writeout(p,usert);
		}
	}
	p=0;
	for(l=2;l!=lp;l++)
	{
		usert=readin(l);
		v=usert.fv;
		if(v!=-1)
		{
			n=p;
			o=0;
			done=FALSE;
			if(p==0)
			{
				p=l;
				usert.ft=-1;
				writeout(l,usert);
			}//end p=0
			else
				while(!done)
				{
					usert=readin(n);
					if((v>usert.fv) && o==0)
					{
						usert=readin(l);
						usert.ft=p;
						writeout(l,usert);
						p=l;
						done=TRUE;
					}
					else
						if(v>usert.fv)
						{
							usert=readin(o);
							c=usert.ft;
							usert.ft=l;
							writeout(o,usert);
							usert=readin(l);
							usert.ft=c;
							writeout(l,usert);
							done=TRUE;
						}
						else
							if(usert.ft==-1)
							{
								usert=readin(n);
								usert.ft=l;
								writeout(n,usert);
								usert=readin(l);
								usert.ft=-1;
								writeout(l,usert);
								done=TRUE;
							}
							else
							{
								o=n;
								n=usert.ft;
							}
				}//end while not done
		}//end v(score)-1
	}//for l=2....
	return(p);
}

void computer(void)
{
	int16 prr;
	int16 s2;
	int16 n;
	char i[1024];

printf("\n<Computer>\n");
printf("<Computer activated>\n");
done=FALSE;
userr=readin(pn);
prr=userr.ff;
s2=prr+lp;
while((!hangup)&&(!done))
	{
	dump();
	tleft();
	printf("\nComputer Comand  (C,F,I,R,S,M,Q,?): ");
	memset(i,0x0,sizeof(i));
	fflush(stdin);
	i[0]=getchar();
	if(i[0]==' ')
		printf("? = Help");
	switch(i[0])
		{
		case '?': compmenu();break;
		case 'C': reportsec(s2);break;
		case 'F': findsec(prr);break;
		case 'I': info(pn);break;
		case 'R': rankings();break;
		case 'S': pchat(pn);break;
		case 'M': rsm();break;
		case 'Q': done=TRUE;break;
		}

	}
printf("\n<Computer deactivated>\n");
}


void otherport(int16 p2)
{
	int16 i;
	int16 ni[4];    //in pascal it starts at 1 while in C we start at 0!!
	int16 hi[4];

	h[0]=userr.fh;
	h[1]=userr.fi;
	h[2]=userr.fj;
	h[3]=userr.fk;

	for(i=1;i<4;i++)
	{
		ni[i]=n[i];
		hi[i]=h[i];     //In the pascal I think we are going from floats to ints... I think C can just 'do it'...
	}
	usert=readin(p2);
	printf("\n");
	printf("Commerce report for %s DATE TIME\n",usert.fa);
	printf(" Items     Status   # units  in holds\n");
	printf(" ~~~~~     ~~~~~~   ~~~~~~~  ~~~~~~~~\n");

	for(i=1;i<4;i++)
	{
		printf("%s",p[i]);
		if(c1[i]<0.0)
			printf(" Buying  ");
		else
			printf(" Selling ");
			printf("%d\t\t%d\n",ni[i],hi[i]);
	}

}


//Details about earth... it's the only place that sells ship stuff!
void port1(void)
{
	int16 mi[5];
	m[1]=50*sin(0.89756*d);
	m[2]=8*sin(0.89714*d+1.5707);
	printf("\n");
	m[1]=m[1]+500;
	m[2]=m[2]+100;
	m[3]=200-m[2];
	mi[1]=m[1];
	mi[2]=m[2];
	mi[3]=m[3];
	printf("Commerce report for sol: DATE TIME\n");
	printf("  Cargo holds :\t%d credits/hold\n",mi[1]);
	printf("  Fighters    :\t%d credits/fighter\n",mi[2]);
	printf("  Armor points:\t%d credits/point\n",mi[3]);
	printf("  Turns       :\t300 credits each.\n");
}
