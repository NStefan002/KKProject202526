int main()
{
    // should be deleted, since x is never used outside the loop
    for (int i = 0; i < 5; i++)
    {
        int x = i * 2;
    }
    return 0;
}
