#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "tw4.h"



//Attack is called from the enterroom menu
//s2 sector
//f2 friendly fighters
//e2 enemy fighters
void attack(int16 s2, int16 f2, int16 e2)
{
	char i[1024];
	int16 n;        //number of fighters to commit
	int16 l;        //friendly lost
	int16 k;        //enemy killed

	printf("\n<Attack>\n");
	if(f2<1)
	{
		printf("\nYou don't have any fighters!\n");
	}
	else
	{
		printf("How many fighters do you wish to use? [%d] ",f2);
		fflush(stdin);
		memset(i,0x0,sizeof(i));
		scanf("%s",i);
		n=atoi(i);
		if(( n>=1) && (n<=9999) )       //lets do this!  also 9999 fighter max hard coded!
		{
			l=0;    //friendly lost thus far
			k=0;    //enemies killed
			if(n>f2)
			{
				printf("You don'thave that many fighters.\n");
			}
			else
			{
				while (  (l<n) && (k<e2) )
					if(TWrandom(2)==1)            //coin flip!
						l=l+1;          //current losses
					else
						k=k+1;
				f2=f2-l;
				e2=e2-k;
				userr.fg=f2;            //update fighters
				writeout(pn,userr);     //write it out
				if(usert.fm>1)          //we are attacking a user, so let them know!
				{
					char message[256];
					memset(message,0x0,sizeof(message));
					sprintf("%s attacked %d of your fighters in sector %d\n",userr.fa,k,userr.ff);
					ssm(usert.fm,message);
					//ssm(usert.fm,userr.fa+' attacked '+cstr(k)+' of your fighters in sector '+cstr(userr.ff));
				}
				usert.fl=e2;
				writeout(s2,usert);
				if(e2<1)                //no more enemy fighters
				{
					usert.fl=0;
					usert.fm=0;             //clear & save sector
					writeout(s2,usert);
				}
				printf("\nYou lost %d fighter(s)\n",l);
				printf("You destroyed %d enemy fighters.\n",k);
				if(e2<=0)
				{
					printf("You destroyed all the fighters.\n");
					done=TRUE;
				}
				else
					done=FALSE;
			}//you do have enough fighters
		}//end of start the fight!
	}

}



//from TWOVER.PAS

void tw4kill(void)
{
	int16 p;
	int16 n;
	int16 e2;
	int16 e3;
	int16 f2;
	int16 l1;
	int16 la;
	int16 k1;
	char i[1024];


	printf("\n<Attack>\n");
	usert=readin(s2);
	a=usert.fi;
	usert=readin(a);
	if(usert.fo==0)
	{
		printf("There are no ships to attack.\n");
	}
	else
		{
			f2=userr.fg;
			la=a;
			if(f2<1)
			{
				printf("You don`t have any fighters.");
			}
			else
			{
			done=FALSE;
			a=usert.fo;
			while(!done && !a==0)
				{
					usert=readin(a);
					printf("\nAttack %s (Y/N)[n]? ",usert.fa);
					if(yesno())
					{p=a;done=TRUE;}
					else
						a=usert.fo;
				}
			if(!done)
				printf("\nThere are no other ships in this sector.");
			else
				{
					printf("\nHow many fighters do you wish to use [0]? ");
					scanf("%s",i);
					n=atoi(i);
					if ( (n>=1) || (n<=9999))
					{
						usert=readin(p);
						e2=usert.fg;            //Enemy fighters on enemy ship
						e3=usert.fe;            //Enemy ship armor
						l1=0;                           //Friendly fighters lost
						k1=0;                           //Enemy fighters lost
						if(n>f2)
							printf("You don't have that many fighters.\n");
						else
						{
							if (usert.fe>0)
								printf("Your fighters encounter some armor & Shielding as they attack the other ship.");
							while ( !(l1>=n) && !(k1>e2) )
							{
								if(  (TWrandom(2)) ==1)       //basic coinflip I guess.
									l1=l1+1;        //Friendly loss
								else
								{
									if(e3>0)        //Check for armor on enemy ship
										e3--;
									else
										k1=k1+1;        //Lost Enemy fighters goes up
								}
							}       //end while going through the fighters.....
						}//end of the else loop for having enough fighters.
						if((k1>0 || e3<usert.fe))
							message(p,pn,k1,usert.fe-e3);
						usert=readin(p);
						userr.fg=f2-l1;
						usert.fg=e2-k1;
						usert.fe=e3;
						writeout(pn,userr);
						writeout(p,usert);
						printf("\nYou lost %d fighter(s), %d remain.",l1,f2-l1);
						if(e2>k1)
						{
							printf("You destroyed %d enemy fighters, %d remain.",k1,e2-k1);
						}
						else
						{
							salvage(pn,p);  //gut the enemy for parts
							killed(pn,p);   //mark the death...
						}

					}
				}
			}
		}
}


void salvage(int16 pn, int16 p)
{
	int16 b;
	int16 c;
	int16 d;
	int16 e;
	int16 f;
	int16 g;
	int16 h;
	int16 i;
	int16 l;
	int16 v;

	usert=readin(p);
	a=usert.fh/4+1;
	if(a+userr.fh>150)
	{
		a=150-userr.fh;
	}
	if(a<1)
	{
		printf("Excellent kill!\n");
		printf("...In fact, TOO excellent!  You can't salvage anything from it!\n");
	}
	else
	{
		b=0;
		c=0;
		d=0;
		e=0;
		f=usert.fi;
		g=usert.fj;
		h=usert.fk;
		i=usert.fh;
		l=1;

		while(l<a)
		{
			v=TWrandom(i);
			if(v<f)
			{
				b=b+1;
				f=f-1;
			}
			else
				if(v<f+g)
				{
					c=c+1;
					g=g-1;
				}
				else
					if(v<f+g+h)
					{
						d=d+1;
						h=h-1;
					}
					else
						e=e+1;
			i=i-1;
			l++;
		}
		userr.fh=userr.fh+b+c+d+e;
		userr.fi=userr.fi+b;
		userr.fj=userr.fj+c;
		userr.fk=userr.fk+d;
		writeout(pn,userr);
		printf("You destroyed the ship and slvaged these cargo holds:\n");
		if(e>0)
			printf("    %d empty\n",e);
		if(b>0)
			printf("    %d with ore\n",b);
		if(c>0)
			printf("    %d with organics\n",c);
		if(d>0)
			printf("    %d with equipment\n",d);
		printf("\n");
	}
}

void fighters()
{
	int16 d2;
	int16 f2;
	int16 n;
	int16 l;
	int16 b;
	char i[1024];


	printf("\n<Drop/Take Fighters>\n");
	if(prr<2)                                       //if the sector is less then 2, you'll make superman mad!
	{
		printfile("kentmad.msg");
		userr.fh=userr.fh-1;    //lose a cargo hold
		userr.fg=userr.fg*0.9;  //and 10% of your fighters
		writeout(pn,userr);             //save this to disk...
	}
	else
	{
		usert=readin(s2);
		d2=usert.fl;
		f2=userr.fg;
		printf("You have %d fighters available.\n",f2+d2);
		printf("How many fighters do you want defending this sector? ");
		memset(i,0x0,sizeof(i));
		scanf("%s",i);
		n=atoi(i);
		if(n>=0)
		{
			l=n;
			b=f2+d2-l;
			if(b<0)
			{
				printf("You don't have that many ships available\n");
			}
			else
				if(b>9999)
					printf("Too many ships in your fleet!  You are limited to 9999\n");
				else
				{
					usert.fl=l;
					usert.fm=pn*sgn(l);
					writeout(s2,usert);
					userr.fg=b;
					writeout(pn,userr);
					printf("Done. You have %d fighter(s) in close support.\n",b);
				}
		}
	}
}


//Called from enterroom when there are other fighters there.
void retreat(void)
{
	int16 lr;
	printf("\n<Retreat>\n");
	lr=userr.fq;    //last sector..
	while( (lr==0) && (lr==prr) )
	{
		lr=e[TWrandom(6)];
	}
	if(userr.fg>=1) //user has some fighters
	{
		printf("Your fighter pilots make a valiant attempt to stall the oncoming\n");
		printf("horde.\n");
		removeship(pn);
		userr.fg=userr.fg-1;    //lose a fighter
		userr.ff=lr;                    //last location
		userr.fq=prr;                   //prior location!?
		writeout(pn,userr);
		addship(pn);
		lr=a;
		done=TRUE;
	}//end fighter loss
	else
		if(userr.fe>4)  //user has some armor
		{
			printf("The on-coming horde is fast & powerful, but your ship armor held...\n");
			printf("...this time...\n");
			removeship(pn);
			userr.fe=userr.fe-5;            //lose 5 armors!
			if(userr.fe<0)                          //negative armor...
				userr.fe=0;
			userr.ff=lr;
			userr.fq=prr;
			writeout(pn,userr);
			addship(pn);
			lr=a;
			done=TRUE;
		}//end armor loss
		else
			if(TWrandom(2)==1)
			{
			printf("Lucky guy!  You escaped by the hair on your chinny-chin-chin!\n");
			removeship(pn);
			userr.fe=0;     //no more armor!
			userr.ff=lr;
			userr.fq=prr;
			writeout(pn,userr);
			addship(pn);
			lr=a;
			done=TRUE;
			}//end lucky guy
			else
			{
				printf("A fitting fate for you, coward: you didn''t escape!\n");
				destroyed();
			}//cowards death
	prr=userr.ff;
	s2=prr+lp;
	usert=readin(s2);
	e[1]=usert.fb;  //last on
	e[2]=usert.fc;  //killed by
	e[3]=usert.fd;  //turns remaining
	e[4]=usert.fe;  //armor
	e[5]=usert.ff;  //location
	e[6]=usert.fg;  //fighters
	printf("\n");

}



//drop a mine
void minedrop(void)
{
	userr=readin(pn);
	printf("\nDo you really want to drop a mine in this sector?");
	if(yesno())
	{
		printf("\n");
		if(userr.ff>9)  //outside sector 9...
		{
			if( (userr.fh<2) && (userr.fg<6) )   //2 cargo holds & 6 fighters needed!!
				printf("You don''t have enough resources aboard to build a mine.\n");
			else
			{
				printfile("twmine.msg");
				userr.fg=userr.fg-5;    //expend 5 fighters!
				userr.fh=userr.fh-2;    //expend 2 cargo holds!
				if(userr.fh<(userr.fi+userr.fj+userr.fk))       //if your holds space is less then the cargo..
					if(userr.fi>1)                          //jettison some ORE
						userr.fi=userr.fi-2;
					else
						if(userr.fj>1)                  //otherwise jettison some organics
							userr.fj=userr.fj-2;
						else
							if(userr.fk>1)          //then some equipment.
								userr.fk=userr.fk-2;
				writeout(pn,userr);
				usert=readin(userr.ff+lp);              //readin the sector data.
				usert.fp=usert.fp+1;                    //add a mine to the sector
				if(usert.fp>20)
					usert.fp=20;                            //maximum 20 mines per sector!
				writeout(userr.ff+lp,usert);
			}//build a mine.
		}//end sector9 requirement
		else
		{
			printf("The Imperial Navy does not like people mining the home quadrant...\n");
		}
	}//end yesno
}
