int main()
{
    int a = 0;
    for (int i = 0; i < 10; i++)
    {
        a += i;
    }

    int b = 0;
    for (int j = 0; j < 10; j++)
    {
        b += a;
    }

    return 0;
}
