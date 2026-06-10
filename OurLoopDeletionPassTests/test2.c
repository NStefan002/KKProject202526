int main()
{
    int sum = 0;
    // should be deleted, sum is never used outside the loop, and i is local to the loop
    for (int i = 0; i < 10; i++)
    {
        sum += i;
    }
    return 0;
}
