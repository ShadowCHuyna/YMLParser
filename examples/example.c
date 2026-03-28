#include <stdio.h>
#include <assert.h>

#include "YMLParser.h"

// мне лень писать что то большее но нужно протестить весь синтаксис yml
static char* yml = "test_val: 10";

int main(){
	int ok;
	char* error;
	YMLValue* root = YMLParse(yml, .ok=&ok, .error=&error);
	if (ok!=0){
		printf("error code: %d; error msg: \"%s\"\n", ok, error);
		exit(ok);
	}

	YMLValue* test_val = YMLMapGet(root->value.object, "test_val", .type=YML_INT, .ok=&ok, .error=&error);
	if (ok!=0){
		printf("error code: %d; error msg: \"%s\"\n", ok, error);
		exit(ok);
	}
	printf("test_val: %lld\n", (long long)test_val->value.integer);

	YMLMapForech(root->value.object, key, test_val){
		printf("key: %s\n", key);
	}

	test_val = YMLMapGet(root->value.object, "test_val2");
	assert(YMLErrorPrint()==0);
	

	return 0;
}
