#include "arg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void 
ArgDesc::procargs(int argc, char** argv, ArgDesc* desc) 
{
  ArgDesc::progname = argv[0];
  argv++; argc--;
  printf("Processing args for %s: %d\n", progname, argc);
  int maxarg = argc;
  int i;
  for (i=0; i<argc; i++) {
    char* arg = argv[i];
    if (ArgDesc::verbose) printf("%d -> [%s]\n", i, arg);
    if (arg[0] == '-') {
      bool ok = false;
      for (int j=0; desc[j].kind != ArgDesc::END; j++) {
	if (desc[j].kind != ArgDesc::Flag) continue;
	if (strcmp(desc[j].key, arg) == 0) {
	  // process it
	  ok = true;
	  switch (desc[j].type) {
	  case ArgDesc::Pointer:
	    *((void**)desc[j].dest.asptr) = argv[i+1];
	    i++;
	    break;

	  case ArgDesc::Flag:
	    *(desc[j].dest.asint) = 1;
	    break;

	  case ArgDesc::Int:
	    if (ArgDesc::verbose) printf("int -> [%s] = %d\n", argv[i+1], atoi(argv[i+1]));
	    *(desc[j].dest.asint) = atoi(argv[i+1]);
	    i++;
	    break;

	  default:
	    printf("NIY: type\n");
	    exit(-1);
	  }
	}
      }
      if (!ok) {
	fprintf(stderr, "Do not understand the flag [%s]\n", arg);
	exit(-1);
      }
    } else {
      break;
    }
  }
  int base = i;
  for (int j=0; desc[j].kind != ArgDesc::END; j++) {
    if (ArgDesc::verbose) printf("j=%d kind=%d\n", j, desc[j].kind);
    if (desc[j].kind != ArgDesc::Position) continue;
    if (base+desc[j].position < argc) {
      switch (desc[j].type) {
      case ArgDesc::Pointer:
	*((void**)desc[j].dest.asptr) = argv[base+desc[j].position];
	break;

      case ArgDesc::Int:
	if (ArgDesc::verbose) printf("Assinging an int %d %d\n", base+i, j);
	*(desc[j].dest.asint) = atoi(argv[base+desc[j].position]);
	i++;
	break;

      case ArgDesc::Flag:
	printf("Flag in position??\n");
	exit(-1);
	break;

      default:
	printf("NIY: type\n");
	exit(-1);
      }
    } else if (desc[j].reqd == ArgDesc::Required) {
      fprintf(stderr, "No argument %d given\n", desc[j].position);
      exit(-1);
    }
  }
}

char* ArgDesc::progname = 0;
int ArgDesc::verbose = 0;

// Local Variables:
// indent-tabs-mode: nil
// c-basic-offset: 4
// End:

