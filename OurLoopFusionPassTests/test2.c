int main()
{
    // Prva petlja je trivijalna, druga u telu ima if/else grananje.
    // Telo druge petlje (zajedno sa granjanjem) treba da bude ubaceno u prvu.
    int a = 0;
    for (int i = 0; i < 10; i++)
    {
        a += i;
    }

    int b = 0;
    for (int j = 0; j < 10; j++)
    {
        if (j % 2 == 0)
        {
            b += j;
        }
        else
        {
            b -= j;
        }
    }

    return 0;
}
