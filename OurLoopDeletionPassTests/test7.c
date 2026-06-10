#include <stdio.h>

int g = 0;
int h = 0;

void f()
{
    printf("%d\n", g);
}

int main()
{
    // should NOT be deleted, loop alters the global variable that is used elsewhere
    for (int i = 0; i < 5; i++)
    {
        g += i;
    }

    // should be deleted, loop alters the global variable, but that variable is not used anywhere else
    for (int i = 0; i < 5; i++)
    {
        h++;
    }

    return 0;
}
