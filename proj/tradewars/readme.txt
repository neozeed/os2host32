Welcome to this release I tentatively call 0.7c?

So what do we have here?.

Basically it?s a C port of the Tradewars pascal program.  I didn?t opt for a machine translation, instead doing this by MYSELF.

Right now I thought it?d be best to release *SOMETHING* to get things going.



I?ve tried to keep as much as the code the same, including some of the weird pascalisims.  I?ve also tried to keep the amount of dependant C code to a minimum.  Right now the only ?quirky? need is getch()? However I suppose you should be able to find something on whatever you are using to replace it.. Or just use scanf and fake it?

This version is NOT multiuser by a long stretch as it doesnt do record locking, checking for player 'updates'? 

I?m using the following compilers for the releases:
Visual Studio 2008 to build the Win64 exes.
Visual C++ 1.0 (32bit eddition) for the  Win32 exes.
QuickC for Windows for both Win16 & MS-DOS exes.
Microsoft C 6.0 for the OS/2 16 bit exe
GCC 4.3.2 (Debian 4.3.2-1.1) for a static i386 linux binary.

there is something I?m doing wrong with binary float data as neither one read each others floats correctly?.. So win16/win32/win64 would each need their own data files (right now).

= INSTALLATION =
The easiest thing to do is unpack a binary distribution into something like C:\twc
First run condat to build the data file.

The next step is to run the maintenence program.  Right now it's in the tw4.exe
And you run it like this:

"tw4 player1.txt maint"

This will reap dead players, and move the romulans around... I need to write in the compact message file portion but I just haven't bothered yet..

Finally to play run "tw4 player1.txt" for a quick start...

First you?ll need to look at the file player1.txt  This is what it expects to be fed.  Tradewars was a BBS door, so it was handed files like this to tell it name, nick, and last time on etc? For now I?m just faking it.  The IMPORTANT part is the paths.. You?ll need to point those to where the tradwar files are.  Keep it simple; don?t expect paths with spaces to work.


= BUILDING =
Compile & build condat.c & run it.  This will build your twdata.dat file.  

Edit the top of tw2 to reflect what platform you are on, and if you want all the debug information...
Next compile & build the tw2 folder's files and run it?

This game certainly has a lot of potential, and after finding the pascal source lying around on some BBS cd archive, I figured now that Turbo Pascal 5.5 is freeware I?d take a look at it? And then port it to C!