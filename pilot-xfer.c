/* pilot-xfer.c:  Pilot Database transfer utility
 *
 * (c) 1996, Kenneth Albanowski.
 *
 * This is free software, licensed under the GNU Public License V2.
 * See the file COPYING for details.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <signal.h>
#include "dlp.h"
#include "pi-socket.h"
#include "pi-file.h"

#define pi_mktag(c1,c2,c3,c4) (((c1)<<24)|((c2)<<16)|((c3)<<8)|(c4))

int sd = 0;
char * device;
char * progname;

RETSIGTYPE SigHandler(int signal);

void Connect(void) {
  struct pi_sockaddr addr;
  int ret;

  if (sd!=0)
    return;
    
  signal(SIGHUP, SigHandler);
  signal(SIGINT, SigHandler);
  signal(SIGSEGV, SigHandler);
      
  if (!(sd = pi_socket(AF_SLP, SOCK_STREAM, PF_PADP))) {
    perror("pi_socket");
    exit(1);
  }

  addr.sa_family = AF_SLP;
  addr.port = 3;
  strcpy(addr.device,device);
  
  ret = pi_bind(sd, &addr, sizeof(addr));
  if(ret == -1) {
    perror("pi_bind");
    exit(1);
  }
    
  printf("Waiting for connection (press the HotSync button now)...\n");

  ret = pi_listen(sd,1);
  if(ret == -1) {
    perror("pi_listen");
    exit(1);
  }

  sd = pi_accept(sd,0,0);
  if(sd == -1) {
    perror("pi_accept");
    exit(1);
  }

  puts("Connected");
}

void Disconnect(void)
{
  if(sd==0)
    return;
    
  dlp_EndOfSync(sd, 0);
  pi_close(sd);
  sd = 0;
}

RETSIGTYPE SigHandler(int signal)
{
  puts("Abort on signal!");
  Disconnect();
  exit(3);
}

void VoidSyncFlags(void)
{
  struct PilotUser U;
  Connect();
  if (dlp_ReadUserInfo(sd, &U)>=0) {
    U.lastSyncPC = 0xDEADBEEF; /* Hopefully unique constant, to tell
                                  any Desktop software that databases
                                  have been altered, and that a slow
                                  sync is necessary */
    dlp_WriteUserInfo(sd, &U);
  } 
}

void Backup(char * dirname)
{
  int i;
  
  Connect();
  
  mkdir(dirname,0700);

  i = 0;
  while(1) {
  	struct DBInfo info;
  	struct pi_file * f;
  	char name[256];

        if (dlp_OpenConduit(sd)<0) {
	  puts("Exiting on cancel, all data _not_ backed up.");
          exit(1);
        }
        
  	if( dlp_ReadDBList(sd, 0, 0x80, i, &info) < 0)
  		break;
  	i = info.index + 1;

  	sprintf(name, "%s/%s", dirname, info.name);
  	if (info.flags & dlpDBFlagResource)
  	  strcat(name,".prc");
  	else
  	  strcat(name,".pdb");
  	  
  	printf("Backing up '%s'... ", name);
  	fflush(stdout);
  	
  	f = pi_file_create(name, &info);
  	if (f==0) {
  	  printf("Failed, unable to create file\n");
  	  break;
  	}
  	
  	if(pi_file_retrieve(f, sd, 0)<0)
  	  printf("Failed, unable to back up database\n");
  	else
  	  printf("OK\n");
  	pi_file_close(f);
  }
  printf("Backup done.\n");
}

void Fetch(char * dbname)
{
  struct DBInfo info;
  char name[256];
    	struct pi_file * f;
    	
  Connect();

  if (dlp_OpenConduit(sd)<0) {
    puts("Exiting on cancel, all data _not_ backed up.");
    exit(1);
  }
  	
  dlp_FindDBInfo(sd, 0, 0, dbname, 0, 0, &info);
        
  strcpy(name, dbname);
  if (info.flags & dlpDBFlagResource)
    strcat(name,".prc");
  else
    strcat(name,".pdb");
  	  
  printf("Fetching '%s'... ", name);
  fflush(stdout);
  	
  f = pi_file_create(name, &info);
  if (f==0) {
    printf("Failed, unable to create file\n");
    return;
  }
  	
  if(pi_file_retrieve(f, sd, 0)<0)
    printf("Failed, unable to back up database\n");
  else
    printf("OK\n");
  pi_file_close(f);

  printf("Fetch done.\n");
}

struct db {
  	char name[256];
  	int flags;
  	unsigned long creator;
  	unsigned long type;
  	int maxblock;
};

int compare(struct db * d1, struct db * d2)
{
  /* types of 'appl' sort later then other types */
  if(d1->creator == d2->creator)
    if(d1->type != d2->type) {
      if(d1->type == pi_mktag('a','p','p','l'))
        return 1;
      if(d2->type == pi_mktag('a','p','p','l'))
        return -1;
    }
  return d1->maxblock < d2->maxblock;
}

void Restore(char * dirname)
{
  DIR * dir;
  struct dirent * dirent;
  struct DBInfo info;
  struct db * db[256];
  int dbcount = 0;
  int i,j,max,size;
  struct pi_file * f;

  dir = opendir(dirname);
  
  while( (dirent = readdir(dir)) ) {
        char name[256];
        
        if (dirent->d_name[0] == '.')
           continue;

	
	db[dbcount] = malloc(sizeof(struct db));
	
	sprintf(db[dbcount]->name, "%s/%s", dirname, dirent->d_name);
	
	f = pi_file_open(db[dbcount]->name);
  	if (f==0) {
  	  printf("Unable to open '%s'!\n", name);
  	  break;
  	}
  	
  	pi_file_get_info(f, &info);
  	
  	db[dbcount]->creator = info.creator;
  	db[dbcount]->type = info.type;
  	db[dbcount]->flags = info.flags;
  	db[dbcount]->maxblock = 0;
  	
  	pi_file_get_entries(f, &max);
  	
  	for (i=0;i<max;i++) {
  	  if (info.flags & dlpDBFlagResource)
  	    pi_file_read_resource(f, i, 0, &size, 0, 0);
  	  else
  	    pi_file_read_record(f, i, 0, &size, 0, 0,0 );
  	    
  	  if (size > db[dbcount]->maxblock)
  	    db[dbcount]->maxblock = size;
  	}
  	
  	pi_file_close(f);
  	dbcount++;
  }

  closedir(dir);

#ifdef DEBUG      
  printf("Unsorted:\n");
  for (i=0;i<dbcount;i++) {
     printf("%d: %s\n", i, db[i]->name);
     printf("  maxblock: %d\n", db[i]->maxblock);
     printf("  creator: '%s'\n", printlong(db[i]->creator));
     printf("  type: '%s'\n", printlong(db[i]->type));
  }
#endif
  
  for (i=0;i<dbcount;i++)
    for (j=i+1;j<dbcount;j++) 
      if (compare(db[i],db[j])>0) {
        struct db * temp = db[i];
        db[i] = db[j];
        db[j] = temp;
      }

#ifdef DEBUG      
  printf("Sorted:\n");
  for (i=0;i<dbcount;i++) {
     printf("%d: %s\n", i, db[i]->name);
     printf("  maxblock: %d\n", db[i]->maxblock);
     printf("  creator: '%s'\n", printlong(db[i]->creator));
     printf("  type: '%s'\n", printlong(db[i]->type));
  }
#endif

  Connect();
  
  for (i=0;i<dbcount;i++) {

	if ( dlp_OpenConduit(sd) < 0) {
	   puts("Exiting on cancel. All data not restored.");
	   exit(1);
	}

  	f = pi_file_open(db[i]->name);
  	if (f==0) {
  	  printf("Unable to open '%s'!\n", db[i]->name);
  	  break;
  	}
  	printf("Restoring %s... ", db[i]->name);
  	fflush(stdout);
  	if(pi_file_install(f, sd, 0)<0)
  	  printf("failed.\n");
  	else
  	  printf("OK\n");
  	pi_file_close(f);
  }
  
  VoidSyncFlags();

  printf("Restore done\n");
}

void Install(char * filename)
{
  struct pi_file * f;
  
  Connect();

  if ( dlp_OpenConduit(sd) < 0) {
    puts("Exiting on cancel. All data not restored.");
    exit(1);
  }

  f = pi_file_open(filename);
  if (f==0) {
    printf("Unable to open '%s'!\n", filename);
    return;
  }
  printf("Installing %s... ", filename);
  fflush(stdout);
  if(pi_file_install(f, sd, 0)<0)
    printf("failed.\n");
  else
    printf("OK\n");
  pi_file_close(f);
  
  VoidSyncFlags();

  printf("Install done\n");
}

void List(void)
{
  struct DBInfo info;
  int i;
  
  Connect();

  if ( dlp_OpenConduit(sd) < 0) {
    puts("Exiting on cancel. All data not restored.");
    exit(1);
  }
  
  printf("Reading list of databases...\n");
  
  i = 0;
  while(1) {
  	if( dlp_ReadDBList(sd, 0, 0x80, i, &info) < 0)
  		break;
  	i = info.index + 1;
  	
  	printf("'%s'\n", info.name);
  }
  
  printf("List done.\n");
}

void Help(void)
{
      printf("Usage: %s %s command(s)\n\n",progname,TTYPrompt);
      printf("Where a command is one of: -b(ackup)  backupdir\n");
      printf("                           -r(estore) backupdir\n");
      printf("                           -i(nstall) filename \n");
      printf("                           -f(etch)   dbname   \n");
      printf("                           -l(ist)\n");
      exit(0);
}

int main(int argc, char *argv[])
{
  int c;
#ifdef sun
  extern char* optarg;
  extern int optind;
#endif


  progname = argv[0];

  if (argc < 3) {
    Help();
  }

  device = argv[1];
  
  optind = 2;
  while ((c = getopt(argc, argv, "b:r:i:f:lh")) != -1) {
    switch (c) {
    case 'b':
      Backup(optarg);
      break;
    case 'r':
      Restore(optarg);
      break;
    case 'i':
      Install(optarg);
      break;
    case 'f':
      Fetch(optarg);
      break;
    case 'l':
      List();
      break;
    default:
    case 'h': case '?':
      Help();
    }
  }
  
  Disconnect();
  
  exit(0);
}

