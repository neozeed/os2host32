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
extern char fs[];



//Integers
extern int16 usernum;  //The user number in the dat files passed in from the bbs
extern int16 ay;
extern int16 tt;
extern int16 lp;               //planet offset into the database
extern int16 ls;               //sector offset into the database
extern int16 lt1;
extern int16 ll1;
extern int16 d; //This holds the date NUMBER as a global...
extern int16 y;
extern int16 a;
extern int16 mo;
extern int16 go;
extern int16 pn;       //player number?
extern int16 pd;       //this gets assigned the value of d.. for the date again?
extern int16 s2;
extern int16 st;
extern int16 g2;
extern int16 prr;
extern int16 e[6];     //not sure what this array is for...
extern int16 b[];
extern int16 f2;
extern int16 e2;
extern int16 r1;       //this get set to a random number @ some point.
extern int16 l2;       //from the planet functions... should be global.
extern int16 s[201][2];        //used to calculate shortest paths... I *THINK* the 200 comes hardcoded here for the # of sectors...!
extern int16 g[9][1];          //something to do with romulans?

//Thse trading vars should be ... localized.
extern int16 m2;       //part of trading.. holds empty cargo holds.
extern int16 nuh;      //another part of trading.
extern int16 r;        //another part of trading....
extern int16 p2;       //for the idunno trading.
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
extern char *p[];
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

