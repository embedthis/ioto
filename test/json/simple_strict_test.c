#include "test.h"

int main(void) {
    Json *obj;
    char *error;

    printf("Testing JSON_STRICT_PARSE flag...\n");

    // Test comment - should fail in strict mode
    obj = jsonParseString("/* comment */ {\"key\": \"value\"}", &error, JSON_STRICT_PARSE);
    if (obj == 0) {
        printf("PASS: Comment rejected in strict mode: %s\n", error);
        rFree(error);
    } else {
        printf("FAIL: Comment accepted in strict mode\n");
        jsonFree(obj);
    }

    // Test single quotes - should fail in strict mode
    obj = jsonParseString("{'key': 'value'}", &error, JSON_STRICT_PARSE);
    if (obj == 0) {
        printf("PASS: Single quotes rejected in strict mode: %s\n", error);
        rFree(error);
    } else {
        printf("FAIL: Single quotes accepted in strict mode\n");
        jsonFree(obj);
    }

    // Test unquoted key - should fail in strict mode
    obj = jsonParseString("{key: \"value\"}", &error, JSON_STRICT_PARSE);
    if (obj == 0) {
        printf("PASS: Unquoted key rejected in strict mode: %s\n", error);
        rFree(error);
    } else {
        printf("FAIL: Unquoted key accepted in strict mode\n");
        jsonFree(obj);
    }

    // Test valid JSON - should pass
    obj = jsonParseString("{\"key\": \"value\"}", &error, JSON_STRICT_PARSE);
    if (obj != 0) {
        printf("PASS: Valid JSON accepted in strict mode\n");
        jsonFree(obj);
    } else {
        printf("FAIL: Valid JSON rejected in strict mode: %s\n", error);
        rFree(error);
    }

    return 0;
}