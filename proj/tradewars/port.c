#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "tw4.h"

//This was 'port' in the pascal version but I renamed it so as not to conflict.
//
void spaceport(void)
{
	int16 st;
	int16 r;
//      int16 p2;       another conflict...!!!!
	int16 c2;       //holds user credits for shopping in sector 1..
//      int16 f2; also conflicting.
//      int16 l2; conflicts with global l2... so let's try the global one.
	int16 m2;
//      int16 a;  conflicts with a global 'a'.. and would wipe out cash in donno..
	int16 nuh;
	BOOL doneit;
	char i[1024];

	printf("\n<Port>\n");
	printf("Beginning docking procedure...\n");
	a=userr.fd;                                     //here a gets how many turns remaing..
	if(a<1)
		printf("\nYou don't have any turns left.\n");
	else
	{
		usert=readin(s2);
		st=usert.fh;
		p2=st+ls;
		if(st==0)
			printf("\nThere are no ports in this sector.\n");
		else
		{
#ifdef DEBUG
//DEBUG we add turns!!!
userr.fd=a+1;
userr.fl=userr.fl+300;
#else
			userr.fd=a-1;
#endif
			writeout(pn,userr);
			printf("\nOne turn deducted\n");
			usert=readin(p2);
				if(prr!=1)      //what sector is this?
				{
					upport(userr.ff+lp);
					h[0]=userr.fh;
					h[1]=userr.fi;
					h[2]=userr.fj;
					h[3]=userr.fk;
					otherport(p2);
					printf("\n");
					f2=0;
					for(l2=1;l2<4;l2++)
						if(c1[l2]<0)            //sell all on the ship
							trade(l2);
					for(l2=1;l2<4;l2++)
						if(c1[l2]>0)            //buy all the ship will hold
							trade(l2);
					if(f2==0)
						printf("You don''t have anything they want, and they don't have anything you can buy\n");
					printf("\n");
					printf("You have %d credits and %0.2f empty cargo holds.\n",userr.fl,h[0]-h[1]-h[2]-h[3]);
				}//prr!=1
				else
				{
					port1();
					c2=userr.fl;
					done=FALSE;
					printf("You have %d credits\n",c2);
					while(!done)
					{
						done=TRUE;
						printf("How many holds do you want to buy [0]? ");
						fflush(stdin);
						memset(i,0x0,sizeof(i));
						scanf("%s",i);
						a=atoi(i);
						if(a<0)
						{
							printf("\nWhat??\n");
							done=FALSE;
						}
						if(m[1]*a>c2)
						{
							printf("You don't have enough money. The maximum amount you can buy is %0.0f\n",c2/m[1]);
							done=FALSE;
						}
						if(a+userr.fh>150)      //maximum 150 holds......
						{
							printf("You are limited to 150 cargo holds.\n");
							done=FALSE;
						}
					}
					if(a>0)                                 //did we actually buy something?
					{
					c2=userr.fl;
					userr.fh=userr.fh+a;    //add the holds
					c2=c2-m[1]*a;                   //deduct temp credits
					userr.fl=c2;                    //make it 'real'
					writeout(pn,userr);             //write out the record.
					}

					done=FALSE;
					printf("You have %d credits\n",c2);
					while(!done)
					{
						done=TRUE;
						printf("How many K-3A fighters do you want to buy [0]? ");
						fflush(stdin);
						memset(i,0x0,sizeof(i));
						scanf("%s",i);
						a=atoi(i);
						if(a<0)
							done=FALSE;
						if(m[2]*a>c2)
						{
							printf("You don't have enough money. The maximum amount you can buy is %0.0f\n",c2/m[2]);
							done=FALSE;
						}
						if(a+userr.fg>9999)     //maximum 9999 fighters......
						{
							printf("Your squadron is limited to 9999 fighters.\n");
							done=FALSE;
						}
					}
					if(a>0)                                 //did we actually buy something?
					{
					c2=userr.fl;
					userr.fg=userr.fg+a;    //add the fighters
					c2=c2-m[2]*a;                   //deduct temp credits
					userr.fl=c2;                    //make it 'real'
					writeout(pn,userr);             //write out the record.
					}

					done=FALSE;
					printf("You have %d credits\n",c2);
					while(!done)
					{
						done=TRUE;
						printf("How many armor points do you want to buy [0]? ");
						fflush(stdin);
						memset(i,0x0,sizeof(i));
						scanf("%s",i);
						a=atoi(i);
						if(a<0)
							done=FALSE;
						if(m[3]*a>c2)
						{
							printf("You don't have enough money. The maximum amount you can buy is %0.0f\n",c2/m[3]);
							done=FALSE;
						}
						if(a+userr.fe>200)      //maximum 200 armor points......
						{
							printf("Your ship is structurally limited to 200 points.\n");
							done=FALSE;
						}
					}
					if(a>0)                                 //did we actually buy something?
					{
					c2=userr.fl;
					userr.fe=userr.fe+a;    //add the armor
					c2=c2-m[3]*a;                   //deduct temp credits
					userr.fl=c2;                    //make it 'real'
					writeout(pn,userr);             //write out the record.
					}

					//lovingly hard coded...
					printf("You have %d credits\n",c2);
					done=FALSE;
					while(!done)
					{
						done=TRUE;
						printf("How many turns you want to buy [0]? ");
						fflush(stdin);
						memset(i,0x0,sizeof(i));
						scanf("%s",i);
						a=atoi(i);
						if(a<0)
							done=FALSE;
						if(300*a>c2)
						{
							printf("You don't have enough money. The maximum amount you can buy is %d\n",c2/300);
							done=FALSE;
						}
					}
					if(a>0)                                 //did we actually buy something?
					{
					c2=userr.fl;
					userr.fd=userr.fd+a;    //add the turns
					c2=c2-300*a;                    //deduct temp credits
					userr.fl=c2;                    //make it 'real'
					writeout(pn,userr);             //write out the record.
					}
				}
		}//there ARE ports in the sector...
	}//you do have turns left!!

}


void trade(int16 l2)
{
	int16 v;
	int16 huh;      //# of empty cargo holds
	int16 b;
	int16 muh;
	char dim[10];
	char eim[10];
	char i[10];

	m2=userr.fl;
	huh=h[0]-h[1]-h[2]-h[3];

	//sort out if we are buying or selling...?
	if(c1[l2]>0)
	{
		sprintf(dim,"buy");
		sprintf(eim,"sell");
		b=-1;
	}
	else
	{
		sprintf(dim,"sell");
		sprintf(eim,"buy");
		b=0;
	}
	if(b==-1)
	{
		muh=huh;
		if(muh>n[l2])
			muh=n[l2];
		if(muh*m[l2]>m2)
			muh=m2/m[l2];
	}
	if(b==0)
	{
		muh=n[l2];
		if(muh>h[l2])
			muh=h[l2];
	}
	if(muh!=0)
	{
		done=FALSE;
		while(!done && !hangup)
		{
			printf("\n\nYou have %d credits and %d empty cargo holds.\n",m2,huh);
			printf("We are %sing up to %0.0f",eim,n[l2]);
			printf(".  You have %0.0f in your holds",h[l2]);
			f2=1;
			printf("\nHow many holds of ");
			if(l2==1)
				printf("Ore");
			else
				if(l2==2)
					printf("Organics");
				else
					printf("Equipment");
			printf(" do you want to %s [%d]? ",dim,muh);
			memset(i,0x0,sizeof(i));
			scanf("%s",i);
			nuh=atoi(i);
			if(nuh==0)
				done=TRUE;
			if(nuh>=1)
				if( (b==-1) && (nuh>huh) )
					printf("You don't have enough cargo holds.\n");
				else
					if( (b==-1) && (nuh>n[l2]) )
						printf("They're not selling that many.\n");
					else
						if( (b==0) && (nuh>n[l2]) )
							printf("They don't want that many.\n");
						else
							if( (b==0) && (nuh>h[l2]) )
								printf("You don't have that many in your holds.\n");
							else
								done=TRUE;
		}//end while!done loop
		if(( nuh>=1) && (!hangup) )
		{
			printf("Agreed, %d units.\n",nuh);
			v=TWrandom(3);
			r=1;
			doneit=FALSE;

		while((r<v+1)) //UNTIL HANGUP OR DONEIT OR (R>V+1);
			{
			printf("\n");
			if(r==v+1)
				printf("\nOurfinal offer is ");
			else
				if(b==-1)
					printf("\nWe'll sell them for ");
				else
					printf("\nWe'll buy them for ");
			printf("%0.0f credits.",nuh*m[l2]*(1+c1[l2]/1000)+0.5);
			done=FALSE;
			while(!done)//while(!hangup && !done)
				{
					done=TRUE;
					printf("\nYour offer? ");
					scanf("%s",&i);
					a=atoi(i);
					if(a==0)
						{doneit=TRUE;r=r+v;}
					if((b==-1) && (userr.fl<m[l2]*nuh/10) )
					{
						printf("You don't have enough cash to backup this deal.\n");
						done=TRUE;
					}
					else
						if( (a<m[l2]*nuh/10) || (a>m[l2]*nuh*10) )
						{
							printf("\nImperial Intelligence frowns upon those who are too flippant. Make a SERIOUS offer...\n");
							done=FALSE;
						}
						if( (a>m2) && (b==-1) )
						{
							printf("  You only have %d credits!\n",m2);
							done=FALSE;
						}
				}//end hangup/done inner while
			if( (b==0) && (a<m[l2]*nuh) )
			{
				printf("Agreed!  We'll PURCHASE them!\n");
				dunno2();
				doneit=TRUE;
				done=TRUE;
				r=r+v;
			}
			else
				if( (b==-1) && (a>=m[l2]*nuh) )
				{
					printf("\nSold!\n");
					dunno2();
					doneit=TRUE;
					done=TRUE;
					r=r+v;
				}
				else
				{
					double t;       //why not..

					t=nuh*m[l2]*((1-c1[l2]/250/r)+0.5);
					if((b==0) || (a>t))
					{
						idunno();       //wait.. but we completed the transaction?!
						printf("\nToo high.  We'll buy them from the Orion Traders.\n");
						doneit=TRUE;
						r=r+v;
					}
					else
						if( (b==-1) || (a<t) )
						{
							idunno();       //wait.. but we completed the transaction?!
							printf("\nToo low.  We'll scap them to the Federation.\n");
							doneit=TRUE;
							r=r+v;
						}
						m[l2]=0.7*m[l2]+0.3*a/nuh;
				}
				r=r+1;
			}//end while r>v+1 loop
		}
	}//end muh!=0

}


//this is called from the trade program.. it looks to me like it places stuff into the ship
//and writes out the record.
void dunno2(void)
{
	int16 s;
	s=sgn(c1[l2]);
	userr.fl=m2-(s*a);                      //the a is zero...
	if(l2==1)
		userr.fi=h[1]+s*nuh;            //ore?
	else
		if(l2==2)
			userr.fj=h[2]+s*nuh;    //organics?
		else
			userr.fk=h[3]+s*nuh;    //equipment?
	writeout(pn,userr);
	idunno();
	h[l2]=h[l2]+s*nuh;
}

//This looks to me like it writes out the sellers reduced inventory...
//Called from dunno2
//references the p2 global....
void idunno(void)
{
	//printf("idunno %d\n",p2);
	usert=readin(p2);
	if(l2==1)
		usert.fd=usert.fd-nuh;//usert.fd=n[1]-nuh;
	else
		if(l2==2)
			usert.fe=usert.fe-nuh;//usert.fe=n[2]-nuh;
		else
			usert.ff=usert.ff-nuh;//usert.ff=n[3]-nuh;
	writeout(p2,usert);
}
