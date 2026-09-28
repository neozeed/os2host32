//
//
//maintenence needs to be finished test...
//random numbers need to be... random.
//
//#define DEBUG
//http://www.classic-games.com/tradewars/download.html
//http://www.macdonald.egate.net/CompSci/Pascal/hdatatypes.html#real

//#define MICROSOFT
//#define               WIN16
//#define				Win32
//#define				Win64
#define				MSDOS


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
//screen IO stuff..
//getchar for now..
#ifdef MICROSOFT
#include <conio.h>
#else

#endif

//32bit compiler
typedef short int16;
typedef long  int32;
typedef long BOOL;

/**** GLOBALS *****/
//Booleans
BOOL hangup; /*has the user hungup*/
BOOL ended;  /*has the user ended the round*/
BOOL incom;  /*Our connection on a COM port?*/
BOOL so;          /*no idea*/
BOOL okansi; /*I guess you are ANSI term?*/
BOOL cs;          /*no idea*/
BOOL done;

#define TRUE 1
#define FALSE 0

//constants
char fs[] = "twdata.dat";



//Integers
int16 usernum;  //The user number in the dat files passed in from the bbs
int16 ay;
int16 tt;
int16 lp;               //planet offset into the database
int16 ls;               //sector offset into the database
int16 lt1;
int16 ll1;
int16 d; //This holds the date NUMBER as a global...
int16 y;
int16 a;
int16 mo;
int16 go;
int16 pn;       //player number?
int16 pd;       //this gets assigned the value of d.. for the date again?
int16 s2;
int16 st;
int16 g2;
int16 prr;
int16 e[6];     //not sure what this array is for...
int16 b[] = {10,20,35};
int16 f2;
int16 e2;
int16 r1;       //this get set to a random number @ some point.
int16 l2;       //from the planet functions... should be global.
int16 s[201][2];        //used to calculate shortest paths... I *THINK* the 200 comes hardcoded here for the # of sectors...!
int16 g[9][1];          //something to do with romulans?

//Thse trading vars should be ... localized.
int16 m2;       //part of trading.. holds empty cargo holds.
int16 nuh;      //another part of trading.
int16 r;        //another part of trading....
int16 p2;       //for the idunno trading.
BOOL doneit;    //more trading variables....

//Floats
double  timeon; //when we logged in?
double  timeleft;       //how long we have?
double  m[4];
double  n[4];
double  pub[4];
double  c1[4];          //I think this is 'goods' on the station...?
double  h[4];           //these are the cargo holds.... no idea why it's a float.
double  srr[4][1];

//Strings
char gfilespath[80];  //Where the G files are... ascii vid?
char datapath[80];        //Where the dat files are.
char i[1024];
char aim[1024];                 //I know we use this in showroom...
char sysopffn[80];              //The sysop file name?
char pnn[41];                   //The users name...
char *p[]={"Z","Ore.......","Organics..","Equipment."};
char userfile[256];             //should have been argv[1]... I know.. I suck.
char sysopfn[256];				//filename of the sysop log file..


//FILE handles
FILE *sysopf; //This file seems to track who played the game & when.
FILE *msger;
FILE *userf; //In the pascal version userf is a file of "users"..
FILE *smg;   //This is a file of SMR...


// Data structures
typedef struct {
	char    name[25];
	char    realname[14];
	char    laston[10];
	char    linelen;
	char    pagelen;
	char    sl;
	char    age;
	char    sex;
	char    callsign[8];
	double  gold;
	char    alert;
	char    smw;
	char    nomail;
	}userrec;

/****  This is the defintion of the 'users' record.
     users = RECORD
	       fa                   : STRING[41];
	       fb,fc,fd,fe,ff,fg    : INTEGER;
	       fh,fi,fj,fk,fl,fr,fp : INTEGER;
	       fm,fo,fq,ft,fv       : INTEGER;
	       newcash              : real;      END;***/
typedef struct {
	char    fa[41]; //user name
	int16           fb; //last on
	int16           fc; //killed by
	int16           fd;     //Turns remaining
	int16           fe; //armor
	int16           ff; //location
	int16           fg; //fighters
	int16           fh; //cargo holds
	int16           fi; //ORE
	int16           fj; //Organics
	int16           fk; //Equipment
	int16           fl; //credits
	int16           fr; //player #
	int16           fp; //unknown
	int16           fm; //unknown
	int16           fo; //next ship sector chain?
	int16           fq; //last sector
	int16           ft; //unknown
	int16           fv; //score, calculated everytime someone checks rank..
	double  newcash; //I don't think this is ever initalized.  But no doubt reals hold MORE money then the int16!!!
	} users;

/****   These are the message structures....
      smr=record
	   msg:stra;
	   destin:integer;
	  end;***/
typedef struct {
	char    msg[1024];
	int16   destin;
} smr;

userrec thisuser;
users userr;
users usert;

//prototypes
void main(int argc,char* argv[]);
void iport(void);
void init(void);
void starting(void);
double timer(void);
void getdate(void);
void printfile(char *p);
void cls(void);
void pausescr(void);
users readin(int16 pn);
void writeout(int16 pn,users u);
void initship(void);
void removeship(int16 pn);
void addship(int16 pn);
void instruct(void);
void enterroom(void);
int16 TWrandom(int16);
void destroyed(void);
void killed(int16 pn,int16 p);
void showroom(void);
void warped(void);              //display the warp exits...
void tleft(void);               //time left?
double nsl(void);
void dump(void);
void checkhangup(void);
void mainmenu(void);
void quitit(void);  //let's quit!
void tw4kill(void);        //running the attack from the main menu.
void attack(int16 s2,int16 f2,int16 e2);        //called from the enterroom
BOOL yesno();                   //ask YES/NO returns true on YES
void moveit(void);      //move to another sector
void inclear(void);
void message(int16 p, int16 po, int16 n, int16 n1);     //send a combat related loss message
void ssm(int16 dest,char *stra);	//how to actually send messages to other players.
void salvage(int16 pn, int16 p); //take stuff from dead people...
void info(int16 pn);                    //info on what's going on!
void fighters(void);            //deploy & pickup fighters in a sector..
int16 sgn(int16 i);                     //sign? absolute value????
void gamble(void);                      //50/50 odds...
void pchat(int16 p);            //send message to another player...
void computer(void);            //Ships computer menu...
void compmenu(void);            //Displays menu for the computer..
void findsec(int16 prr);        //Find a path to sector...
void shortest(int16 a, int16 b);        //find shortest path from a to b.. used in findsec.
void reportsec(int16 s2);       //report on the sector.
void rankings(void);            //prints out the rankings...
int16 rank(void);                       //called by rankings...
void upport(int16 s2);          //part of the reportsec bit...
char *timestring(void);         //replacement for time in the pascal version
int16 calcminutes(void);        //returns how many minutes there have been in the day.
void otherport(int16 p2);       //second half of upport...
void port1(void);                       //information about port #1 AKA earth
void helpit(void);                      //help from the main menu!
void planet(void);                      //from the main menu....
void upplanet(int16 s2);        //I know we run this before display does it's thing...
void display(void);                     //displays info on the planet....
void planethelpit(void);        //this was 'overloaded' in the pascal version to a local helpit...
void destroy(int16 l2);         //destroy a planet!
void increase(int16 l2);        //increase a planet's output
void sysoplog(char *i);         //logs to console...
void sl1(char* i);                      //called from sysoplog
void retreat(void);                     //retreat from a room with other fighters.. called from enterroom..
void minedrop(void);            //drop a mine in the current sector
void helpme(void);                      //called from enterroom
void spaceport(void);                   //dock to a port for some commerce.....
void trade(int16 l2);           //the trade process called in from the spaceport...
void dunno2(void);                      //part of the trade program... Adds goods to the ship.
void idunno(void);                      //called from dunno2, removes inventory from the seller
void leaveore(int16 l2);        //leave ore on planet...
void leaveorg(int16 l2);        //leave organics on planet...
void leaveequ(int16 l2);        //leave equipment on planet...
void takeore(int16 l2);         //}
void takeorg(int16 l2);         //}take from the planet...
void takeequ(int16 l2);         //}
void takeall(int16 l2);         //}
void maint(void);                       // the maintence program...
void TWdelete(int16 p);         //remove the user p from the db...
void rsm(void);                         //read sent messages...?
void readmsg(void);                     //prints some banner, then runs rsm
void movecabal(int16 go,int16 a,int16 b);       //move the cabal..?!
int16 picksec(void);            //pick a sector.. used by the maint process for moving around aliens..
void cattack(int16 go,int16 p,int16 f); //called in the maint process
void OSinit(void);              //setup some OS specific stuff
void OSdinit(void);             //turn them off...




/*******
This is just a stub, no hangups...
*******/
void main(int argc,char* argv[])
{
OSinit();
memset(userfile,0x0,sizeof(userfile));
#ifdef DEBUG
#else
if(argc<2)
      {
      printf("\nYou need to specify the parameter file!\n");
      OSdinit();
      exit(-1);
      }
#endif
sprintf(userfile,"%s",argv[1]);
iport();
ended=FALSE;

if(argc==3)			//lame attempt at making the maint more.. independant.
	{
	userr=readin(1);

	ay=userr.fc;    // The year?!
	tt=userr.fd;    // Turns per user?
	lp=userr.fe;    // ships start after here?
	ls=userr.ff;
	lt1=userr.fg;
	ll1=userr.fo;   // I think these are planets
	getdate();
	maint();
	exit(0);
	}


if(!hangup)
	init();

starting();

while(!ended)
	mainmenu();
//fclose(userf);
//fclose(msger);
OSdinit();
}


/*I think this reads the BBS config file into memory*/
void iport(void)
{
FILE *f;    /*The file handle*/
char i[1024];
int16 n;

#ifdef DEBUG
f=fopen("player1.txt","r");
#else
f=fopen(userfile,"r");
#endif
if(f!=NULL)
	{
	char buffer[1024];
	memset(buffer,0x0,sizeof(buffer));
	fgets(buffer,sizeof(buffer),f);
	usernum=atoi(buffer);

	fgets(buffer,sizeof(buffer),f);
	strcpy(thisuser.name,buffer);
	thisuser.name[strlen(thisuser.name)-1]=0x0;

	fgets(buffer,sizeof(buffer),f);
	strcpy(thisuser.realname,buffer);
	thisuser.realname[strlen(thisuser.realname)-1]=0x0;

	fgets(buffer,sizeof(buffer),f);
	strcpy(thisuser.callsign,buffer);
	thisuser.callsign[strlen(thisuser.callsign)-1]=0x0;

	fgets(buffer,sizeof(buffer),f);
	thisuser.age=buffer[0];

	fgets(buffer,sizeof(buffer),f);
	thisuser.sex=buffer[0];

	fgets(buffer,sizeof(buffer),f);
	thisuser.gold=atof(buffer);

	fgets(buffer,sizeof(buffer),f);
	strcpy(thisuser.laston,buffer);
	thisuser.laston[strlen(thisuser.laston)-1]=0x0;

	fgets(buffer,sizeof(buffer),f);
	thisuser.linelen=buffer[0];

	fgets(buffer,sizeof(buffer),f);
	thisuser.pagelen=buffer[0];

	fgets(buffer,sizeof(buffer),f);
	thisuser.sl=buffer[0];

	/*then we seem to read in some globals*/
	//CS
	fgets(buffer,sizeof(buffer),f);
	cs=atoi(buffer);
	if(cs>1){cs=1;}

	//SO
	fgets(buffer,sizeof(buffer),f);
	so=atoi(buffer);
	if(so>1){so=1;}

	//OKANSI
	fgets(buffer,sizeof(buffer),f);
	okansi=atoi(buffer);
	if(okansi>1){okansi=1;}

	//INCOM I think this means we are on a serial port..
	fgets(buffer,sizeof(buffer),f);
	incom=atoi(buffer);
	if(incom>1){incom=1;}

	fgets(buffer,sizeof(buffer),f);
	timeleft=atof(buffer);

	fgets(buffer,sizeof(buffer),f);
	sprintf(gfilespath,"%s",buffer);
	gfilespath[strlen(gfilespath)-1]=0x0;


	fgets(buffer,sizeof(buffer),f);
	strcpy(datapath,buffer);
	datapath[strlen(datapath)-1]=0x0;

	fgets(buffer,sizeof(buffer),f);
	strcpy(sysopfn,buffer);
//      i[strlen(i)-1]=0x0;

	fclose(f);
	}
else
	{
	printf("Parameter file not found.\n");
	exit(-1);
	}
hangup=FALSE;
timeon=timer();
//printf("timeon %f\n",timeon);
}

/*********************************************
This function returns time time in following manner
HOURS*3600+MINUTES*60+secontds+ticks/100
This used to align to MS-DOS function 2C
Get system time (Advanced MS-DOS page 324
*********************************************/
double timer(void)
{
double rval;
struct tm *tmm;
time_t result;
result=time(NULL);
tmm=localtime(&result);
rval=tmm->tm_hour*3600+tmm->tm_min*60+tmm->tm_sec;
return(rval);
}



void init(void)
{
int16 l;
int16 done;
char wtf;

//msger=fopen("twopeng.dat","r");
//smg=fopen("twsmf.dat","wb+"); //why open here when we use it in the ssm procedure?
//userf=fopen("twdata.dat","r+b");

userr=readin(1);

ay=userr.fc;    // The year?!
tt=userr.fd;    // Turns per user?
lp=userr.fe;    // ships start after here?
ls=userr.ff;
lt1=userr.fg;
ll1=userr.fo;   // I think these are planets
getdate();
if(userr.fl<d)
	{
		// maintence needs to run.. I think... but there is no provision for the year!!!
//		maint();
	}
printf("\n\n");
printfile("twhello.msg");
printf("\n");
printfile("twnewmod.msg");
pausescr();
cls();
printfile("twopeng.msg");
printf("\n");
printf("Initializing...");
pd=d;
printf("\n");
printf("Welcome %s!\n",thisuser.name);
printf("Searching my records for your name.\n");
l=2;
done=FALSE;

while(!done && l<lp)
	{
		userr=readin(l);
//strcmpi vs strcmp vs strcmp!?
		if(!strcmp(userr.fa,thisuser.name))
		{
			pn=l;
			done=TRUE;
			//printf("FOUND RECORD!\n");
		}
		else
			l++;
	}
if(!done)
	{
		printf("I can`t find your record, so I am assuming you are a new trainee.\n");
		printf("Entering a new trainee...");
		pn=2; //we start with #2 since the first 2 records are for the system...
		done=FALSE;
		while(!done || pn>lp)
		{
		usert=readin(pn);
		if(usert.fm<1)
			{
			done=TRUE;
			}
		else
			pn++;           //keep on looking for a free record...
		}
		if(!done)
			{
				printf("Im sorry but the game is full.");
				printf("Please leave a message for the Emperor so");
				printf("he can save a space for you when one opens up.");
				//sysoplog....
				ended=TRUE;
		    }
		else
			{
				char syslogtext[1024];
				usert=readin(1);

				printf("\n");
				printf("Notice: If you don`t user the simulator for %d days, you will",usert.fk);
				printf(" be\n.....removed to make room for someone else.");
				printf("\n");
				initship();
				userr=readin(pn);
				memset(userr.fa,0x0,sizeof(userr.fa));
				memcpy(userr.fa,thisuser.name,sizeof(thisuser.name));
				userr.fm=strlen(thisuser.name);
				userr.fr=pn;
				writeout(pn,userr);
				memset(syslogtext,0x0,sizeof(syslogtext));
				sprintf(syslogtext,"TIME DATE %s %d: New TW Player logged on\n",userr.fa,pn);
				sysoplog(syslogtext);
				instruct();
			}
	}//end of adding a new user...
	else
	{
		char syslogtext[1024];

		strcpy(pnn,userr.fa);
		printf("\n");
		memset(syslogtext,0x0,sizeof(syslogtext));
		sprintf(syslogtext,"TIME DATE %s %d: TradeWars/QP.\n",userr.fa,pn);
		sysoplog(syslogtext);
		userr=readin(pn);
		a=userr.fb;     //last on..
		done=FALSE;
		if(a>pd)        //banned?
		{
			printf("you won''t be allowed on for another %d day(s)\n",a-pd);
			ended=TRUE;
		}
		if( (a==pd) && (userr.fc!=99) )
		{
			printf("You have been on today.\n");
			if(userr.fd<1)
			{
				printf("You don't have any turns left today.\n");
				printf("You will be allowed to play tomorrow.\n");
				ended=TRUE;
			}
			if(userr.fc==pn)
			{
				printf("Qu''vatlh!!  You killed yourself today! \n");
				printf("With some reserve, tho, you will be allowed on tomorrow.\n");
				ended=TRUE;
			}
		}
		if( (a<pd) || (a==pd) &&(!ended) &&(userr.fc!=99) )
		{
			readmsg();
			if(userr.fc==0) //user hasn't been killed...
			{
				if((userr.fd<=tt) && (userr.fb<pd) )    //do you have enough turns & ...?
				{
					userr.fd=tt;                                            //add some turns
					userr.fb=pd;                                            //update the last on
					writeout(pn,userr);
				}
				done=TRUE;
				printf("\n");
				printf("You have %d turns this Stardate.\n",userr.fd);
			}
		}
		if((!ended)&&(!done))
		{
		a=userr.fc;             //who killed you
		if(a==-99)              //..?
			initship();     //lol free ship!
		else
			if(a==-98)
				printf("You have been destroyed by a person who has been...removed from the game.\n");
		if(a==-1)
			printf("You have been ambushed by the Romulans!\n");
		if(a==pn)
			printf("You managed to Q'est' yourself on your last time on.\n");
		if( (a>1) && (a<=lp) )
		{
			usert=readin(a);
			printf("%s destroyed your ship!\n",usert.fa);
		}
		initship();
		}
	}
}

void starting(void)
{
	//How did the user get a negative sector?!
	if(userr.ff<1 &&!ended)
	{
		printf("You are being moved to sector 1");
		userr.ff=1;
		userr.fq=0;
		writeout(pn,userr);
	}
	//This *tax* was to prevent 32767 integers from rolling...
	if(userr.fl>20000)
	{
		printf("Tax time! You are being taxed 3000 credits to help support");
		printf("the Federation and the struggle against the Romulan Empire.");
		userr.fl=userr.fl-3000;
		writeout(pn,userr);
	}
enterroom();
}

//I think this returns the day # of the year...
void getdate(void)
{
	struct tm *tmm;
time_t result;
result=time(NULL);
tmm=localtime(&result);
d=tmm->tm_yday;
}

void printfile(char *p)
{
	FILE *fp;
	char t1[1024];
	int line;

	line=0;
	memset(t1,0x0,sizeof(t1));
#ifdef MICROSOFT
	sprintf(t1,"%s\\%s",gfilespath,p);
#else
	sprintf(t1,"%s/%s",gfilespath,p);
#endif
	//printf("opening [%s]\n",t1);
	fp=fopen(t1,"r");
	if(fp==NULL)
		printf("Error opening [%s]\n",t1);
	else
	{
		while(!feof(fp))
		{
			memset(t1,0x0,sizeof(t1));
			fgets(t1,sizeof(t1),fp);
		if(!feof(fp))
			{
			fwrite(t1,1,strlen(t1),stdout);
			line++;
			if(line>=24)		//hardcode 24 lines!!
				{
				pausescr();
				line=0;
				}
			}
		}
	fclose(fp);
	}

	//for now this is not interesting.
}

void cls(void)
{
	//clear the screen
}

void pausescr(void)
{
	//pauses the input
	printf("\t\t\t-*-\t[Enter to continue]");
	getchar();
}

// Reads in the user record number from the userf file.
users readin(int16 pn)
{
	char wtf;
	users readinusers;
	char fullname[1024];

	memset(fullname,0x0,sizeof(fullname));

	#ifdef MICROSOFT
	sprintf(fullname,"%s\\twdata.dat",datapath);
#else
	sprintf(fullname,"%s/twdata.dat",datapath);
#endif

#ifdef DEBUG
printf("Opening [%s]\n",fullname);
#endif


	userf=fopen(fullname,"r+b");
	if(userf==NULL)
	{printf("\nERROR with your paths, [%s] doesn't exist!!!\n\n\n",fullname);exit(-11);}
	else
	{
	rewind(userf);
	fread(&wtf,1,1,userf);  //The first byte in the file is garbage....
	fseek(userf,sizeof(userr)*pn,SEEK_CUR);
	fread(&readinusers,sizeof(userr),1,userf);
	fclose(userf);
	}
	return(readinusers);
}

void writeout(int16 pn,users u)
{
	int err;
	char wtf;
	char fullname[1024];

	#ifdef MICROSOFT
	sprintf(fullname,"%s\\twdata.dat",datapath);
#else
	sprintf(fullname,"%s/twdata.dat",datapath);
#endif


	userf=fopen(fullname,"r+b");
	rewind(userf);
	fread(&wtf,1,1,userf);  //The first byte in the file is garbage;
	fseek(userf,sizeof(userr)*pn,SEEK_CUR);
	err=fwrite(&u,sizeof(userr),1,userf);
#ifdef DEBUG
	printf("writeout pn%d RC:%d\n",pn,err);
	printf("\n%s\n b%d c%d d%d e%d f%d g%d h%d i%d j%d k%d l%d r%d p%d m%d o%d q%d t%d v%d \n",u.fa,u.fb,u.fc,u.fd,u.fe,u.ff,u.fg,u.fh,u.fi,\
		u.fj,u.fk,u.fl,u.fr,u.fp,u.fm,u.fo,u.fq,u.ft,u.fv);
#endif
	fclose(userf);
}

void initship(void)
{
int16 b;
int16 c;
printf("\nYour ship is being initialized.");
removeship(pn);
usert=readin(1);
//These values seem to corespond with 'default' settings....
a=usert.fh;  //default cargo units?
b=usert.fi;      //default cash
c=usert.fj;  //defualt fighters?
userr=readin(pn);
userr.fb=pd;    //last on...
userr.fc=0;             //killed by
userr.fd=tt;    //turns remaining
userr.fe=0;             //armor
userr.ff=1;             //location
userr.fg=a;             //fighters
userr.fh=c;             //cargo holds
userr.fi=0;             //ORE
userr.fj=0;             //Organics
userr.fk=0;             //Equipment
userr.fl=b;             //credits
userr.fm=1;             //unknown
writeout(pn,userr);
addship(pn);
}

void removeship(int16 p)
{
int16 r;
int16 b;
int16 done; //our BOOLEAN....
usert=readin(p);
r=usert.ff;
if(r!=0)
	{
		usert=readin(lp+r);
		a=usert.fi;
		if(a!=0)
		{
			if(a==p)
			{
			usert=readin(a);
			b=usert.fo;
			usert=readin(lp+r);
			usert.fi=b;
			writeout(lp+r,usert);
			}
			else
			{
				done=FALSE;
				usert=readin(a);
				while(!done)
				{
					if(usert.fo==p)
					{
						b=a;
						done=TRUE;
					}
					a=usert.fo;
					usert=readin(a);
				}
				a=usert.fo;
				usert=readin(b);
				usert.fo=a;
				writeout(b,usert);
			}
		}

		userr=readin(pn);
	}
}


void addship(int16 pn)
{
int16 r;
int16 b;
int16 done;
r=userr.ff;
if(r!=0)
	{
		usert=readin(lp+r);
		b=usert.fi;
		usert.fi=pn;
		writeout(lp+r,usert);
		userr.fo=b;
		writeout(pn,userr);
	}
}

/*prints the instructions....*/
void instruct(void)
{
	if(!ended)
	{
		printf("\nDo you want instructions (Y/N) [N]? ");
		if(yesno())
			printfile("twinstr.doc");
	}
}


//Uppon entering a 'room'...
void enterroom(void)
{
	int16 f2;
	int16 e2;
	int16 r1;
	char i[10];

removeship(pn);
addship(pn);
prr=userr.ff;
s2=prr+lp;              //lp is that the sector offset?
usert=readin(s2);       //load in the sector
e[1]=usert.fb;          //setup the warps.
e[2]=usert.fc;
e[3]=usert.fd;
e[4]=usert.fe;
e[5]=usert.ff;
e[6]=usert.fg;
printf("\n");
if((s2>9) && (usert.fp>0))    //sector .gt 9 & I guess the number of mines is in .fp?
	{
		r1=TWrandom(10);

		if(usert.fp-r1>0)
		{
			usert.fp=usert.fp-1;    //reduce mine count in sector
			writeout(s2,usert);
			r1=TWrandom(5)+18;              //mine damage 18 to 22
			userr.fe=userr.fe-r1;   //Ship Armor knocked down
			printf("A space mine detonates near you!\n");
			printf("The console reports damages of %d battle points!\n",r1);
			if(userr.fe>-1)                 //Can Armor handle it?
			{
				printf("Your ship armor absorbs the brunt of the explosion!\n");
			}
			else
			{
				r1=userr.fe*-1;         //damage less the armor
				if(r1>userr.fg)
				{
					printf("Life support knocked out! Energy Generation shut down\n");
					printf("In space, theres no one to hear you scream...");
					destroyed();
				}
				else
				{
					printf("%d K3-A Fighers destoroyed by the impact!",r1);
					userr.fe=0;
					userr.fg=userr.fg-r1;
				}
			}
		}
	}       //end of the mine check...


		writeout(pn,userr);

		if(usert.fm!=pn)
		{
		if(usert.fl!=0)
			{
				showroom();
				printf("You have to destory the fighers before entering this sector.");
				f2=userr.fg;
				usert=readin(s2);
				e2=usert.fl;
				printf("\n");
				printf("Fighters: %d/%d\n",f2,e2);
				done=FALSE;
				while((!done)&& (!hangup))
				{
					dump();
					tleft();
					printf("Option? (A,D,I,Q,R,S,?):? ");
					memset(i,0x0,sizeof(i));
					fflush(stdin);
					i[0]=getchar();
					if(i[0]==' ')
						printf("? =<Help>");
						switch(i[0])
							{
							case 'R': {retreat();break;}
							case 'D': {printf("Display>\n");showroom();break;}
							case 'A': {attack(s2,f2,e2);break;}
							case 'Q': {quitit();done=TRUE;break;}
							case 'I': {printf("\n<Info>\n");info(pn);break;}
							case 'S': {pchat(pn);break;}
							case '?': {helpme();break;}
							}
						f2=userr.fg;	//update the fighters stats
						usert=readin(s2);	//and re-read in the romulans
						e2=usert.fl;	//the pascal version preserved s2/f2/e2...
				}
		}
		else
			inclear();
		}
		else
			inclear();
	}


int16 TWrandom(int16 n)
{
int ret;
#ifdef DEBUG
	return(n-1);
#else
#define getrandom( min, max ) ((rand() % (int)(((max)+1) - (min))) + (min))
return(getrandom(1,n));
#endif
}

void destroyed(void)
{
	printf("Your ship has been destroyed!\n");
	printf("You will start over tomorrow with a new ship.");
	killed(pn,pn);
	ended=TRUE;
	done=TRUE;
}

//P is dead guy, PN is killer
//
void killed(int16 pn,int16 p)
{
	int16 l;
	removeship(p);
	usert=readin(p);
	usert.fc=pn;
	usert.ff=0;
	writeout(p,usert);
	for(l=lp;l<ls;l++)
	{
		usert=readin(l);
		if(usert.fm==p)
		{
			usert.fm=-2;
			writeout(l,usert);
		}
	}
}


//Shows the room we are in?
void showroom(void)
{
	int16 l;
	int16 lee;

	prr=userr.ff;
	s2=prr+lp;
	printf("\n");
	printf("Sector: %d\n",prr);
	usert=readin(s2);
	st=usert.fh;
	if(st!=0)
	{
		usert=readin(st+ls);
		printf("Ports: %s, class %d\n",usert.fa,usert.fb);
	}
	else
	{
		printf("Ports: None\n");
	}
	usert=readin(s2);
	a=usert.fo;
	if(a!=0)
	{
		usert=readin(a+lt1);
		printf("Planet: %s\n",usert.fa);
		usert=readin(s2);
	}
	g2=0;
	printf("Other Ships: ");
	a=usert.fi;
	if(a==0)
		printf("None\n");
	else
	{
	while(a!=0)
		{
			usert=readin(a);
			if(a!=pn)
			{
				printf("\n   %s with %d fighters in a ",usert.fa,usert.fg);
				if(usert.fh>125)
					printf("very ");
				if(usert.fh < 50)
					printf("small ");
				if(usert.fh > 90)
					printf("large ");
				printf("merchant ship\n");
				g2=1;
			}
			a=usert.fo;
		}
	if(g2==0)
		printf("None\n");
	}
	usert=readin(s2);
	printf("Fighters in sector: ");
	if(usert.fl==0)
		printf("None\n");
	else
	{
		memset(aim,0x0,sizeof(aim));
		sprintf(aim,"%d",usert.fl);
		if(usert.fm==-2)
			printf("%s Rouge mercenaries\n",aim);
		else
		{
			if(usert.fm<1)
				printf("%s (Romulans)\n",aim);
			else
				if(usert.fm==pn)
					printf("%s (yours)\n",aim);
				else
				{
					usert=readin(usert.fm);
					printf("%s (belong to %s)\n",aim,usert.fa);
				}
		}

	}
	warped();
}



void warped(void)
{
	int16 lee;
	int16 l;


	printf("Warp Lanes lead to: ");
	if(e[1]!=0)
	{printf("%d ",e[1]);lee=2;}
	if(e[2]!=0)
	{printf("%d ",e[2]);lee=3;}
	if(e[2]!=0)
	{printf("%d ",e[2]);lee=3;}
	if(e[3]!=0)
	{printf("%d ",e[3]);lee=4;}
	if(e[4]!=0)
	{printf("%d ",e[4]);lee=5;}
	if(e[5]!=0)
	{printf("%d ",e[5]);lee=5;}
						//lee=7;
	printf("\n");
}


void tleft(void)
{
	int16 x;
	int16 y;
	if(timer()<timeon)
		timeon=timeon-24.0*60*60;
	if(nsl()<0)
	{
		printf("\n");
		printf("Time expired.");
		hangup=TRUE;
	}
	checkhangup();
}

double nsl(void)
{
	if (timer()<timeon)
		timeon=timeon-24*3600;
return(timeleft-(timer()-timeon));
}

//Im sure this was a diagnostic function.
void dump(void)
{
}

//it's a stub in the pascal version.
void checkhangup(void)
{
}


void mainmenu(void)
{
	char i[1024];
	int16 intt;
	dump();
	tleft();
	printf("\n");
	fflush(stdin);
	printf("Command [%d](?=Help)? ",userr.fd);	//prompt with the # of remaining turns
	memset(i,0x0,sizeof(i));
	scanf("%c",i);
	if(strlen(i)==0)
		printf("? = Help");
	switch(i[0])
	{
	case 'A': tw4kill();break;
	case 'C' : {computer();break;}
	case 'D' : {printf("\n<Display>");showroom();break;}
	case 'F' : {fighters();break;}
	case 'G' : {gamble();break;}
	case 'I' : {printf("\n<Info>");info(pn);break;}
	case 'L' : {planet();break;}
	case 'M' : {printf("\n<Drop Mine in sector>");minedrop();break;}
	case 'P' : {spaceport();break;}
	case 'W' : moveit();break;
	case 'Z' : {printf("\n<Instructions>");instruct();break;}
	case 'Q' : quitit();break;
	case '?' : helpit();break;
	case ' ' : helpit();break;
	}
}

//Quit the game.
void quitit(void)
{
	printf("\n<Quit>\n");
	printf("Confirmed? (Y/N)? ");
	if(yesno())
		ended=TRUE;
	printf("\n");
}





//Asks Y/N and returns true on YES, FALSE on no.
BOOL yesno(void)
{
	char c;
	fflush(stdin);
	c=getchar();
	fflush(stdin);
	//c=toupper(c);
	if(c=='Y')
		return TRUE;
	else
		return FALSE;
}


void moveit(void)
{
	int16 t2;
	int16 l;
	int16 t;
	int16 lee;
	char i[1024];
	BOOL done;

	printf("\n<Warp to another sector>\n");
	t2=userr.fd;
	if(t2<1)
	{
		printf("You don't have any turns left.");
	}
	else
	{
		warped();
		printf("To which Sector? ");
		memset(i,0x0,sizeof(i));
		scanf("%s",&i);
		t=atoi(i);
		if((t<1) || (t>9999))
			printf("Illegal Number.");
		else
		{
			done=FALSE;
			for(l=1;l<7;l++)
			{
				if(e[l]==t)
					done=TRUE;
			}
			if(!done)
				printf("That Warp Lane is currently closed.");
			else
			{
				t2=t2-1;        //you expend a turn!
				removeship(pn);
				userr.ff=t;             //current location
				userr.fq=prr;   //last sector you were in
				userr.fd=t2;    //turns remainting
				writeout(pn,userr);     //write out user record
				addship(pn);
				if( (t2==10) && (t2<6) )        //warn users with low turns...
					{
						printf("You have %d turns left.\n",t2);
					}
				enterroom();


			}
		}
	}

}


void inclear(void)
{
	if(prr!=85)
		showroom();
	else
	{
		printf("\n\nCOWABUNGA!!! You've defeated the Romulan Invasion fleet and recieved");
		printf("\nan Imperial Commendation, as well as a cash bonus!");
		printf("\nUnfortunately, the Roms are too stupid to know they're beaten...");
		usert=readin(s2);
		userr.fl=userr.fl+2500;
		writeout(pn,userr);
		usert.fl=2000;
		usert.fm=-1;
		writeout(s2,usert);
	  //addmsg('Congrats to '+pnn+' who smashed the Romulan Invasion Fleet on '+date);
	  //addmsg('and received an Imperial Commendation.');
	}
}

void message(int16 p, int16 po, int16 n, int16 n1)
{
	char string[1024];
	memset(string,0x0,sizeof(string));
if(po<2)
	{
	sprintf(string,"The Romulans destroyed %d of your fighters.",n);
	ssm(p,string);
	}
else
	{
		usert=readin(po);
		if(n1==0)
		{
		sprintf(string,"%s destroyed %d of your fighters.",usert.fa,n);
		ssm(p,string);
		}
		else
		{
			sprintf(string,"%s destroyed %d armor points and %d of your fighters.",usert.fa,n1,n);
			ssm(p,string);
		}
	}
}


// I think this is working accidentally
// the logic is all screwed up.
// but it shouldn't be *THAT* hard
// I think because I'm doing it BACKWARDS....
void ssm(int16 dest,char *stra)
{
	int16 e;
	int16 cp;
	int16 t;
	smr x;
	userrec u;
	char fullname[1024];

	#ifdef MICROSOFT
	sprintf(fullname,"%s\\twsmf.dat",datapath);
#else
	sprintf(fullname,"%s/twsmf.dat",datapath);
#endif

    smg=fopen(fullname,"r+b");
	if(smg==NULL)
	{
		//printf("ssm() error with twsmf.dat [%s\\n",fullname);
	}
	else
	{
		fseek(smg,0,SEEK_END);
		memset(x.msg,0x0,sizeof(x.msg));
		memcpy(x.msg,stra,strlen(stra));
		x.destin=dest;
		fwrite(&x,sizeof(x),1,smg);
		fclose(smg);
	}
}






//dumps out some information on the user...
void info(int16 pn)
{
	double a;
	int16 b;
	int16 c;

	usert=readin(pn);
	printf("\nName: %s",usert.fa);
	printf("\nSector: %d\tTurns left: %d",usert.ff,usert.fd);
	printf("\nFighters: %d\tArmor points: %d",usert.fg,usert.fe);
	printf("\nCargo Holds: %d",usert.fh);
	printf("\nOre: %d  Org: %d  Eqp: %d",usert.fi,usert.fj,usert.fk);
	printf(" Empty holds: %d\n",usert.fh-usert.fi-usert.fj-usert.fk);
	printf("\nCredits: %d\n",usert.fl);
}




int16 sgn(int16 i)
{
	int16 retval;

	if(i>0)
		retval=1;
	else
		if(i<0)
			retval=-1;
		else
			retval=0;
	return(retval);
}


void gamble(void)
{
	int16 m;        //temp field for user credits
	int16 n;        //number of credits user is going to gamble....
	int16 j;
	int16 v;
	char i[1024];

	printf("\n<Gamble with.....Harcourt Fenton Mudd\?\?>\n");
	m=userr.fl;
	printf("You have %d credits.\n",m);
	printf("How much do you want to gamble at double or nothing (50-50) odds)[0]? ");
	memset(i,0x0,sizeof(i));
	scanf("%s",i);
	n=atoi(i);
	if(n>0) //we are going to gamble!
	{
		if(n>m)
			printf("You're not that rich.\n");
		else
		{
			printf("Tossing the Klin Zha Spindles...\n");
			v=TWrandom(2);//+1;
			if(v==1)
			{
				userr.fl=m-n;
				writeout(pn,userr);
				printf("\nYou lost. Mudd smiles as he takes away your wager...\n");
			}
			else
			{
				if(userr.fl=m+n<0)
					{printf("ERROR WRAPPED!!!\n");n=0;}
				userr.fl=m+n;
				writeout(pn,userr);
				printf("\nVICTORY! Mudd looks a bit grim...\n");
			}
		printf("You now have %d credits.\n",userr.fl);
		}
	}
}




void upport(int16 s2)
{
	int16 p2;
	int16 c;
	int16 l;
	int16 code;
	int16 mn;
	double  temp;
	double  dim;

	usert=readin(s2);       //read in the sector..
	p2=usert.fh+ls;         //to find the port?
	usert=readin(p2);
	n[1]=usert.fd+usert.fr/10000;
	n[2]=usert.fe+usert.fo/10000;
	n[3]=usert.ff+usert.fp/10000;
	pub[1]=usert.fg;
	pub[2]=usert.fh;
	pub[3]=usert.fi;
	c1[1]=usert.fj;
	c1[2]=usert.fk;
	c1[3]=usert.fl;
	getdate();
	c=d;
	mn=calcminutes();                               //minutes in the day
	dim=d-usert.fc+(mn-usert.fq)/1440;
	if(dim>=0)
	{
		if(dim>10)
			dim=10.0;
		for(l=1;l<4;l++)
		{
			n[l]=n[l]+pub[l]*dim;
			if(n[l]>pub[l]*10)
				n[l]=pub[l]*10;
		}
	}//end of the dim thing

	for(l=1;l<4;l++)
	{
	//      m[l] := INT(b[l]*(1-c1[l]*n[l]/pub[l]/1000)+0.5);
		m[l]  =    (b[l]*(1-c1[l]*n[l]/pub[l]/1000)+0.5);
	}
	usert=readin(p2);
	usert.fc=c;
	usert.fd=n[1];
	usert.fe=n[2];
	usert.ff=n[3];

	for(l=1;l<4;l++)
	{           //INT((n[l]-INT(n[l]))*10000+0.5);
		srr[l][0]= (n[l]-n[l])*10000+0.5; //how can (x-x)*y+0.5 come out to anything but??
		n[l]=n[l];
		//why are we even doing this?!!?!!?!
	}
	usert.fr=(int)srr[1][0];
	usert.fo=(int)srr[2][0];
	usert.fp=(int)srr[3][0];
	usert.fq=mn;
	writeout(p2,usert);
}

//This *WAS* time in the pascal version...
//but it collides in C
//
//This simply returns a string in the HH:MM:SS format....
char *timestring(void)
{
	char ts[1024];
	printf("timestring incomplete()\n");
	return(ts);
}

//This is just a shortcut to return how many minutes in the day it is..
int16 calcminutes(void)
{
int16 rval;
struct tm *tmm;
time_t result;

result=time(NULL);
tmm=localtime(&result);
rval=tmm->tm_hour*60+tmm->tm_min;
return(rval);
}



//The main menu help function
void helpit(void)
{
  printf("\n<Help>\n");
  printf("A - <A>ttack enemy vessels\n");
  printf("C - <C>omputer\n");
  printf("D - re<D>isplay sector\n");
  printf("F - take or leave <F>ighters\n");
  printf("G - <G>amble with an odd Tera''ngan trader\n");
  printf("I - <I>nfo on your ship\n");
  printf("L - <L>and on surface of a planet.\n");
  printf("M - drop <M>ine\n");
  printf("P - dock at a space <P>ort (and trade)\n");
  printf("W - <W>arp to another sector\n");
  printf("Q - <Q>uit game\n");
  printf("Z - instructions\n");
}




//From the planet function, display what is going on.....
void display(void)
{
	int16 i;

	upplanet(s2);
	usert=readin(l2);
	printf("\nPlanet: %s\n",usert.fa);
	printf(" Item      Prod.  Amount  in holds\n");
	printf(" ~~~~      ~~~~~  ~~~~~~  ~~~~~~~~\n");
	for(i=1;i<4;i++)
	{
		printf("%s\t%.0f\t%.0f\t%.0f\n",p[i],pub[i],n[i],h[i]);
	}
	printf("You have %.0f free cargo holds.\n",h[0]-h[1]-h[2]-h[3]);
}


//This procedure is to check if the oper is logged on or some such thing.
void sysoplog(char *i)
{
	// if (not so) or incom then
	sl1(i);
}

//here we actually update the sysoplog file, if its a valid handle
//otherwise we print it to the screen.
void sl1(char *i)
{
//File slash order for MS vs the UNIX world...
memset(sysopffn,0x0,sizeof(sysopffn));
#ifdef MICROSOFT
	sprintf(sysopffn,"%s\\%s",gfilespath,sysopfn);
#else
	sprintf(sysopffn,"%s/%s",gfilespath,sysopfn);
#endif
	if(sysopffn[strlen(sysopffn)-1]=0x10)
		sysopffn[strlen(sysopffn)-1]=0x0;
	sysopf=fopen(sysopffn,"a+");
	if(sysopf==NULL)
		{
		printf("error writing [%s]\n",sysopffn);
		fwrite(i,strlen(i),1,stdout);
		}

	else
	{
		//fprintf(sysopf,"%s
		fwrite(i,strlen(i),1,sysopf);
		fclose(sysopf);
	}
}


//From enterroom
void helpme(void)
{
printf("\nHelp\n");
printf("A - <A>ttack\n");
printf("D - re<D>isplay sector\n");
printf("I - <I>nformation about your ship\n");
printf("Q - <Q>uit the game\n");
printf("R - <R>etreat\n");
printf("S - <S>end a distress call to another player\n");

}

//This is called from the findsec function in the computer menu.
//traveling salesman ?
//
void shortest(int16 a, int16 b)
{
	int16 n;
	int16 c;
	int16 l;
	int16 m;
	BOOL found;

	n=1;
	c=b;
	if(a==b)
	{
		s[0][0]=a;
		s[0][1]=0;
		s[a][1]=0;
	}//you are going NOWHERE...
	else
	{
		//for(l=1;l<201;l++)
		//      for(m=0;m<2;m++)
		//              s[l][m]=0;                      //lol no memset.
		memset(s,0x0,sizeof(s));
		s[a][1]=1;
		found=FALSE;
		while(!found && (n<10000))
		{
			l=1;
			while(!found && (l<201) )
			{
				if(s[l][1]==n)
				{
					usert=readin(l+lp);
					e[1]=usert.fb;          //read in the warp to's..
					e[2]=usert.fc;
					e[3]=usert.fd;
					e[4]=usert.fe;
					e[5]=usert.ff;
					e[6]=usert.fg;
					for(m=1;m<7;m++)
					{
						if(e[m]!=0)             //there is an exit...
							if(s[e[m]][1]==0)
							{
								s[e[m]][1]=n+1;
								s[e[m]][0]=l;
								if(e[m]==b)
									found=TRUE;
							}
					}
					//l=l+1;
				}
				l=l+1;
			}//end l to 200 search
			if(!found)
				n++;
			//printf("Search 9999 iterations... %d\r",n);
		}//end n<10000
		if(!found)
		{
		memset(i,0x0,sizeof(i));
		sprintf(i,"*** Error - Sector path not found - from sector %d to sector %d\n",a,b);
		sysoplog(i);
		printf("*** Error - Sector path not found - from sector %d to sector %d\n",a,b);
		s[a][1]=0;
		ended=TRUE;
		}//end path not found!!
		else
		{
			//printf("found it!\n");
			while(s[c][0]!=0)
			{
				s[s[c][0]][1]=c;
				c=s[c][0];
				if(s[c][0]==0)
					s[b][1]=0;
			}
		}
	}//end going somewhere...?
}


//The maintenence routeene from twmaint.pas
void maint(void)
{
	int16 i;
	int16 p;        //used to iterate through players looking for dead ones.
	int16 l;
	int16 m;
	int16 a;
	int16 l2;
	int16 e1;
	int16 v;
	int16 s1;
	int16 r;
	int16 go;
	int16 b1;
	int16 g1;
	int16 sc1;
	int16 t1;
	BOOL done;
	BOOL done1;
	smr x;
	FILE *smg2;

	printf("You're the first player today!\n\n\n");
	printf("TradeWars II/QP Daily Maintence program\n");
	sysoplog(":  TW Maintence program ran\n");
	usert=readin(1);
	l2=usert.fk;            //what is the max age of a player....
	printf("\nNow...'Removing' inactive players\n");
	getdate;
	l2=d-l2;
	for(p=2;p!=lp;p++)
	{
		usert=readin(p);
		if((usert.fb<=l2)&&(usert.fm!=0))       //check last on & the user fm record...
		{
			char i[255];
			sprintf(i,"  -  %s deleted from game\n",usert.fa);
			sysoplog(i);
			TWdelete(p);
			memset(i,0x0,sizeof(i));
		}
	}
//compact the smg data file..
	printf("Need to compact the SMG\n");


	printf("\nRomulans advance across the Disputed Zone... \n");
	sysoplog("   Romulan report:\n");
	for(l=1;l!=9;l++)
	{
		usert=readin(l+lp);
		g[l][0]=usert.ft;
		g[l][1]=0;
	}
	for(l=1;l!=8;l++)
	{
		for(m=l;m!=9;m++)
		{
			if(g[l][0]==g[m][0])
				g[m][0]=0;

		}
	go=0;
	}
	for(l=1;l!=9;l++)
		if(g[l][0]!=0)                                  //if location !=0
		{
			usert=readin(g[l][0]+lp);       //read in sector of a group
			if(usert.fm==-1)                        //cabal fighters
			{
				go=go+usert.fl;
				g[l][1]=usert.fl;
			}
		}
	usert=readin(1);
	r=usert.fr;             //what's this parameter?!
	if(go<2000-r)
		e1=r;
	else
	{
		e1=2000-go;
		if(e1<0)
			e1=0;
	}                                               //find how many to 'add'...
	movecabal(2,83,85);             //move gorup 2 to sector 85
	usert=readin(85+lp);    //all the fun is in sector 85.. hard coded.
	if(usert.fm!=-1)
	{
		g[1][1]=1000;
		usert.fm=-1;
		usert.fl=1000;
		writeout(85+lp,usert);
	}
	a=usert.fl;
	usert.fl=usert.fl+e1;
	writeout(85+lp,usert);
	s1=g[1][1]+g[2][1]+e1;
	if(s1<1500)
		e1=1;
	else
		e1=0;
	if(s1<1000)
	{
		g[1][1]=s1;
		g[2][0]=0;
		g[2][1]=0;
	}
	else
	{
		g[1][1]=1000;
		g[2][1]=s1-1000;
		g[2][0]=85;
	}
	movecabal(2,85,83);             //move group 2 to sector 83
	for(g1=3;g1!=5;g1++)            //move group type II fighters
	{
		printf("%d\n",g1);
		done=FALSE;
		done1=FALSE;
		//repeat ((g[g1,0]<=0) OR (g[g1,0]>=8) OR (g[g1,1]=0)) AND done;
		if( (g[g1][1]!=0) && (g[g1][0]) || done1)
		{
			done=TRUE;
			while( (g[g1][0]!=usert.fq) && (usert.fq!=0) )
			{
				usert=readin(g1+lp);
				if((g[g1][0]=usert.fq)||(usert.fq==0))
				{
					v=picksec();
					usert.fq=v;
					writeout(g1+lp,usert);
				}
			}
			if( (g[g1][1]<50) || (g[g1][1]>100) )
			{
				usert.fq=83;
				writeout(g1+lp,usert);
			}
			if(e1==1)
			{
				usert.fq=85;
				writeout(g1+lp,usert);
			}
			shortest(g[g1][0],usert.fq);
			if(s[g[g1][0]][1]!=0)
				movecabal(g1,g[g1][0],s[g[g1][0]][1]);
						//(*' Move 1 step toward goal*)
		}
		else
			if(g[2][1]>=600)
			{
				g[g1][1]=100;
				g[2][1]=g[2][1]-100;
				done1=TRUE;
				g[g1][0]=83;            //Create a group II group
			}//end gXY>=600
			else
				done=TRUE;
		//until ((g[g1,0]<=0) OR (g[g1,0]>=8) OR (g[g1,1]=0)) AND done;
	}
	p=rank();
	if(p<1)
	{
		sc1=0;
		t1=0;
	}
	else
	{
		t1=p;
		usert=readin(t1);
		if(usert.fv<2500)
		{
			sc1=0;
			t1=0;
		}
	}
	if( (sc1==0) || (t1==0) )
	{
		sc1=83;
		t1=0;
	}
	for(g1=6;g1!=9;g1++)                    //Move group type III fighters...
	{
		done=FALSE;
		done1=FALSE;
		printf("\nEntering Repeat #1\n");
		//REPEAT  UNTIL ((g[g1,0]<=0) OR (g[g1,0]>=8) OR (g[g1,1]=0)) AND done1;
		while( ((g[g1][0]<=0) || (g[g1][0]>=8) || (g[g1][1]=0)) && done1 )
		{
			if (((g[g1][1]!=0) && (g[g1][0]!=0)) || done)
			{
				if(g1==9)
					b1=sc1;
				else                                                              //This is where It hangs!?! *)
					while ((v!=g[g1][0]) && (v>1) )   // This should stop hang...
					{
						printf("Entering conditional Repeat #2\n");
						v=picksec();
						b1=v;
					}
					printf("Left conditional Repeat #2\n");
					if( (g[g1][1]<20) || (g[g1][1]>50) )    //pick a destination...
						b1=83;
					if(e1==1)
						b1=85;
					shortest(g[g1][0],b1);                                  //plot a course...
					done1=FALSE;
					if( s[g[g1][0]][1]!=0 )
					{
						while((g[g1][0]==b1) && done1)
						{
						if ( (g[g1][1]<0) || (g[g1][0]==0) )
						{
							g[g1][0]=0;
							g[g1][1]=0;
							done1=TRUE;
						}
						else
							if((g1!=9) || (g[g1][0]!=sc1))
							{
								movecabal(g1,g[g1][0],s[g[g1][0]][1]);
								if( (g[g1][1]<0) || (g[g1][0]==0) )
								{
									g[g1][0]=0;
									g[g1][1]=0;
									done1=TRUE;
								}//end if ( (g[g1][1]<0) || (g[g1][0]==0) )
								else
								{
									usert=readin(g[g1][0]+lp);
									if( (g1!=9) && (usert.fi!=0) )
									{
										p=usert.fi;
										cattack(g1,p,20);
									}
								}
							}       //end if if((g1!=9) || (g[g1][0]!=sc1))
						} //end while (g[g1,0]=b1) OR done1;
						if( (t1!=0) && (g1==9) && (!done1) )
						{
							cattack(g1,t1,g[g1][1]);
						}//end if( (t1!=0) && (g1==9) && (!done1) )
						done1=TRUE;
					}//end if if( s[g[g1][0][1]!=0 )
					else
						done1=TRUE;
			}       //end of if (((g[g1][1]!=0) && (g[g1][0]!=0)) || done)
			else
				if(g[2][1]>=550)
				{
					g[g1][1]=50;
					g[2][1]=g[2][1]-50;
					g[g1][0]=83;
					done=TRUE;
				}
				else
					done1=TRUE;
			if( (g[g1][0]>0) && (g[g1][0]<8) && (g[g1][1]!=0) )
			{
				s1=85;
				done=TRUE;
			}
		}//end REPEAT UNTIL ((g[g1,0]<=0) OR (g[g1,0]>=8) OR (g[g1,1]=0)) AND done1;
	}//end move type III fighters
	for(l=1;l!=9;l++)
	{
		usert=readin(lp+l);
		usert.ft=g[l][0];
		writeout(lp+l,usert);
	}
	usert=readin(1);                        //read in the config sector
	usert.fl=d;                                     //set the maint date to TODAY
	writeout(1,usert);                      //write it back to disk.
}

//have to rename it because delete is a reserved word..
void TWdelete(int16 p)
{
	int16 l;
	usert=readin(p);
	printf("Terminating %s %d...\n",usert.fa,p);
	removeship(p);
	usert=readin(p);
	memset(&usert,0x0,sizeof(usert));               //why not just 'erase' the user?
	usert.fm=0;                     //this tags the user as gone I thought..
	usert.fo=0;
	usert.fb=0;
	writeout(p,usert);
	for(l=lp+1;l!=ls;l++)
	{
		usert=readin(l);
		if(usert.fm==p)
		{
			usert.fm=-2;
			writeout(l,usert);
		}
	}
	pn=p;
	rsm();
	for(l=2;l!=lp;l++)              //remove created planets?
	{
		usert=readin(l);
		if(usert.fc==p)
		{
			usert.fc=-98;
			writeout(l,usert);
		}
	}
}

//read sent messages.
void rsm(void)
{
	smr x;
	int16 i;
	long filesize;
	int16 records;
	//FILE *smg;    use the global?
	char fullname[1024];

	#ifdef MICROSOFT
	sprintf(fullname,"%s\\twsmf.dat",datapath);
#else
	sprintf(fullname,"%s/twsmf.dat",datapath);
#endif

    smg=fopen(fullname,"r+b");	//read write MUST exist...
	if(smg==NULL)
		{
		//printf("rsm() error with twsmf.dat[%s]\n",fullname);
		if( (smg=fopen(fullname,"w"))!=NULL)		//create an empty file
			{fclose(smg);}                      //and close it.
		}
	else
		{
		fseek(smg,0,SEEK_END);
		filesize=ftell(smg);
		if(filesize==0)
		{
			//empty data file!
		}
		else
		{
		records=filesize/sizeof(smr);
		//printf("\n%d\t%d\n",sizeof(smr),filesize);
		//printf("\nThere are %d records in twsmf.dat\n",records);
		rewind(smg);
			for(i=0;i!=records;i++)
			{
				//printf("going to read record %d at offset %d\n",i,i*sizeof(smr));
				fseek(smg,i*sizeof(smr),SEEK_SET);
				fread(&x,sizeof(smr),1,smg);
				if(x.destin==pn)
				{
					fwrite(x.msg,1,strlen(x.msg),stdout);
					memset(&x,0x0,sizeof(x));
					x.destin=-1;
					fseek(smg,i*sizeof(smr),SEEK_SET);
					fwrite(&x,sizeof(smr),1,smg);
				}
			}

		}//file has data...
	fclose(smg);
	}
//      printf("write the rsm()\n");
}

//called by the init process....
void readmsg(void)
{
	printf("\nThe following happened to your ship since your last time on:\n");
	rsm();
}

void movecabal(int16 go,int16 a,int16 b)
{
	int16 n;
	int16 p;
	int16 v;
	int16 k;
	int16 l;
	if((a>=1) && (b>=1) && (a<=ls-lp) && (a!=b))
	{
		n=g[go][1];
		usert=readin(a+lp);
		if(usert.fm!=-1)
		{
			g[go][0]=0;
			g[go][1]=0;
		}
		else
		{
			if(usert.fl<=n)
			{
				n=usert.fl;
				g[go][1]=n;
				usert.fl=0;
				usert.fm=0;
				writeout(a+lp,usert);
			}
	else
		if(usert.fl>n)
		{
			usert.fl=usert.fl-n;
			writeout(a+lp,usert);
		}
		g[go][0]=b;
		usert=readin(b+lp);
		if(usert.fl==0)
		{
			usert.fl=n;
			usert.fm=-1;
			writeout(b+lp,usert);
		}
		else
		{
			p=usert.fm;
			if(p==-1)
			{
				usert.fl=usert.fl+n;
				writeout(b+lp,usert);
			}
			else
			{
				l=0;
				k=0;
				while( (l>=usert.fl) && (k>=g[go][1]) )
				{
					v=TWrandom(2);
					if(v==1)
						l=l+1;
					else
						k=k+1;
				}
				userr=readin(p);
				message(p,-1,l,0);                      //bad news for someone!
				if(l<usert.fl)
				{
					g[go][0]=0;
					g[go][1]=0;
					usert.fl=usert.fl-l;
					writeout(b+lp,usert);
					//sysoplog     Group '+cstr(go)+' --> Sector' +cstr(b)+'('+userr.fa+'):');
					//sysoplog(' lost '+cstr(k)+', dstrd '+cstr(l)+' (Rom ftrs lose battle)');
				}
				else
				{
					usert.fl=n-k;
					usert.fm=-1;
					writeout(b+lp,usert);
					n=n-k;
					g[go][1]=n;
					//sysoplog('      Group '+cstr(go)+' --> Sector'+cstr(b)+'('+userr.fa+'):');
		    //sysoplog(' lost'+cstr(k)+', dstrd'+cstr(l)+' (Player ftrs lose battle)');

				}
			}
		}
	}
}
}


//pick a sector.. used by the maint process for moving around aliens..
int16 picksec(void)
{
	int16 v;
	v=TWrandom(3);
	if(v!=1)
		v=TWrandom(99);               //these guys got... adventerious!
	else
	{
		v=TWrandom(6);                //think of it as rolling 1D6.
		if(v==1)
			v=80;
		if(v==2)
			v=81;
		if(v==3)
			v=84;
		if(v==4)
			v=82;
		if(v==5)
			v=71;
		if(v==6)
			v=86;
	}
	return(v);
}

void cattack(int16 go,int16 p,int16 f)
{
	int16 r;
	int16 k;        //fighters lost
	int16 c13;
	int16 r13;      //romulans remaining...?
	int16 v;
	int16 n;
	int16 pn;
	char syslogtext[256];

	if(f>g[go][1])
		f=g[go][1];
	if( (p>=1) && (p<=lp) )
	{
		c13=g[go][0]+lp;
		usert=readin(c13);
		if ( (usert.fm==1) && (f>=1) )
		{
			usert=readin(p);
			if(usert.ff==c13-lp)
			{
				r=0;
				k=0;
				while( (r>usert.fg) || (k>=f) )
				{
					v=picksec();    //coin flip.....
					if(v==1)
						r=r+1;          //fighters killed
					else
						k=k+1;          //enemies killed
				}//end while
				g[go][1]=g[go][1]-k;
				usert=readin(c13);
				usert.fl=g[go][1];
				writeout(c13,usert);
				if( g[go][1]<1)
				{
					usert.fm=0;
					usert.fl=0;
					writeout(c13,usert);
					g[go][0]=0;
					g[go][1]=0;
				}//end if( g[go][1]<1)
				usert=readin(p);
				f=usert.fg-r;
				n=r;
				r13=r;
				pn=-1;
				message(p,pn,n,0);
				if(f>0)
				{
					usert=readin(p);
					usert.fg=f;
					writeout(p,usert);
				}
				else
					killed(pn,p);
				usert=readin(p);
				memset(syslogtext,0x0,sizeof(syslogtext));
				if(g[go][0]==0)
				{       //sysoplog(usert.fa+': lost '+cstr(k)+', dstrd '+cstr(r13)+' (Roms wiped out)')
					sprintf(syslogtext,"%s: lost %d, dstrd %d (Roms wiped out)\n",usert.fa,k,r13);
					sysoplog(syslogtext);
				}
				else
				{       //sysoplog(usert.fa+': lost '+cstr(k)+', dstrd '+cstr(r13)+' (Player destroyed)');
					sprintf(syslogtext,"%s: lost %d, dstrd %d (Player destroyed)\n",usert.fa,k,r13);
					sysoplog(syslogtext);
				}
			}//end if(usert.ff==c13-lp)
		}//end if ( (usert.fm==1) && (f>=1) )
	}//end if( (p>=1) && (p<=lp) )
}

