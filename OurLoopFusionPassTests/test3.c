int main()
{
    // Tri trivijalne petlje sa istim granicama (0..10).
    // Pass se pokrece nad svakom petljom: druga se spaja u prvu, a treca
    // u (vec spojenu) prvu -> na kraju sve tri postaju jedna petlja.
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

    int c = 0;
    for (int k = 0; k < 10; k++)
    {
        c += k;
    }

    return 0;
}
