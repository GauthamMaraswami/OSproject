/* operating systen running on bochs done by Gautham M
in may- june 2016 as a part ACM nitk chapter summer mentorship programme*/

#define true 1
#define PLAINFILE 0x01
#define SUBDIR    0x02

void main2();
void main() { main2(); }

struct procTable
{
    int isActive;
    int sp;
} kproctable[7];
int CurrentProcess;

char curPath[7][7];
int dirSect;
void printString(char* str);
void readString(char buffer[]);
void readSector(char* buffer, int sector);
void writeSector(char* buffer, int sector);
int myDIV(int dividend, int divisor);
int myMOD(int dividend, int divisor);
void directory();
void changeDir(char* pathName);
void getCurDir();
void deleteFile(char* filename);
void readFile(char* filename, char* outbuf);
void writeFile(char* filename, char* inbuf);
void handleInterrupt21(int AX, int BX, int CX, int DX);
void executeProgram(char* name);
void terminate();
void killProcess(int procID);
void handleTimerInterrupt(int segment, int sp);

void createSubDir(char* dirname);
void changeParentDir();
void getCurProcID();

// interrupt() usage:
// int interrupt (int number, int AX, int BX, int CX, int DX)

void main2()
{
    int i, j;
    CurrentProcess = 0;
    for(i=0; i<7; i++)
    {
        kproctable[i].isActive = 0;
        kproctable[i].sp = 0xFF00;
    }

    for(i=0; i<7; i++)
    {
        for(j=0; j<7; ++j)
        {
            if(j == 0)
                curPath[i][0] = '/';
            else
                curPath[i][j] = 0;
        }
    }
    // sector number of root directory
    dirSect = 2;

    for(i=0; i<0x6700; ++i)
        putInMemory(0x9000, i, 0);

    makeInterrupt21();
    makeTimerInterrupt();
    interrupt(0x21, 9, "shell", 0, 0);
    while(1);
}
//to print the entered string
void printString(char* str)
{
	int i=0;

	while(*(str+i) != '\0')
	{
		interrupt(0x10, 0xe*256+*(str+i), 0, 0, 0);
		i++;
	}
}
//to read the string into buffer
void readString(char buffer[])
{
	int j=0;
	while(true)
	{
		char current_read = interrupt(0x16, 0x0, 0, 0, 0);
		// if it's not backspace (0x8) or enter (0xd), save it
		if(current_read != 0xd && current_read != 0x8)
		{
			buffer[j] = current_read;
			j++;
			// display what entered on stdout (console)
			interrupt(0x10, 0xe*256+current_read, 0, 0, 0);
		}
		// if it is backspace (0x8)
		else if(current_read == 0x8)
		{
			// move one character backwards to drop the last enter char.
			j--;
			// backspace will not erase the deleted character on screen
			// use a blank to overwrite that position and backspace again.
			interrupt(0x10, 0xe*256+0x8, 0, 0, 0);
			interrupt(0x10, 0xe*256+' ', 0, 0, 0);
			interrupt(0x10, 0xe*256+0x8, 0, 0, 0);
		}
		else {
			// add CR to buffer end
			buffer[j] = 0xd;
			// add LF to buffer end
			buffer[j+1] = 0xa;
			buffer[j+2] = 0x0;
			break;
		}
	};
}

void readSector(char* buffer, int sector)
{
	int AX, CX, DX;

	AX = 2*256 + 1;
	CX = myDIV(sector, 36)*256 + ( myMOD(sector, 18) + 1 );
	DX = myMOD(myDIV(sector, 18), 2)*256 + 0;
	interrupt(0x13, AX, buffer, CX, DX);
}

void writeSector(char* buffer, int sector)
{
	int AX, CX, DX;

	AX = 3*256 + 1;
	CX = myDIV(sector, 36)*256 + ( myMOD(sector, 18) + 1 );
	DX = myMOD(myDIV(sector, 18), 2)*256 + 0;
	interrupt(0x13, AX, buffer, CX, DX);
}

int myDIV(int dividend, int divisor)
{
	int result =0;

	if(divisor == 0)
		/* can't divide 0, return error */
		return 1;

	/* circle minus */
	while( (dividend -= divisor) >= 0 )
		++result;

	return result;
}

int myMOD(int dividend, int divisor)
{
	int mod_result;
	// use DIV() to get MOD result
	mod_result = dividend - (divisor * myDIV(dividend, divisor));
	return mod_result;
}

void directory()
{
    int p_row, p_col;
    int sectNum;
    int looprange;
    char dirbuf[512];
    setKernelDataSegment();
    sectNum = dirSect;
    restoreDataSegment();
    readSector(dirbuf, sectNum);
    if(sectNum == 2) {
        looprange = 512;
    }
    else {
        looprange = 480;
    }
    // search the beginning of every 32 bytes
    for(p_row=0; p_row < looprange; p_row+=32)
    {
        if(dirbuf[p_row] != 0)
        {
            // read out the first 6 bytes of each file entry and print
            for(p_col=p_row; p_col<(p_row+6); p_col++)
                interrupt(0x10, 0xe*256 + dirbuf[p_col], 0, 0, 0);

            interrupt(0x10, 0xe*256+0xd, 0, 0, 0);
            interrupt(0x10, 0xe*256+0xa, 0, 0, 0);
        }
    }
}

void changeDir(char* pathName)
{
    int index, i, j;
    int sectindex;
    int dfIndicator;
    int curps;
    char tmpchar;
	char dirbuf[512];

    sectindex = 0;
    dfIndicator = 0;
    tmpchar = 0;

    setKernelDataSegment();
    curps = CurrentProcess;
    restoreDataSegment();

    if(pathName[0] == '/' && pathName[1] != 0) {
        readSector(dirbuf, 2);
    }
    else if(pathName[0] == '/' && pathName[1] == 0) {
        setKernelDataSegment();
        curPath[curps][0] = '/';
        restoreDataSegment();
        for(index=1; index<7; ++index)
        {
            setKernelDataSegment();
            curPath[curps][index] = 0;
            restoreDataSegment();
        }
        setKernelDataSegment();
        dirSect = 2;
        restoreDataSegment();
        return;
    }
    else {
        return;
    }

    for(i=0; i<512; i+=32)
    {
        // search filename in directory
        if(dirbuf[i] == pathName[1] && dirbuf[i+1] == pathName[2] && \
           dirbuf[i+2] == pathName[3] && dirbuf[i+3] == pathName[4] && \
           dirbuf[i+4] == pathName[5] && dirbuf[i+5] == pathName[6])
        {
            // find occupied sector
            for(j=i+6; j<(i+31); j++)
            {
                if(dirbuf[j] != 0)
                {
                    sectindex = dirbuf[j];
                    dfIndicator = dirbuf[i+31];
                    // subdir only occupies one sector
                    break;
                }
            }
        }
    }

    if(dfIndicator == 0x02)
    {
        for(index=0; index<7; ++index)
        {
            tmpchar = pathName[index];
            setKernelDataSegment();
            curPath[curps][index] = tmpchar;
            restoreDataSegment();
        }

        setKernelDataSegment();
        dirSect = sectindex;
        restoreDataSegment();
    }
    else
    {
        printString(pathName);
        setKernelDataSegment();
        printString(" is not a directory\n\r");
        restoreDataSegment();
    }
}
//for pwd command
void getCurDir()
{
    int curps;
    int i;
    char tmpCurPath[7];
    char tmpch;
    setKernelDataSegment();
    curps = CurrentProcess;
    restoreDataSegment();

    for(i=0; i<7; ++i)
    {
        setKernelDataSegment();
        tmpch = curPath[curps][i];
        restoreDataSegment();
        tmpCurPath[i] = tmpch;
    }

    printString(tmpCurPath);
}

void getCurProcID()
{
    char curps[2];
    setKernelDataSegment();
    curps[0] = CurrentProcess;
    restoreDataSegment();
    curps[0] = curps[0] + '0';
    curps[1] = 0;
    printString(curps);
}

// deleteFile supports multi-sector storage
void deleteFile(char* filename)
{
    char mapbuf[512], dirbuf[512];
    // use static to initialize with all zero
    static char zerobuf[512];
    int i, j, n;
    // load map and directory into buffer
    readSector(mapbuf, 1);
    readSector(dirbuf, 2);
    for(i=0; i<512; i+=32)
    {
        // search filename in directory
        if(dirbuf[i] == filename[0] && dirbuf[i+1] == filename[1] && \
           dirbuf[i+2] == filename[2] && dirbuf[i+3] == filename[3] && \
           dirbuf[i+4] == filename[4] && dirbuf[i+5] == filename[5])
        {
            // delete file name in entry in directory
            for(n=i; n<(i+6); n++)
            {
                dirbuf[n] = 0x00;
            }
            // find occupied sector
            for(j=i+6; j<(i+31); j++)
            {
                // erase corresponding sector
                if(dirbuf[j] != 0x00)
                {
                    writeSector(zerobuf, dirbuf[j]);
                    // update map in sector #1
                    mapbuf[dirbuf[j]] = 0x00;
                }
            }
            // clear directory/file flag
            dirbuf[i+32] = 0x00;
        }
    }
    // write back to update disk
    writeSector(mapbuf, 1);
    writeSector(dirbuf, 2);
}

// readFile supports multi-sector storage
void readFile(char* filename, char* outbuf)
{
    char dirbuf[512];
    int i, j;
    int sectindex;
    // load directory sector into buffer
    setKernelDataSegment();
    sectindex = dirSect;
    restoreDataSegment();
    readSector(dirbuf, sectindex);
    for(i=0; i<512; i+=32)
    {
        // search filename in directory
        if(dirbuf[i] == filename[0] && dirbuf[i+1] == filename[1] && \
           dirbuf[i+2] == filename[2] && dirbuf[i+3] == filename[3] && \
           dirbuf[i+4] == filename[4] && dirbuf[i+5] == filename[5])
        {
            // find occupied sector
            for(j=i+6; j<(i+31); j++)
            {
                if(dirbuf[j] != 0)
                {
                    readSector((outbuf+(j-i-6)*512), dirbuf[j]);
                }
            }
        }
    }
}

// writeFile supports multi-sector storage
void writeFile(char* filename, char* inbuf)
{
	char mapbuf[512], dirbuf[512];
	int i, j, n;
    int sector_index[25];
    int valid_sector;
	// load map and directory into buffer
	readSector(mapbuf, 1);
	readSector(dirbuf, 2);
    for(i=0; i<12800; i+=512)
    {
        if(inbuf[i] == 0x00)
            valid_sector = myDIV(i, 512);
    }
	// search for available sectors in map
    for(j=0; j<valid_sector; j++)
    {
        for(n=0; n<512; n++)
        {
            if(mapbuf[n] == 0x00)
            {
                mapbuf[n] = 0xFF;
                sector_index[j] = n;
                break;
            }
        }
    }
	// search for available file entry in directory
	for(i=0; i<512; i+=32)
	{
		if(dirbuf[i] == 0)
			break;
	}
	// write filename into directory
	for(j=i; j<(i+6); j++)
		dirbuf[j] = filename[j-i];

    // write occupied sector number to entry
    for(n=0; n < valid_sector; n++)
        dirbuf[j+n] = sector_index[n];

	writeSector(mapbuf, 1);
	writeSector(dirbuf, 2);
	// write data into corresponding sector
    for(i=0; i < valid_sector; i++)
        writeSector((inbuf+i*512), sector_index[i]);
}

void createSubDir(char* dirname)
{
	char mapbuf[512], dirbuf[512];
    char subdirbuf[512];
    char tmpchar;
	int i;
    int freedirentry;
    int availsect;
    int curdirsect;
    int curps;

	readSector(mapbuf, 1);

    // load directory sector into buffer
    setKernelDataSegment();
    curps = CurrentProcess;
    curdirsect = dirSect;
    restoreDataSegment();
    readSector(dirbuf, curdirsect);

    // search for available entry in dir sector
    for(i=0; i<512; i+=32)
    {
        if(dirbuf[i] == 0x00)
        {
            freedirentry = i;
            break;
        }
    }

    // copy folder name into dir sector, and mark as subdir.
    for(i=0; i<6; ++i)
    {
        dirbuf[freedirentry + i] = dirname[i];
    }
    dirbuf[freedirentry + 31] = SUBDIR;

    // search for available sector for this subdir
    for(i=0; i<512; ++i)
    {
        if(mapbuf[i] == 0x00)
        {
            availsect = i;
            mapbuf[i] = 0xFF;
            break;
        }
    }
    dirbuf[freedirentry + 6] = availsect;
    readSector(subdirbuf, availsect);
    // store the subdir's parent directory
    for(i=0; i<7; ++i)
    {
        setKernelDataSegment();
        tmpchar = curPath[curps][i];
        restoreDataSegment();
        subdirbuf[480+i] = tmpchar;
    }
    subdirbuf[510] = curdirsect;
	writeSector(subdirbuf, availsect);

	writeSector(mapbuf, 1);
	writeSector(dirbuf, curdirsect);
}

void changeParentDir()
{
    int curdirsect;
    char dirbuf[512];
    int i;
    char tmpchar;
    int curps;

    // load directory sector into buffer
    setKernelDataSegment();
    curps = CurrentProcess;
    curdirsect = dirSect;
    restoreDataSegment();
    readSector(dirbuf, curdirsect);
    curdirsect = dirbuf[510];

    setKernelDataSegment();
    dirSect = curdirsect;
    restoreDataSegment();
    for(i=0; i<7; ++i)
    {
        tmpchar = dirbuf[480+i];
        setKernelDataSegment();
        curPath[curps][i] = tmpchar;
        restoreDataSegment();
    }
}

void executeProgram(char* name)
{
    char filebuf[4096];
    int segment;
    int i, j;
    readFile(name, filebuf);
    setKernelDataSegment();
    for(i=0; i<7; i++)
    {
        if(kproctable[i].isActive == 0)
            break;
    }
    restoreDataSegment();
    segment = 0x1000 * (i+2);
    //CurrentProcess = i;
    for(j=0; j<4096; j++) {
        putInMemory(segment, j, filebuf[j]);
    }
    // initialize to segment other than kernel
    initializeProgram(segment);
    setKernelDataSegment();
    kproctable[i].sp = 0xFF00;
    kproctable[i].isActive = 1;
    restoreDataSegment();
}

void killProcess(int procID)
{
    setKernelDataSegment();
    kproctable[procID].isActive = 0;
    kproctable[procID].sp = 0xFF00;
    restoreDataSegment();
}

void terminate()
{
    setKernelDataSegment();
    kproctable[ CurrentProcess ].isActive = 0;
    kproctable[ CurrentProcess ].sp = 0xFF00;
    restoreDataSegment();
    while(1);
}

void handleTimerInterrupt(int segment, int sp)
{
    int i;
    setKernelDataSegment();
    // avoid passing kernel segment
    if(segment != 0x1000)
        kproctable[ CurrentProcess ].sp = sp;
    restoreDataSegment();

    // round robin
    setKernelDataSegment();
    for(i = CurrentProcess + 1; i < (CurrentProcess + 8); i++)
    {
        if(kproctable[myMOD(i,7)].isActive == 1)
        {
            CurrentProcess = myMOD(i,7);
            segment = 0x1000 * (CurrentProcess + 2);
            sp = kproctable[ CurrentProcess ].sp;
            break;
        }
    }
    restoreDataSegment();
    returnFromTimer(segment, sp);
}



void handleInterrupt21(int AX, int BX, int CX, int DX)
{
	switch(AX)
	{
		case 0:
			printString(BX);
			break;
		case 1:
			readString(BX);
			break;
		case 2:
			readSector(BX, CX);
			break;
		case 3:
			//printString("List directory:");
			directory();
			break;
		case 4:
		    deleteFile(BX);
			break;
		case 5:
            terminate();
			break;
		case 6:
			// BX is filename, CX is buffer pointer
			readFile(BX, CX);
			break;
		case 7:
			writeSector(BX, CX);
			break;
		case 8:
			writeFile(BX, CX);
			break;
		case 9:
			executeProgram(BX);
			break;
		case 10:
            // BX is int not char.
			killProcess(BX);
			break;
		case 11:
            //sendMessage(BX, CX);
			break;
		case 12:
            //getMessage(BX);
			break;
		case 13:
            changeDir(BX);
			break;
		case 14:
            getCurDir();
			break;
		case 15:
            createSubDir(BX);
			break;
		case 16:
            changeParentDir();
			break;
		case 17:
            getCurProcID();
			break;
		default:
			printString("Syscall not supported");
			break;
	}
}
