#include <stdio.h>

int main()
{
    // should NOT be deleted, call instruction may have side effects
    for (int i = 0; i < 5; i++)
    {
        printf("%d\n", i); // call instruction
    }
    return 0;
}
