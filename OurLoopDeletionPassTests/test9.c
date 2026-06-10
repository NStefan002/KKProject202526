int main()
{
    // should be deleted, loop has empty body and loop variable does not escape
    for (int i = 0; i < 1000; i++)
    {
    }
    return 0;
}
