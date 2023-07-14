#ifndef PERLIN_H
#define PERLIN_H

class Perlin
{
public:
    Perlin() {}
    ~Perlin() {}

    static double perlin(double x, double y, double z);
    static double octavePerlin(double x, double y, double z, int octaves, double persistence);

private:
};

#endif // PERLIN_H
