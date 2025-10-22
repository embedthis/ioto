/*
    template.c.tst - Unit tests for jsonTemplate

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void testTemplate()
{
    Json    *obj;
    char    *text;

    obj = jsonParse("{ color: 'red', weather: 'sunny'}", 0);

    // Basic
    text = jsonTemplate(obj, "Color is ${color}", 0);
    tmatch(text, "Color is red");
    rFree(text);

    text = jsonTemplate(obj, NULL, 0);
    tmatch(text, "");
    rFree(text);

    text = jsonTemplate(obj, "", 0);
    tmatch(text, "");
    rFree(text);

    text = jsonTemplate(NULL, "Hello World", 0);
    tmatch(text, "Hello World");
    rFree(text);

    // Resolved 
    text = jsonTemplate(obj, "${color}", 0);
    tmatch(text, "red");
    rFree(text);

    // UnResolved and !keep
    text = jsonTemplate(obj, "${unknown}", 0);
    tmatch(text, "");
    rFree(text);

    // UnResolved and KEEP
    text = jsonTemplate(obj, "${unknown}", 1);
    tmatch(text, "${unknown}");
    rFree(text);

    // Var at the end
    text = jsonTemplate(obj, "Hello ${color}", 0);
    tmatch(text, "Hello red");
    rFree(text);

    // Without braces -- not expanded
    text = jsonTemplate(obj, "Hello $color", 0);
    tmatch(text, "Hello $color");
    rFree(text);

    // Multiple
    text = jsonTemplate(obj, "Hello ${color} ${weather}", 0);
    tmatch(text, "Hello red sunny");
    rFree(text);

    // Unterminated token
    text = jsonTemplate(obj, "Hello ${color ", 0);
    ttrue(text == NULL);
    rFree(text);

    jsonFree(obj);

    obj = jsonParse("{ name: 'John', age: 30, address: { city: 'New York' }, registered: true }", 0);

    // Token at the beginning
    text = jsonTemplate(obj, "${name} is ${age}", 0);
    tmatch(text, "John is 30");
    rFree(text);

    // Consecutive tokens
    text = jsonTemplate(obj, "${name}${age}", 0);
    tmatch(text, "John30");
    rFree(text);

    // Mix of resolved and unresolved
    text = jsonTemplate(obj, "Name: ${name}, City: ${address.city}, Country: ${address.country}", 0);
    tmatch(text, "Name: John, City: New York, Country: ");
    rFree(text);

    // Mix of resolved and unresolved with keep=1
    text = jsonTemplate(obj, "Name: ${name}, City: ${address.city}, Country: ${address.country}", 1);
    tmatch(text, "Name: John, City: New York, Country: ${address.country}");
    rFree(text);
    
    // Empty token
    text = jsonTemplate(obj, "Empty: ${}", 0);
    tmatch(text, "Empty: ");
    rFree(text);

    // Token with spaces - not found
    text = jsonTemplate(obj, "Name: ${ name }", 0);
    tmatch(text, "Name: ");
    rFree(text);

    // Access nested property
    text = jsonTemplate(obj, "City: ${address.city}", 0);
    tmatch(text, "City: New York");
    rFree(text);

    // Boolean value
    text = jsonTemplate(obj, "Registered: ${registered}", 0);
    tmatch(text, "Registered: true");
    rFree(text);

    // Unresolved at end with keep
    text = jsonTemplate(obj, "Hello ${name} from ${unknown}", 1);
    tmatch(text, "Hello John from ${unknown}");
    rFree(text);

    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    testTemplate();
    rTerm();
    return 0;
}
