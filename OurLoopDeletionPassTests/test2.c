int main()
{
    // nested loops, both should be deleted
    // inner loop should be deleted first, then the outer loop
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            int x = i + j;
        }
    }
    return 0;
}
