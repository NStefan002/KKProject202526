int main()
{
    // nested loops, both should be deleted
    // inner loop should be deleted first, then the outer loop
    for (int i = 0; i < 5; i++)
    {
        int x = 0;
        for (int j = 0; j < 10; j++)
        {
            x += j;
        }
    }
    return 0;
}
