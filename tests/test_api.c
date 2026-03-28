#include <stdio.h>
#include "test.h"
#include "YMLParser.h"

int main(void) {
    int ok = 0;
    char *error = NULL;
    YMLValue *root, *v;

    root = YMLParse("n: 42\ns: hello\narr: [1, 2, 3]\n", .ok=&ok);
    CHECK(ok==0 && root, "setup parse");

    /* ── YMLMapGet ────────────────────────────────────────────────── */
    SECTION("YMLMapGet found");
    v = YMLMapGet(root->value.object, "n", .ok=&ok);
    CHECK(ok==0 && v && v->value.integer==42, "found ok");

    SECTION("YMLMapGet type check ok");
    v = YMLMapGet(root->value.object, "n", .type=YML_INT, .ok=&ok);
    CHECK(ok==0 && v, "correct type");

    SECTION("YMLMapGet type mismatch → ok=2");
    v = YMLMapGet(root->value.object, "n", .type=YML_STRING, .ok=&ok, .error=&error);
    CHECK(ok==2, "ok=2 on mismatch");
    CHECK(v==NULL, "returns NULL");
    CHECK(error && error[0], "error message set");

    SECTION("YMLMapGet missing key → ok=1");
    v = YMLMapGet(root->value.object, "missing", .ok=&ok, .error=&error);
    CHECK(ok==1, "ok=1 on missing");
    CHECK(v==NULL, "returns NULL");

    SECTION("YMLMapGet default type = YML_ANY");
    v = YMLMapGet(root->value.object, "n");
    CHECK(v && v->value.integer==42, "no type check by default");

    /* ── YMLErrorPrint ────────────────────────────────────────────── */
    SECTION("YMLErrorPrint no error → 0");
    /* успешный вызов сбрасывает g_ok в 0 */
    v = YMLMapGet(root->value.object, "n"); (void)v;
    CHECK(YMLErrorPrint()==0, "no error after successful call");

    SECTION("YMLErrorPrint after missing key → 1");
    YMLMapGet(root->value.object, "nonexistent"); /* без .ok — ставит глобальную ошибку */
    CHECK(YMLErrorPrint()==1, "global error code 1");

    /* ── YMLMapForech ─────────────────────────────────────────────── */
    SECTION("YMLMapForech iterates all keys");
    int count = 0;
    YMLMapForech(root->value.object, key, val) {
        (void)key; (void)val;
        count++;
    }
    CHECK(count == 3, "3 keys visited");

    SECTION("YMLMapForech key and value accessible");
    int found_n = 0;
    YMLMapForech(root->value.object, key, val) {
        if (key && val && strcmp(key, "n")==0 && val->value.integer==42)
            found_n = 1;
    }
    CHECK(found_n, "found n=42 in foreach");

    /* ── ArrayLen ─────────────────────────────────────────────────── */
    SECTION("ArrayLen");
    v = YMLMapGet(root->value.object, "arr", .ok=&ok);
    CHECK(ok==0 && v && v->type==YML_ARRAY, "arr is array");
    CHECK(ArrayLen(v->value.array) == 3, "len=3");
    CHECK(v->value.array[0].value.integer==1, "[0]=1");
    CHECK(v->value.array[1].value.integer==2, "[1]=2");
    CHECK(v->value.array[2].value.integer==3, "[2]=3");

    YMLDestroy(root);

    /* ── YMLParse error handling ──────────────────────────────────── */
    SECTION("YMLDestroy null safe");
    YMLDestroy(NULL); /* не должен упасть */
    CHECK(1, "no crash");

    SECTION("YMLDestroyStream null safe");
    YMLDestroyStream(NULL);
    CHECK(1, "no crash");

    /* ── YMLParseStream ───────────────────────────────────────────── */
    SECTION("YMLParseStream ArrayLen");
    YMLValue **docs = YMLParseStream("---\n1\n---\n2\n---\n3\n", .ok=&ok);
    CHECK(ok==0 && docs && ArrayLen(docs)==3, "3 documents");
    CHECK(docs[0]->value.integer==1, "doc[0]=1");
    CHECK(docs[1]->value.integer==2, "doc[1]=2");
    CHECK(docs[2]->value.integer==3, "doc[2]=3");
    YMLDestroyStream(docs);

    TEST_REPORT();
    return _failed != 0;
}
