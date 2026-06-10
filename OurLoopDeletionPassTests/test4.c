#include <stdio.h>

int main()
{
    int sum = 0;
    // should NOT be deleted, variable altered in the loop is used after the loop
    for (int i = 0; i < 5; i++)
    {
        sum += i;
    }
    printf("%d\n", sum); // sum escapes
    return 0;
}
