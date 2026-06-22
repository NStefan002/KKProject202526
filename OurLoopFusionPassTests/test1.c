int main()
{
    // Dve trivijalne petlje, svaka radi sa po jednom promenljivom.
    // Iste granice (0..10) i isti korak -> petlje treba da se spoje u jednu.
    int a = 0;
    for (int i = 0; i < 10; i++)
    {
        a += i;
    }

    int b = 0;
    for (int j = 0; j < 10; j++)
    {
        b += j;
    }

    return 0;
}
