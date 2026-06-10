#include <stdio.h>

int main()
{
    int i;
    // should NOT be deleted, i escapes (even though it is the loop counter)
    for (i = 0; i < 5; i++)
    {
    }
    printf("%d\n", i);
    return 0;
}
