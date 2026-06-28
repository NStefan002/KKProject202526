int main()
{
    // should be deleted, since sum is never used outside the loop
    int sum = 0;
    for (int i = 0; i < 5; i++)
    {
        sum += i;
    }
    return 0;
}
