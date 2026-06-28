#include <stdio.h>

int main()
{
    // should NOT be deleted, call instruction may have side effects
    for (int i = 0; i < 5; i++)
    {
        int x;
        scanf("%d", &x);
        printf("%d\n", x);
    }
    return 0;
}
