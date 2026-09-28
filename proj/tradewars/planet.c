#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>


#include "tw4.h"

//L from the main menu kicks off the planet menu
//
void planet(void)
{
	int16 t;
	//int16 l2;  this is to be a 'global' var...
	int16 l;
	int16 m;
	//BOOL done;    While it's in it's own scope in pascal, in C it needs the global!!
	char i[1024];

	printf("\n<Land on a planet surface>\n");
	printf("Beginning landing maneuvers...\n");
	prr=userr.ff;
	s2=prr+lp;
	pd=userr.fb;
	usert=readin(s2);
	l=usert.fo;
	if(l==0)
	{
		printf("There isn't a planet in this sector.\n");
		printf("You can create on with a Genesis Torpedo.\n");
		printf("Torpedos cost 10,000 credits.\n");
		if(userr.fl<10000)
			printf("You're too poor to buy one.\n");
		else
		{
			printf("\nYou have %d credits.\n",userr.fl);
			printf("Do you wish to buy a torpedo (Y/N) [N]? ");
			if(yesno())
			{
				done=FALSE;
				l=lt1+1;
				while(!done || (l>ll1))
				{
					usert=readin(l);
					if(usert.fm==0)         //==0 by default! make it !=0 to overwrite an existing planet..
						done=TRUE;
					else
						l++;
				}
				if(!done)
				{
					printf("\nI'm sorry but not enough free matter exists.");
					printf("\nOne has to be destroyed before you can create one.\n");
				}
				else
				{
					char syslogtext[1024];
					//printf("gfiles\\genesis.msg");
					printfile("genesis.msg");
					printf("\nWhat do you want to name this planet? (41 chars. max)? ");
					memset(i,0x0,sizeof(i));
					fflush(stdin);

					fgets((char*)i,40,stdin);
					fflush(stdin);
					i[41]=0x0;
					usert=readin(l);
					strcpy(usert.fa,i);
					usert.fm=strlen(i);
					usert.fc=1;
					usert.fd=1;
					usert.fe=1;
					usert.ff=1;
					usert.fg=1;
					usert.fh=1;
					usert.fi=0;
					usert.fk=0;
					writeout(l,usert);
					usert=readin(s2);
					usert.fo=l-lt1;
					writeout(s2,usert);
					getdate();
					m=calcminutes();
					usert=readin(l);
					usert.fb=d;
					usert.fr=m;
					writeout(l,usert);
					userr.fl=userr.fl-10000;
					writeout(pn,userr);
					memset(syslogtext,0x0,sizeof(syslogtext));
					//this wont capture the entire planet name, need some strcat love.
					sprintf(syslogtext,"  -  %s made a planet: %s at sector %d\n",userr.fa,i,userr.ff);
					sysoplog(syslogtext);
				}//end create planet bit

			}//don't buy a torpedo..
		}
	}// I think this means there is a planet!
	else
	{
	//printf("is there a planet here?\n");
	l2=l+lt1;
	display();
	printf("\n");
	done=FALSE;
	while(!done)
		{
			printf("\nPlanet command (?=help)? ");
			fflush(stdin);
			i[0]=getchar();
			fflush(stdin);
			switch(i[0])
				{
			case 'A': takeall(l2);break;
			case 'D': destroy(l2);break;
			case 'I': increase(l2);break;
			case '1': takeore(l2);break;
			case '2': takeorg(l2);break;
			case '3': takeequ(l2);break;
			case '4': leaveore(l2);break;
			case '5': leaveorg(l2);break;
			case '6': leaveequ(l2);break;
			case 'L': done=TRUE;break;
			case 'R': display();break;
			case '?': planethelpit();break;
			case ' ': planethelpit();break;
			}
		}//end !done loop
	printf("leaving Planet...\n");
	}

}


//leave ORE on a planet
void leaveore(int16 l2)
{
	int16 o;
	char i[255];

	printf("\n<Leave ore>");
	usert=readin(l2);
	printf("\nHow much [%d]? ",userr.fi);
	memset(i,0x0,sizeof(i));
	fflush(stdin);
	scanf("%s",i);
	fflush(stdin);
	o=atoi(i);
	if(o>userr.fi)
		printf("You don't have that many.");
	else
	{
		if(o<0)
			o=0;
		userr.fi=userr.fi-o;
		writeout(pn,userr);
		usert.ff=usert.ff+o;
		writeout(l2,usert);
		printf("You left %d units of ore on the planet surface.",o);
	}
}

//leave ORGANICS on a planet
void leaveorg(int16 l2)
{
	int16 o;
	char i[255];

	printf("\n<Leave organics>");
	usert=readin(l2);
	printf("\nHow much [%d]? ",userr.fj);
	memset(i,0x0,sizeof(i));
	fflush(stdin);
	scanf("%s",i);
	fflush(stdin);
	o=atoi(i);
	if(o>userr.fj)
		printf("You don't have that many.");
	else
	{
		if(o<0)
			o=0;
		userr.fj=userr.fj-o;
		writeout(pn,userr);
		usert.fg=usert.fg+o;
		writeout(l2,usert);
		printf("You left %d units of organics on the planet surface.",o);
	}
}

//leave EQUIPMENT on the planet...
void leaveequ(int16 l2)
{
		int16 o;
	char i[255];

	printf("\n<Leave equipment>");
	usert=readin(l2);
	printf("\nHow much [%d]? ",userr.fk);
	memset(i,0x0,sizeof(i));
	fflush(stdin);
	scanf("%s",i);
	fflush(stdin);
	o=atoi(i);
	if(o>userr.fk)
		printf("You don't have that many.");
	else
	{
		if(o<0)
			o=0;
		userr.fk=userr.fk-o;
		writeout(pn,userr);
		usert.fh=usert.fh+o;
		writeout(l2,usert);
		printf("You left %d units of equipment on the planet surface.",o);
	}
}


void takeore(int16 l2)
{
	int16 o;
	char i[255];

	printf("\n<Take ore>");
	printf("\nHow much [%0.2f]? ",n[1]);
	memset(i,0x0,sizeof(i));
	fflush(stdin);
	scanf("%s",i);
	fflush(stdin);
	o=atoi(i);
	if(o>n[1])
		printf("They don't have that many.");
	else
	if((h[0]-h[1]-h[2]-h[3])<0)
		printf("You don't have enough free cargo holds.\n");
	else
	{
		userr.fi=userr.fi+o;
		writeout(pn,userr);
		usert=readin(l2);
		usert.ff=usert.ff-o;
		writeout(l2,usert);
		h[1]=h[1]+o;
		n[1]=n[1]=o;
	}
}

void takeorg(int16 l2)
{
	int16 o;
	char i[255];

	printf("\n<Take organics>");
	printf("\nHow much [%0.2f]? ",n[2]);
	memset(i,0x0,sizeof(i));
	fflush(stdin);
	scanf("%s",i);
	fflush(stdin);
	o=atoi(i);
	if(o>n[2])
		printf("They don't have that many.");
	else
	if((h[0]-h[1]-h[2]-h[3])<0)
		printf("You don't have enough free cargo holds.\n");
	else
	{
		userr.fj=userr.fj+o;
		writeout(pn,userr);
		usert=readin(l2);
		usert.fg=usert.fg-o;
		writeout(l2,usert);     //write the planet to disk
		h[2]=h[2]+o;    //update the hold
		n[2]=n[2]=o;    //update the planet variables...
	}
}
void takeequ(int16 l2)
{
	int16 o;
	char i[255];

	printf("\n<Take equipment>");
	printf("\nHow much [%0.2f]? ",n[3]);
	memset(i,0x0,sizeof(i));
	fflush(stdin);
	scanf("%s",i);
	fflush(stdin);
	o=atoi(i);
	if(o>n[3])
		printf("They don't have that many.");
	else
	if((h[0]-h[1]-h[2]-h[3])<0)
		printf("You don't have enough free cargo holds.\n");
	else
	{
		userr.fk=userr.fk+o;
		writeout(pn,userr);
		usert=readin(l2);
		usert.fh=usert.fh-o;
		writeout(l2,usert);     //write the planet to disk
		h[3]=h[3]+o;    //update the hold
		n[3]=n[3]=o;    //update the planet variables...
	}
}

//we just go through the list of planetary goods
//and look at available space, and take all the free ones...
void takeall(int16 l2)
{
	int16 f;        //cargo capacity
	int16 a;        //ammount to take....
	printf("\n<Take all>\n");
	f=h[0]-h[1]-h[2]-h[3];
	a=n[3];         //equipment on planet
	if(f==0)
	{
		printf("\nYou don't have any free holds.\n");
	}
	else
	{
		if(f<a)
			a=f;
		if(f!=0)                                        //start with equipment
		{
			f=f-a;
			h[3]=h[3]+a;
			n[3]=n[3]-a;
			userr.fk=userr.fk+a;    //add the 'a' to the equipment
			writeout(pn,userr);             //save the user
			usert=readin(l2);               //load the planet
			usert.fh=usert.fh-a;    //deduct the equipment
			writeout(l2,usert);             //save the planet
			printf("You took %d holds of equipment.\n",a);
			a=n[2];                                 //setup a for organics..?
			if(f==0)
			{
				printf("Your holds are filled.\n");
			}//hold is full
			else
			{
				if(f<a)
				a=f;
				if(f!=0)                //start with Organics
				{
				f=f-a;
				h[2]=h[2]+a;
				n[2]=n[2]-a;
				userr.fj=userr.fj+a;    //add the 'a' to the Organics
				writeout(pn,userr);             //save the user
				usert=readin(l2);               //load the planet
				usert.fg=usert.fg-a;    //deduct the Organics
				writeout(l2,usert);             //save the planet
				printf("You took %d holds of Organics.\n",a);
				a=n[1];
					if(f==0)
					{
						printf("Your holds are filled.\n");
					}//hold is full
//                              }//end organic else
				else
				{
					if(f<a)
						a=f;
				if(f!=0)                //start with Ore
				{
				f=f-a;
				h[1]=h[1]+a;
				n[1]=n[1]-a;
				userr.fi=userr.fi+a;    //add the 'a' to the Ore
				writeout(pn,userr);             //save the user
				usert=readin(l2);               //load the planet
				usert.ff=usert.ff-a;    //deduct the Ore
				writeout(l2,usert);             //save the planet
				printf("You took %d holds of Ore.\n",a);
				}//end of ore else
					}}}
		}//end equipment grab
	}//end you had free holds
}		//end of takeall..

//      Destroy the planet in l2
//  User needs 50+ fighters.
//
void destroy(int16 l2)
{
	printf("\n*** DESTROY THE PLANET ***\n");
	printf("\nConfirmed??? (Y/N)[N]? ");
	if(yesno())
	{
		if(userr.fg>=50)        //does the user have more then 50 fighters?
		{
			char syslogtext[1024];
			usert=readin(l2);
			usert.fm=0;
			writeout(l2,usert);
			usert=readin(s2);
			usert.fo=0;
			writeout(s2,usert);
			memset(syslogtext,0x0,sizeof(syslogtext));
			sprintf(syslogtext,"  -  %s destroyed the planet in sector %d\n",userr.fa,s2-lp);
			sysoplog(syslogtext);
			ssm(pn,"The Imperial Command for Interstellar Trade has sent this message:");
			ssm(pn,"   Do that again and we will dispatch War Rocket Ajax to bring");
			ssm(pn,"   back your body!");
			printf("\n");
			printfile("geneboom.msg");
			done=TRUE;
			userr.fg=userr.fg-30;           //it costs 30 fighters to do so...
			writeout(pn,userr);
		}//end more then 50 fighters...
		else
		{
			printf("Your computer has forcasted a loss of 25-40 K3-A fighters and\n");
			printf("has aborted the mission.\n");
			done=TRUE;
		}//end less then 50 fighters...
	}//end yes we are destroying the planet
}

//increases a planet's output...
void increase(int16 l2)
{
	int16 a1;               //holds some secondary 'answer..'..
	int16 b;                //temp credit buffer..
	char i[256];    //keyboard buffer....

	printf("\n<Increase productivity>\n");
	printf("you have %d credits.\n",userr.fl);
	printf("1 - mining Ore costs 500\n");
	printf("2 - growing Organics costs 700\n");
	printf("3 - heavy machinery & Equipment production costs 100.\n");
	printf("\nWhich one do you want to increase (1,2,3)? ");
	memset(i,0x0,sizeof(i));
	scanf("%s",i);
	a=atoi(i);
	if( (a>=1) && (a<4) )
	{
		if(a==1)        //lets mine!
		{
			if(pub[1]>9)    //only can have a 9 mine thing
				printf("It's at its maximum value\n");
			else
			{
				printf("Ore: Increase by how many units? ");
				memset(i,0x0,sizeof(i));
				scanf("%s",i);
				a1=atoi(i);
				if(a1>=1)
					if(a1+pub[1]>10)
						printf("The most it can be is 10 units.\n");
					else
					{
						b=userr.fl;
						if(a1*500>b)    //lovingly hard coded to 500 credits
							printf("You're too poor.  You only have %d credits.\n",userr.fl);
						else
						{
							userr.fl=b-500*a1;              //deduct credits
							writeout(pn,userr);             //save the user
							usert=readin(l2);               //read the planet
							usert.fc=usert.fc+a1;   //increase planetary production
							writeout(l2,usert);             //save the planet
						}//end the else
					}//end else for enough units&cash


			}//increasing ore else..
		}//end of the mining choices...
		if(a==2)
		{
		if(pub[2]>19)   //only can have a 20 bio thing
			printf("It's at its maximum value\n");
			else
			{
				printf("Organics: Increase by how many units? ");
				memset(i,0x0,sizeof(i));
				scanf("%s",i);
				a1=atoi(i);
				if(a1>=1)
					if(a1+pub[2]>20)
						printf("The most it can be is 20 units.\n");
					else
					{
						b=userr.fl;
						if(a1*700>b)    //lovingly hard coded to 700 credits
							printf("You're too poor.  You only have %d credits.\n",userr.fl);
						else
						{
							userr.fl=b-700*a1;              //deduct credits
							writeout(pn,userr);             //save the user
							usert=readin(l2);               //read the planet
							usert.fd=usert.fd+a1;   //increase planetary production
							writeout(l2,usert);             //save the planet
						}//end the else
					}//end else for enough units&cash


			}//increasing Organics else..
		}//end of the Oranics section
		if(a==3)
		{
			if(pub[3]>29)   //only can have a 30 equipment thing
				printf("It's at its maximum value\n");
			else
			{
				printf("Equipment: Increase by how many units? ");
				memset(i,0x0,sizeof(i));
				scanf("%s",i);
				a1=atoi(i);
				if(a1>=1)
					if(a1+pub[3]>30)
						printf("The most it can be is 30 units.\n");
					else
					{
						b=userr.fl;
						if(a1*100>b)    //lovingly hard coded to 100 credits
							printf("You're too poor.  You only have %d credits.\n",userr.fl);
						else
						{
							userr.fl=b-100*a1;              //deduct credits
							writeout(pn,userr);             //save the user
							usert=readin(l2);               //read the planet
							usert.fe=usert.fe+a1;   //increase planetary production
							writeout(l2,usert);             //save the planet
						}//end the else
					}//end else for enough units&cash
			}//increasing Equipment else..
		}//end of the Equipment section
	}//end valid options

}


void planethelpit(void)
{
	printf("\n");
printf("1 - take ore\n");
printf("2 - take organics\n");
printf("3 - take eguipment\n");
printf("4 - leave ore\n");
printf("5 - leave organics\n");
printf("6 - leave eguipment\n");
printf("A - take <A>ll\n");
printf("D - <D>estroy the planet\n");
printf("I - <I>ncrease productivity\n");
printf("L - <L>eave planet\n");
printf("R - planet <R>eport\n\n");
}

void pchat(int16 p)
{
	int16 po;
	char pname[1024];
	char pmsg[1024];

	printf("\n<Warming up the Chambers' coil>\n");
	printf("Send a message to player # ");
	fflush(stdin);
	memset(pname,0x0,sizeof(pname));
	scanf("%s",pname);
	fflush(stdin);
	po=atoi(pname);

	if( (po>1) && (po<lp) )
	{
		char tstring[1024];
		printf("\nEnter your message now, up to 160 characters, <CR> to end:\n");
		memset(pmsg,0x0,sizeof(pmsg));
		fflush(stdin);
		fgets((char*)pmsg,160,stdin);
		memset(tstring,0x0,sizeof(tstring));
		sprintf(tstring,"   %s sent a message: ",userr.fa);
		ssm(po,tstring);
		ssm(po,pmsg);
		printf("Message transmitted to %s\n",pname);
	}
	else
		printf("Not an active player\n");

}

void upplanet(int16 s2)
{
	int16 l;
	int16 c;
	int16 l2;
	int16 mn;       //minutes in the day so far.
	double dim;

	usert=readin(s2);
	if(usert.fo!=0)
	{
		l2=usert.fo+lt1;
		h[0]=userr.fh;
		h[1]=userr.fi;
		h[2]=userr.fj;
		h[3]=userr.fk;
		usert=readin(l2);
		n[1]=usert.ff+usert.fi/10000;
		n[2]=usert.fg+usert.fj/10000;
		n[3]=usert.fh+usert.fk/10000;
		pub[1]=usert.fc;        //produced ore
		pub[2]=usert.fd;        //produced organics
		pub[3]=usert.fe;        //produced equipment
		getdate();
		c=d;
		mn=calcminutes();
		dim=d-usert.fb+(mn-usert.fr)/1440;
		if(dim<0)
			dim=0;
		else
			if(dim>10)
				dim=10;
		for(l=1;l<4;l++)
		{
			if(n[l]<pub[l]*10)
				n[l]=n[l]+pub[l]*dim;
			if(n[l]>=pub[l]*10)
				n[l]=n[l]+pub[l]*dim/10;
		}
	}
	usert=readin(l2);
	usert.fb=c;
	usert.ff=n[1];
	usert.fg=n[2];
	usert.fh=n[3];
	for(l=1;l<4;l++)
	{
		srr[l][0]=(n[l]-n[l])*10000+0.5;
		n[l]=n[l];
	}
	usert.fi=srr[1][0];
	usert.fj=srr[2][0];
	usert.fk=srr[3][0];
	usert.fr=mn;
	writeout(l2,usert);
}