#include "YMLParser.h"

#include <stdio.h>

static int ok;	// ok хранит последний код ошибки
static char error[1024]; // error buf хранит последнее сообщенее об ошибки

int YMLErrorPrint(){
	if(ok==0) return 0;
	fprintf(stderr, "YMLParser error code: %d; error massage: %s\n", ok, error);
	return ok;
}
