/*
    recursion.c.tst - Unit tests for JSON template expansion and recursion prevention

    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonTemplateExpansionTest()
{
    Json    *json;
    char    *result;

    // Test self-referencing template (should prevent recursion by leaving ${loop} unexpanded)
    json = parse("{\"loop\": \"${loop}\"}");
    ttrue(json != 0);

    result = jsonToString(json, 0, 0, JSON_EXPAND);
    ttrue(result != 0);
    // Self-reference should remain unexpanded to prevent infinite recursion
    ttrue(scontains(result, "${loop}"));
    rFree(result);
    jsonFree(json);

    // Test that single-level template expansion works correctly
    json = parse("{\
        \"name\": \"value\",\
        \"template\": \"The name is ${name}\"\
    }");
    ttrue(json != 0);

    result = jsonToString(json, 0, 0, JSON_EXPAND);
    ttrue(result != 0);
    // Single-level expansion should work
    ttrue(scontains(result, "The name is value"));
    rFree(result);
    jsonFree(json);

    // Test chained template references (should only expand one level)
    json = parse("{\
        \"level1\": \"${level2}\",\
        \"level2\": \"final value\",\
        \"test\": \"${level1}\"\
    }");
    ttrue(json != 0);

    result = jsonToString(json, 0, 0, JSON_EXPAND);
    ttrue(result != 0);
    // Only single-level expansion: ${level1} becomes ${level2}, not "final value"
    ttrue(scontains(result, "${level2}"));
    ttrue(scontains(result, "final value"));
    rFree(result);
    jsonFree(json);

    // Test template expansion in nested objects
    json = parse("{\
        \"config\": {\
            \"host\": \"localhost\",\
            \"port\": \"8080\"\
        },\
        \"url\": \"http://${config.host}:${config.port}/api\"\
    }");
    ttrue(json != 0);

    result = jsonToString(json, 0, 0, JSON_EXPAND);
    ttrue(result != 0);
    // Nested property access should work for single-level expansion
    ttrue(scontains(result, "http://localhost:8080/api"));
    rFree(result);
    jsonFree(json);
}

static void jsonEdgeCaseTest()
{
    Json    *json;
    char    *result;

    // Test multiple variables in one template
    json = parse("{\
        \"first\": \"John\",\
        \"last\": \"Doe\",\
        \"greeting\": \"Hello ${first} ${last}!\"\
    }");
    ttrue(json != 0);

    result = jsonToString(json, 0, 0, JSON_EXPAND);
    ttrue(result != 0);
    ttrue(scontains(result, "Hello John Doe!"));
    rFree(result);
    jsonFree(json);

    // Test undefined variable (expansion fails, original text skipped)
    json = parse("{\
        \"message\": \"Hello ${undefined_var}\"\
    }");
    ttrue(json != 0);

    result = jsonToString(json, 0, 0, JSON_EXPAND);
    ttrue(result != 0);
    // Undefined variables cause expansion to fail and are skipped
    ttrue(scontains(result, "Hello "));
    ttrue(!scontains(result, "${undefined_var}"));
    rFree(result);
    jsonFree(json);

    // Test empty template (should be skipped as malformed)
    json = parse("{\
        \"empty\": \"prefix${}suffix\"\
    }");
    ttrue(json != 0);

    result = jsonToString(json, 0, 0, JSON_EXPAND);
    ttrue(result != 0);
    // Empty template should be skipped/removed from output
    ttrue(scontains(result, "prefixsuffix"));
    rFree(result);
    jsonFree(json);
}

int main(void)
{
    rInit(0, 0);
    jsonTemplateExpansionTest();
    jsonEdgeCaseTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
*/