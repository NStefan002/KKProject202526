#include <stdio.h>

int main()
{
    int sum = 0;
    // should NOT be deleted, sum escapes
    for (int i = 0; i < 5; i++)
    {
        sum += i;
    }

    if (sum > 10)
    {
        printf("Sum: %d\n", sum);
    }
    else
    {
        printf("Sum <= 10\n");
    }

    // should be deleted, x is never used outside the loop
    for (int j = 0; j < 5; j++)
    {
        sum += j * 2;
    }
    return 0;
}
